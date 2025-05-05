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
                    //fdapde::Threadpool* ptr_threadpool_; per far accedere worker a metodi di threadpool e poter fare steal job ma problemi con distruttori segmentation fault
                    fdapde::Synchro_queue<job,fdapde::relax_nowait> sync_queue_;
                    std::thread t_;
                    bool stop_ = false;
                    std::mutex m_;
                    std::condition_variable cv_; 
                    int count_job_ = 0;
                public:
                    // costruttore con numero elementi di coda
                    Worker(int n):sync_queue_(n),t_(&Worker::worker_loop,this){};
                    
                    ~Worker(){
                        //while(!sync_queue_.empty()){}; //per aspettare che worker finisca i job in coda. PB: a volte empty() chiamato prima di push_back() di metodo send_task di threadpool
                        std::unique_lock<std::mutex> loc(m_);
                        stop_ = true; //PROBLEMA: chiamato distruttore prima che job effettivamente finiti. SOLUZIONE: usare future associato a task in main cosi che future.get() garantisce fine di task prima di chiamata distruttore
                        //DOMANDA: serve che stop_ sia atomico ? bisogna distruggere dento al mutex ? perche non è possibile che notifca arrivi a worker_loop si sveglia e controlla stop_ prima che sia effettivamente cambiato
                        //CREDO SERVA loc di MUTEX !!!!!!!!!!!!
                        //oss: stop atomico inutile, o serve mutex e quindi modifica stop dentro mutex quindi non serve sia atomico, o non serve niente basta il notify (come in threadpool di link di primisimo esempio visto) 
                        cv_.notify_one();
                        loc.unlock();
                        t_.join();
                    }
                    //per poter bloccare il mutex di Worker m_ in threadpool
                    std::unique_lock<std::mutex> get_loc(){
                        std::unique_lock<std::mutex> loc(m_);
                        return loc;          
                    }
                    //per poter notificare da threadpool
                    void notifica(){
                        cv_.notify_one();
                    }
                    // tre woorker_loop: uno con semplice yields, uno con mutex e cv, uno con steal
                    // oss: se non worker_loop con mutex va tolto: mutex, cv, notifica(), lock(),  uso in metodi send ecc...
                     //mutex
                    void worker_loop(){
                        while(!stop_){
                            //TODO: capire come mettere a dormire se coda vuota, nel frattempo messo yields()
                            //OSS: uso di empty() brutto ma non vanifica del tutto synchro_queue perche blocca tutto mentre lo fa ma poi pop e push semi sequenziali.
                            //     il problema è l'uso di un mutex necessario per usare una cv che renderebbe tutto sequenziale e vanifica tutto lavoro fatto fin ora(sync_queue sarebbe inutile tanto varebbe usare normale deque)
                            //OSS: possibile mettere un  mutex che avvisi e poi fare pop fuori da mutex, push pero deve essere fatto dentro per avere certezza che notify() sia coerente 
                            //OSS: alternativa thread non va mai a dormire, se fa il pop di un nullopt va a rubare job ad altri (credo lo faccia eigen cosi)
                            std::unique_lock<std::mutex> loc(m_);
                            cv_.wait(loc,[this](){return !sync_queue_.empty() || stop_;});
                            loc.unlock();
                            if(stop_ == true){return;} //per evitare che finisca iterazione quando coda vuota ma svegliato per segnale di stop e quindi pop di nullopt
                            //std::cout<<std::this_thread::get_id()<<"sta facendo workerloop"<<std::endl; //per debug
                            //OSS: cosi facendo durante un push da threadpool non possono avvenire pop (sequenziale :( ), ma rimane che durante un pop possono essere fatti dei push. si perde a meta il vantaggio di avere synchro_queue parzialmente non bloking, ma non del tutto
                            std::optional<job> j = pop_front();
                            if(j){//esegue se non è nullopt
                                (j.value())(); //esegue funzioni con 0 parametri e void. per non void si dovra fare wrap e associare a promise. per parametri lamda wrap che li cattura cosi no param  
                            }                            
                        }
                    };
                    
                    /*  //Yiels
                    void worker_loop(){
                        while(!stop_){
                            std::optional<job> j = pop_front();
                            if(j){//esegue se non è nullopt
                                (j.value())(); //esegue funzioni con 0 parametri e void. per non void si dovra fare wrap e associare a promise. per parametri lamda wrap che li cattura cosi no param  
                            }
                            else{
                                std::this_thread::yield(); //da controllo a OS, possibile che sospenda l'esecuzione del thread a favore di altro, (usato per mettere una pezza a mancanza condion varibale che fa wait se coda empty)
                            }                             
                        }
                    };
                    */
                    /*  //STEAL
                    void worker_loop(){
                        while(!stop_){
                            if(count_job_ == 0){
                                std::this_thread::yield();
                            }
                            if(!sync_queue_.empty()){
                                std::optional<job> j = pop_front();
                                if(j)//esegue se non è nullopt
                                    (j.value())(); //esegue funzioni con 0 parametri e void. per non void si dovra fare wrap e associare a promise. per parametri lamda wrap che li cattura cosi no param  
                            }
                            else{ //steal
                                //steal_from_most_busy_and_do();  
                            }                                 
                        }
                    };
                    */
                    //oss: top sarebbe combinare steal e mutex ma come ?
                    
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

                    /* ancora da capire come far accedere worker a threadpool, con puntatori il problema è segmentation fault durante distruzione
                    //per rubare job da back a chi è piu impegnato ed eseguirlo
                    void steal_from_most_busy_and_do(){
                            int most_busy = indx_most_busy();
                            std::optional<job> j = threadpool_[most_busy]->pop_back();
                            if(j){
                                (j.value())();
                            }
                    }
                    */

            };
        private:
            std::vector<std::shared_ptr<Worker>> threadpool_; //vettore di putatori perche non movable e copiable synchro_queue per via di mutex
            int n_worker_;
            indx_worker indxw_; 
        public:
            //n = size code, k = numero worker
            Threadpool(int n, int k):n_worker_(k){
                threadpool_.reserve(k);
                for(int i=0; i<k; i++){
                    threadpool_.emplace_back(std::make_shared<Worker> (n));
                }
            };

            //ridà indice di worker piu libero (lettura di elmenti in sync_queue di worker fatta con metodo count_el non affidabile ma è abbastanza per avere un idea)
            int indx_most_free(){
                int worker_indx = 0;
                int min_elem= threadpool_[0]->get_count_job(); //numero elementi in primo worker 
                for (int j=1; j<n_worker_; j++){
                    int current_el = threadpool_[j]->get_count_job();
                    if(current_el < min_elem ){ 
                        worker_indx = j;
                        min_elem = current_el;
                    }
                }
                return worker_indx;
            };
            // indice di worker con piu job in coda, sara utile per steal job
            int indx_most_busy(){
                int worker_indx = 0;
                int max_elem= threadpool_[0]->get_count_job(); //numero elementi in primo worker 
                for (int j=1; j<n_worker_; j++){
                    int current_el = threadpool_[j]->get_count_job();
                    if(current_el > max_elem ){
                        worker_indx = j;
                        max_elem = current_el;
                    }
                }
                return worker_indx;
            }
            //send con indx_freer criterio
            bool send_task(job j){
                int indx_worker = indx_most_free();
                std::unique_lock<std::mutex> loc(threadpool_[indx_worker]->get_loc());
                bool flag = threadpool_[indx_worker]->push_back(j);
                threadpool_[indx_worker]->notifica();
                loc.unlock();
                if(flag){
                    return true;
                }
                return false;
            };
            //send a giro usando struct indxw 
            bool send_task_round(job j){
                std::unique_lock<std::mutex> loc(threadpool_[indxw_.indx_]->get_loc());
                bool flag = threadpool_[indxw_.indx_]->push_back(j);
                threadpool_[indxw_.indx_]->notifica();
                loc.unlock();
                if(flag){
                    indxw_.next(n_worker_);
                    return true;
                }
                return false;
            };
    };
}
#endif