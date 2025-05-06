// This file is part of fdaPDE, a C++ library for physics-informed
// spatial and functional data analysis.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef __FDAPDE_THREADPOOL_H__
#define __FDAPDE_THREADPOOL_H__
#include "header_check.h"

namespace fdapde{
    class Threadpool{
        using job = std::function<void()>;
        //usato per send_task_round (che è equivalente a usare coutn_task senza decremento)
        struct indx_worker{
            int indx_ = 0;
            void next(int n_worker){
                (indx_ == n_worker-1)? (indx_ = 0):(indx_++);
            };
        };
        public:
            class Worker{
                private:
                    int indx_; 
                    fdapde::Synchro_queue<job,fdapde::relax_nowait> sync_queue_;
                    std::thread t_;
                    int count_job_ = 0;
                    bool stop_ = false;
                    std::mutex m_;
                    std::condition_variable cv_; 
                    int n_worker_ = 0; //size() di workers_
                public:
                    friend class Threadpool; //TODO: classi friend quindi inutili tuti i getter ecc.. ripulire codice. lasciare magari wrap di alcune funzioni per chiarezza in uso in threadpool(es workers_[i]->notifica() al posto di workers_[i]->cv_.notify_one())
                    // costruttore con numero elementi di coda
                    //inizializza thread con funzione membro worker_loop di Threadpoool cosi che accessibili altre code senza passaggio di puntatore/reference a threadpool in worker che causava segmentation fault  
                    Worker(int n, void (Threadpool::*worker_loop)(int),Threadpool* T, int idx):indx_(idx),sync_queue_(n),t_(worker_loop,T,idx){};

                    //per poter bloccare il mutex di Worker m_ in threadpool
                    std::unique_lock<std::mutex> get_loc(){
                        std::unique_lock<std::mutex> loc(m_);
                        return loc;          
                    }
                    //per poter notificare da threadpool
                    void notifica(){
                        cv_.notify_one();
                    }
                    /*messo wait direttamente in worker_loop grazie a dichiarazione friend possibile accedere a cv_
                    void wait (Threadpool* T){
                        std::unique_lock<std::mutex> loc(m_); //OSS: empty() gia sincronizzato con push grazie a mtex dentro Synchro_queue, mutex in synchro_queue_count serve solo per avere cv che mand a dormire. get_count_job_all() invece non sincronizzato (diversi thread vedono diverso quindi magari ce stato push e count++ ma in thread che fa il check non lo vede) però pazienza meglio di niente 
                        cv_.wait(loc,[&](){return !sync_queue_.empty() || stop_ || T->get_count_job_all();});
                        //loc.unlock(); //superfluo out of scope loc si distrugge e fa unlock mutex
                    }
                    */
                    void set_stop(bool s){
                        stop_ = s;
                    }
                    void join_thread(){
                        t_.join();
                    }
                    
                    
                    //lettura non affidabile però è sufficente per dare una aprossimazione utile a implementare  steal e send_task 
                    int get_count_job() const{
                        return count_job_;
                    };
                    

                    //wrap di funzioni per pop e push. 
                    bool push_front(job fun){
                        if(sync_queue_.push_front(fun)){
                            count_job_ ++;
                            return true;
                        }
                        return false;
                    };
                    bool push_back(job fun){
                        //std::cout<<"incremento count in push_back, count: "<<count_job_<<std::endl;
                        if(sync_queue_.push_back(fun)){
                            count_job_ ++;
                            return true;
                        }
                        return false;
                    };
                    std::optional<job> pop_front(){
                        std::optional<job> j = sync_queue_.pop_front();
                        if(j){
                            count_job_ --;
                        }
                        return j;
                    };
                    std::optional<job> pop_back(){
                        std::optional<job> j = sync_queue_.pop_back();
                        if(j){
                            count_job_ --;
                        }
                        return j;
                    };

            };
        private://TODO: cambiare nome threadpoool_ in workers_
            std::vector<std::shared_ptr<Worker>> workers_; //vettore di putatori perche non movable e copiable synchro_queue per via di mutex
            std::vector<int> count_job_;
            int n_worker_;
            indx_worker indxw_; 
            //std::mutex m_threadpool_; //non serve per ora
            //bool active_ = true;
        public:
            friend class Worker; //poi togliere tuti get e sostituire con accesso diretto
            //n = size code, k = numero worker
            Threadpool(int n, int k):n_worker_(k){
                workers_.reserve(k);
                count_job_.reserve(k);
                for(int i=0; i<k; i++){
                    workers_.emplace_back(std::make_shared<Worker> (n,&Threadpool::worker_loop,this,i));
                    count_job_.push_back(0);
                }
            };
            ~Threadpool(){
                //std::unique_lock<std::mutex> loc_t(m_threadpool_);
                //active_= false;
                //facciamo terminare tutti worker_loop cosi che nessun worker acceda a worker distrutti o a threadpool (perche quando il resto del distruttore di threadpool verra chiamato, cioe finito il corpo di questo distruttore, tutti i worker avranno terminato worker_loop grazie a join())
                for(int j = 0; j<n_worker_; j++){
                    std::unique_lock<std::mutex> loc(workers_[j]->get_loc());
                    workers_[j]->set_stop(true); //in mutex perche se ce cv che dorme notifica e aggiornamnto stop devono essere sincronizzati
                    workers_[j]->notifica();
                }
                for(int j = 0; j<n_worker_; j++){
                    workers_[j]->join_thread();
                }
            }

            void worker_loop(int i){
                while(!workers_[i]->stop_){
                    std::unique_lock<std::mutex> loc(workers_[i]->m_); //OSS: empty() gia sincronizzato con push grazie a mtex dentro Synchro_queue, mutex in synchro_queue_count serve solo per avere cv che mand a dormire. get_count_job_all() invece non sincronizzato (diversi thread vedono diverso quindi magari ce stato push e count++ ma in thread che fa il check non lo vede) però pazienza meglio di niente 
                    workers_[i]->cv_.wait(loc,[&](){return !workers_[i]->sync_queue_.empty() || workers_[i]->stop_ || get_count_all_job()>0;});
                    loc.unlock();
                    if(workers_[i]->stop_){return;}
                    if(!workers_[i]->sync_queue_.empty()){
                        std::optional<job> j = workers_[i]->pop_front();
                        if(j){//esegue se non è nullopt
                            count_job_[i]--;
                            (j.value())(); //esegue funzioni con 0 parametri e void. per non void si dovra fare wrap e associare a promise. per parametri lamda wrap che li cattura cosi no param  
                            std::cout<<"thread: "<<std::this_thread::get_id()<<" ha eseguito"<<std::endl;
                        }
                        //else{std::cout<<"thread: "<<std::this_thread::get_id()<<"nullopt"<<std::endl;}
                    }
                    else{ //steal
                        steal_from_most_busy_and_do();  
                    }                             
                }
            };
            void steal_from_most_busy_and_do(){
                int most_busy = indx_most_busy();
                if(most_busy != -1){
                    std::optional<job> j = workers_[most_busy]->pop_back();
                    if(j){
                        count_job_[most_busy]--;
                        (j.value())();
                        std::cout<<"thread: "<<std::this_thread::get_id()<<" ha eseguito"<<std::endl;
                    }
                    else{std::cout<<"thread: "<<std::this_thread::get_id()<<"nullopt"<<std::endl;}
                }
            }
            /* versione con contatore in worker
            int get_count_job_all(){
                std::unique_lock<std::mutex> loc(m_threadpool_);
                if(active_){
                    int count = 0;
                    for(int i=0; i<n_worker_; i++){
                        count += workers_[i]->get_count_job();
                    }
                    return count;
                }
                return 0;
            }
            */
            int get_count_all_job(){
                int somma = 0;
                for (auto x: count_job_)
                    somma += x;
                return somma;
            }
            void notifica_tutti(){
                for(int i=0; i<n_worker_; i++){
                    workers_[i]->notifica();
                }
            }
            //per acquisire tutti i mutex dei worker
            std::vector<std::unique_lock<std::mutex>> lock_tutti(){
                std::vector<std::unique_lock<std::mutex>> vett_lock;
                vett_lock.reserve(n_worker_);
                for(int i=0; i<n_worker_; i++){
                    std::unique_lock<std::mutex> loc_w(workers_[i]->get_loc());
                    vett_lock.push_back(std::move(loc_w));
                }
                return vett_lock;
            }
            //per rilasciare tutti i mutex dei worker
            void unlock_tutti(std::vector<std::unique_lock<std::mutex>>& vett_lock){
                for(int i=0; i<n_worker_; i++){
                    vett_lock[i].unlock();
                }
            }
            //ridà indice di worker piu libero (lettura di elmenti in sync_queue di worker fatta con metodo count_el non affidabile ma è abbastanza per avere un idea)
            /*versione con contatore in worker
            int indx_most_free(){
                int worker_indx = 0;
                int min_elem= workers_[0]->get_count_job(); //numero elementi in primo worker 
                for (int j=1; j<n_worker_; j++){
                    int current_el = workers_[j]->get_count_job();
                    if(current_el < min_elem ){ 
                        worker_indx = j;
                        min_elem = current_el;
                    }
                }
                return worker_indx;
            };
            */
            int indx_most_free(){
                int worker_indx = 0;
                int min_elem= count_job_[0]; //numero elementi in primo worker 
                for (int j=1; j<n_worker_; j++){
                    int current_el = count_job_[j];
                    if(current_el < min_elem ){ 
                        worker_indx = j;
                        min_elem = current_el;
                    }
                }
                return worker_indx;
            };

            //spostato in worker, ma tenuto anche qui magari poi sarà utile
            // indice di worker con piu job in coda, sara utile per steal job
            /*versione con contatore in worker
            int indx_most_busy(){
                std::unique_lock<std::mutex> loc(m_threadpool_);
                if(active_){
                    int worker_indx = 0;
                    int max_elem= workers_[0]->get_count_job(); //numero elementi in primo worker 
                    for (int j=1; j<n_worker_; j++){
                        int current_el = workers_[j]->get_count_job();
                        if(current_el > max_elem ){
                            worker_indx = j;
                            max_elem = current_el;
                        }
                    }
                    return worker_indx;
                }
                return -1;
            }
            */
            int indx_most_busy(){
                int worker_indx = 0;
                int max_elem= count_job_[0]; //numero elementi in primo worker 
                for (int j=1; j<n_worker_; j++){
                    int current_el = count_job_[j];
                    if(current_el > max_elem ){
                        worker_indx = j;
                        max_elem = current_el;
                    }
                }
                return worker_indx;
            }

            //send con indx_freer criterio
            /*lock solo di a chi manda e notifica a tutti (ma sara sincronizzata solo worker che riceve il push)
            bool send_task(job j){
                int indx_worker = indx_most_free();
                std::unique_lock<std::mutex> loc(workers_[indx_worker]->get_lock());
                bool flag = workers_[indx_worker]->push_back(j);
                notifica_tutti();
                loc.unlock();
                if(flag){
                    return true;
                }
                return false;
            };
            */
           //lock di tutti i mutex cosi notifica a tutti sara sincronizzata lettura di count++, OSSERVAZIONE: se un solo thread che send_job questa è migliore di altra versione che blocca solo un mutex. se piu thread forse meglio altra perche in questa invio job è sequenziale su tutti i worker
            //se F(Args) non void
            template<typename F, typename... Args>
            requires (!std::is_same_v<std::invoke_result_t<F, Args...>, void>)
            auto send_task(F&& f,Args... args) -> std::optional<std::future<decltype(f(args...))>>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = std::forward<F>(f), ...args_catturati = std::forward<Args>(args) ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};

                int indx_worker = indx_most_free();
                std::vector<std::unique_lock<std::mutex>> vett_locks(lock_tutti());
                bool flag = workers_[indx_worker]->push_back(j);
                notifica_tutti(); // problema: push e notifica sono sincronizati solo in thread su cui viene fatto push perche mutex che poi leggera è lo stesso bloccato in push.
                                //POSSIBILE SOLUZIONE: fare lock_all() e poi unlock_all() ma cosi ogni send blocca worker_loop di chi ancora non ha superato la cv_.wait(), pero sarebbe sincronizzata la lettura dei count_job ++. 
                unlock_tutti(std::ref(vett_locks));
                if(flag){
                    count_job_[indx_worker]++;
                    return fut;
                }
                return std::nullopt;  //OSSERVAZIONE:return optional e non future cosi possibilita di fallire per push e non è necessario fare while(). spostato check se push e quindi send a buon fine fuori da threadpool perche usando hold queue per esempio non puo fallire il push e quindi ci sarebbe un while inutile              
            };
            //se F(Args) void --> wrap in bool() cosi che sara possibile usare future.get() per aspettare che funione venga eseguita prima di mandare out of scope la threadpool
            template<typename F, typename... Args>
            requires std::is_same_v<std::invoke_result_t<F, Args...>, void>
            std::optional<std::future<bool>> send_task(F&& f,Args... args){

                //wrap di funzione in un task e poi in lamda in modo da ricondursi a firma void()
                using return_type = bool;
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = std::forward<F>(f), ...args_catturati = std::forward<Args>(args) ]()mutable{fun(args_catturati...);
                return true;});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};

                int indx_worker = indx_most_free();
                std::vector<std::unique_lock<std::mutex>> vett_locks(lock_tutti());
                bool flag = workers_[indx_worker]->push_back(j);
                notifica_tutti(); // problema: push e notifica sono sincronizati solo in thread su cui viene fatto push perche mutex che poi leggera è lo stesso bloccato in push.
                                //POSSIBILE SOLUZIONE: fare lock_all() e poi unlock_all() ma cosi ogni send blocca worker_loop di chi ancora non ha superato la cv_.wait(), pero sarebbe sincronizzata la lettura dei count_job ++. 
                unlock_tutti(std::ref(vett_locks));
                if(flag){
                    count_job_[indx_worker]++;
                    return fut;
                }
                return std::nullopt;  //OSSERVAZIONE:return optional e non future cosi possibilita di fallire per push e non è necessario fare while(). spostato check se push e quindi send a buon fine fuori da threadpool perche usando hold queue per esempio non puo fallire il push e quindi ci sarebbe un while inutile              
            };

            //send a giro usando struct indxw 
            //se F(Args) non void
            template<typename F, typename... Args>
            requires (!std::is_same_v<std::invoke_result_t<F, Args...>, void>)
            auto send_task_round(F&& f,Args... args) -> std::optional<std::future<decltype(f(args...))>>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = std::forward<F>(f), ...args_catturati = std::forward<Args>(args) ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};
            
                std::unique_lock<std::mutex> loc(workers_[indxw_.indx_]->get_loc());
                bool flag = workers_[indxw_.indx_]->push_back(j);
                notifica_tutti(); //sincronizzata solo worker a cui si fa il push ma meglio che niente
                loc.unlock();
                if(flag){
                    count_job_[indxw_.indx_]++;
                    indxw_.next(n_worker_);
                    return fut;
                }
                return std::nullopt;
            };
            //se F(Args) void --> wrap in bool() cosi che sara possibile usare future.get() per aspettare che funione venga eseguita prima di mandare out of scope la threadpool
            template<typename F, typename... Args>
            requires std::is_same_v<std::invoke_result_t<F, Args...>, void>
            std::optional<std::future<bool>> send_task_round(F&& f,Args... args){

                //wrap di funzione in un task e poi in lamda in modo da ricondursi a firma void()
                using return_type = bool;
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = std::forward<F>(f), ...args_catturati = std::forward<Args>(args) ]()mutable{fun(args_catturati...);
                return true;});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};

                std::unique_lock<std::mutex> loc(workers_[indxw_.indx_]->get_loc());
                bool flag = workers_[indxw_.indx_]->push_back(j);
                notifica_tutti(); //sincronizzata solo worker a cui si fa il push ma meglio che niente
                loc.unlock();
                if(flag){
                    count_job_[indxw_.indx_]++;
                    indxw_.next(n_worker_);
                    return fut;
                }
                return std::nullopt;
            };

            //per debug di steal job
            bool send_al_worker_n(job j,int n,int & push_count){
                std::unique_lock<std::mutex> loc(workers_[n]->get_loc());
                bool flag = workers_[n]->push_back(j);
                notifica_tutti(); //sincronizzata solo worker a cui si fa il push ma meglio che niente
                loc.unlock();
                if(flag){
                    count_job_[indxw_.indx_]++;
                    push_count++;
                    return true;
                }
                std::cout<<"thread: "<<std::this_thread::get_id()<<"non push"<<std::endl;
                return false;
            };
    };
}
#endif
