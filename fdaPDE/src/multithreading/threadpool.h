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

    enum class steal {no_steal, random, most_busy, random_half_most_busy};

    template <steal T> class Threadpool{
        using job = std::function<void()>;
        //usato per send_task_round 
        struct indx_worker{
            int indx_ = 0;
            //TODO:lasciato con parametro perche usato in send_round parziale, se poi non serve mettere N
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
                    bool stop_ = false;
                    std::mutex m_;
                    std::condition_variable cv_; 
                public:
                    friend class Threadpool; //TODO: classi friend quindi inutili tuti i getter ecc.. ripulire codice. lasciare magari wrap di alcune funzioni per chiarezza in uso in threadpool(es workers_[i]->notifica() al posto di workers_[i]->cv_.notify_one())
                    // costruttore con numero elementi di coda
                    //inizializza thread con funzione membro worker_loop di Threadpoool cosi che accessibili altre code senza passaggio di puntatore/reference a threadpool in worker che causava segmentation fault  
                    Worker(int n, void (Threadpool::*worker_loop)(int),Threadpool* Th, int idx):indx_(idx),sync_queue_(n),t_(worker_loop,Th,idx){}; //oss non serve <N> compila e funziona lo stesso ma non so perche

                    //  TUTTI WRAP "INUTILI" BASTA FATTO CHE SIA FRIEND PER ACCESSO DIRETTO, FORSE PIU LEGGIBILE USARLI PERO.
                    //per poter bloccare il mutex di Worker m_ in threadpool
                    std::unique_lock<std::mutex> get_loc(){
                        std::unique_lock<std::mutex> loc(m_);
                        return loc;          
                    }
                    //per poter notificare da threadpool
                    void notifica(){
                        cv_.notify_one();
                    }

                    void set_stop(bool s){
                        stop_ = s;
                    }
                    void join_thread(){
                        t_.join();
                    }
                
                    //wrap di funzioni per pop e push. 
                    bool push_front(job fun){
                        return sync_queue_.push_front(fun);
                    };
                    bool push_back(job fun){
                        //std::cout<<"incremento count in push_back, count: "<<count_job_<<std::endl;
                        return sync_queue_.push_back(fun);
                    };
                    std::optional<job> pop_front(){
                        return sync_queue_.pop_front();
                        
                    };
                    std::optional<job> pop_back(){
                        return sync_queue_.pop_back();
                    };

            };
        private:
            std::vector<std::shared_ptr<Worker>> workers_; //vettore di putatori perche non movable e copiable synchro_queue per via di mutex
            std::deque<std::atomic<int>> count_job_; //deque perché atomic<int> non movable e quindi non si puo fare vector
            int n_worker_ ;
            int queue_size_; // forse non costante poi
            int threadpool_volume_; //oss: se queue_size non costante fai funzione get thteadpool volume che lo calcola
            indx_worker indxw_; 
            std::shared_mutex m_threadpool_; 
            std::condition_variable_any cv_threadpool_;
            bool active_ = false; //TODO oss: possibile evitare stop_ di singolo worker e usare active, ma stop_ in singolo worker rende worker indipendente da threadpool (puo terminare anche se threadpool non distrutta) e questo puo essere utile magari in seguito 
        public:
            friend class Worker; //poi togliere tuti get e sostituire con accesso diretto
            //n = size code, k = numero worker
            Threadpool(int n,int k):n_worker_(k),queue_size_(n){
                threadpool_volume_ = k * n;
                std::unique_lock<std::shared_mutex> loc(m_threadpool_,std::defer_lock);
                loc.lock();
                //if(k> std::thread::hardware_concurrency()){std::cout<<"thread richiesti > thread supportati: "<<std::thread::hardware_concurrency()<<std::endl; }
                workers_.reserve(k);
                for(int i=0; i<k; i++){
                    workers_.emplace_back(std::make_shared<Worker> (n,&Threadpool::worker_loop,this,i));
                    count_job_.emplace_back(0);
                }
                active_ = true;
                cv_threadpool_.notify_all();
            };

            //constructor default numero di worker ma specifica size code
            Threadpool(int n): Threadpool(n, std::thread::hardware_concurrency()){}

            //default constructor
            Threadpool(): Threadpool(256){}

            ~Threadpool(){
                //std::unique_lock<std::mutex> loc_t(m_threadpool_);
                //active_= false;
                //loc_t.unlock();
                while(get_count_all_job()>0){
                    std::this_thread::yield(); //per dare piu tempo a worker di finire e ridurre esecuzione cicli while a vuoto.
                }; //aspetta finche count_job[] di tutti non a zero
                //aspetta che code siano vuote una per volta, messo dopo count_job perche questo chiama empty e quindi locca interventi su coda e rallenta tutto, dopo while con get_count_all_job elemeti in coda dovreero essere zero o quasi quindi empty rallenta ma molto meno
                for (int i =0; i<n_worker_; i++){
                    while(!workers_[i]->sync_queue_.empty()){}
                }

                //TODO: ora deve aspettare che worker abbiano effettivamente finito i job dopo averli pop da coda
                //RISPOSTA: i realta ora sicuro che che code sono vuote quindi eseguito pop su tutte, dopo viene messo stop_ = true ma dato che siamo dopo il pop prima di check su stop_ cronologicamente in workerloop avviene esecuzioe job :)

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
                //per assicurare che thread partano a fare worker_loop solo dopo che tutti siano stati inizializzati
                m_threadpool_.lock_shared();
                cv_threadpool_.wait(m_threadpool_,[this](){return active_;});
                m_threadpool_.unlock_shared();
                while(!workers_[i]->stop_){
                    //TODO è meglio fare unico thread_local job per ogni thread e riusare sempre quello ?. forse si perche tanto std::fuction<> ha membro operator = quindi permesso copia assegazione non solo inizializzazione
                    std::optional<job> try_j = workers_[i]->pop_front();
                    if(try_j){//esegue se non è nullopt
                        count_job_[i]--;
                        (try_j.value())();
                    }
                    else{
                        std::unique_lock<std::mutex> loc(workers_[i]->m_); //OSS: empty() gia sincronizzato con push grazie a mtex dentro Synchro_queue, mutex in Worker serve solo per avere cv che manda a dormire. get_count_job_all() invece vede sincronizzati solo i count_job[i]++ perche avvengono in mutex con push, pro: se ce push chi non lha ricevuto si sveglia per rubare, contro: possibile svegliarsi e invece non ce niente da rubare perche count_job[i]-- gia fatto ma non letto perche non sincornizzato 
                        //TODO: migliorare condizioni sveglia. vedi oss fine riga successiva
                        workers_[i]->cv_.wait(loc,[&](){return get_count_all_job()>0 || !workers_[i]->sync_queue_.empty() || workers_[i]->stop_ ;}); //oss: spostato get_count_job() per primo cosi si riduce chiamata a empty() che blocca push e pop su coda. //OSS: magari get_cout_jo()>N cosi da evitare sveglia se steal non necessario (logica da coordinare con send) //TODO: empty() inutile basta get_count_al_job. se get_count_all_job()>N mettere has_job = count_job[i]>0 al posto di empty() cosi chi ha suo si sveglia e ladri si svegliano solo se ognuno ne ha piu di uno (oss: get_count_all_job()>n_worker <-> ogni worker ha 1 job, se usato send_round e nemmeno certo)
                        loc.unlock();
                        if(workers_[i]->stop_){return;}
                        std::optional<job> j = workers_[i]->pop_front();
                        if(j){//esegue se non è nullopt. sigifica non empty()==true
                            count_job_[i].fetch_sub(1,std::memory_order_release);
                            (j.value())(); //esegue funzioni con 0 parametri e void. per non void si dovra fare wrap e associare a promise. per parametri lamda wrap che li cattura cosi no param  
                            //std::cout<<"thread: "<<std::this_thread::get_id()<<" ha eseguito"<<std::endl; 
                        }
                        else{ //steal.
                            if constexpr(T == steal::random){
                                random_steal_and_do();
                            }
                            if constexpr(T == steal::most_busy){
                                steal_from_most_busy_and_do();
                            }
                            if constexpr(T == steal::random_half_most_busy){
                                steal_random_from_most_busy_and_do();
                            } 
                            //std::cout<<"furtooooo"<<std::endl;
                                //oss: steal_random_from_most_busy_and_do() ha senso solo per N > 5 (perche dimezza i worker con job tra cui sceglie random)
                            
                        }                             
                    }
                }
            };
            //al posto di steal se n_worer solo 2
            //ora che umero worker non noto at compile time inutile
            void steal_from_the_other_one_and_do(int i){
                int indx_the_other_one = (i == 0)?(1):(0);
                //if(count_job_[indx_the_other_one]<2) return; 
                std::optional<job> j = workers_[indx_the_other_one]->pop_back();
                if(j){
                    count_job_[indx_the_other_one].fetch_sub(1,std::memory_order_release);
                    (j.value())();
                }
            };
            
            void steal_from_most_busy_and_do(){ 
                int most_busy = indx_most_busy();
                if(most_busy != -1){
                    std::optional<job> j = workers_[most_busy]->pop_back();
                    if(j){
                        count_job_[most_busy].fetch_sub(1,std::memory_order_release);
                        (j.value())();
                        //std::cout<<"thread: "<<std::this_thread::get_id()<<" ha rubato"<<std::endl;
                    }
                    //else{std::cout<<"thread: "<<std::this_thread::get_id()<<"nulloptRubato"<<std::endl;}
                }
            }

            // per steal random tra chi ha code non vuote
            int random_indx(int size){
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> distrib(0,size-1);
                return distrib(gen);
            }
            void random_steal_and_do(){
                std::vector<int> indxs;
                for(int k = 0; k<n_worker_; k++){
                    //if per evitare se stessi.  poi da fare piu efficente se possibile
                    //if(k != i){ per ora tolto, se fa steal molto probabilmente ha coda vuota (non certo perche pop in Relax puo fallire anche se coda non vuota)
                        if(count_job_[k].load(std::memory_order_acquire) > 0){
                            indxs.push_back(k);
                        }
                    //}
                }
                int size = indxs.size();
                if(size == 0){ return;}
                int indx = indxs[random_indx(size)];
                std::optional<job> j = workers_[indx]->pop_back();
                if(j){
                    count_job_[indx].fetch_sub(1,std::memory_order_release);
                    (j.value())();
                    //std::cout<<"thread: "<<std::this_thread::get_id()<<" ha eseguito"<<std::endl;
                }
                //else{std::cout<<"thread: "<<std::this_thread::get_id()<<"nullopt"<<std::endl;}
                
            }

            //m numero di worker con job, i ladri fanno steal a worker casuale tra i primi m/2 con piu job in coda
            //obbiettivo evitare starvation di steal_from_most_busy ma preservare la sua proprietà stabilizzante (steal_from_most_busy tende a riequilibrare i count_job, steal_random mantiene squilibrio )
            //OSS: importante equilibrare count_job perchè count_job[] squilibrati porta poi a maggiore penalita per starvation
            void steal_random_from_most_busy_and_do(){ 
                std::vector<std::pair<int,int>> indxs; //vettore di coppie j,count_job[j] 
                int tmp_count_job = 0;
                //mappa di chi ha count_job()>0
                for(int k = 0; k<n_worker_; k++){
                    tmp_count_job = count_job_[k];
                    if(tmp_count_job > 0){
                        indxs.emplace_back(k,tmp_count_job);
                    }
                }
                int size = indxs.size();
                if(size == 0) {return;}
                std::sort(indxs.begin(), indxs.end(), [](std::pair<int,int> &a, std::pair<int,int> &b) {return a.second > b.second;}); //ordina da chi ha piu job (indx[].second) a chi ne ha meno
                int indx = 0;
                switch (size)
                {
                case 1:
                    indx = indxs[0].first; //evita chiamata a geerazioe random number inutile
                    break;
                default: 
                    indx = indxs[random_indx(size/2)].first; //sceglie a caso tra i primi size/2 con count_job maggiore. //TODO: per ora size/2, sarebbe meglio aggiungere dipendenza da N-size (N-size = numero worker ladri) o simile. 
                    break;
                }
                std::optional<job> j = workers_[indx]->pop_back();
                if(j){
                    count_job_[indx].fetch_sub(1,std::memory_order_release);
                    (j.value())();
                }
            }
            
            int get_count_all_job(){
                int somma = 0;
                for (int i = 0; i<n_worker_; i++)
                    somma += count_job_[i].load(std::memory_order_acquire);
                return somma;
            }
            //per notificare tutte le CV_ dei worker
            void notifica_tutti(){
                for(int i=0; i<n_worker_; i++){
                    workers_[i]->cv_.notify_one();
                }
            }
            //per acquisire tutti i mutex dei worker
            std::vector<std::unique_lock<std::mutex>> lock_tutti(){
                std::vector<std::unique_lock<std::mutex>> vett_lock;
                vett_lock.reserve(n_worker_);
                for(int i=0; i<n_worker_; i++){
                    std::unique_lock<std::mutex> loc_w(workers_[i]->m_);
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

            int indx_most_free(){
                int worker_indx = 0;
                int min_elem= count_job_[0].load(std::memory_order_acquire); //numero elementi in primo worker 
                for (int j=1; j<n_worker_; j++){
                    int current_el = count_job_[j].load(std::memory_order_acquire);
                    if(current_el < min_elem ){ 
                        worker_indx = j;
                        min_elem = current_el;
                    }
                }
                return worker_indx;
            };

            int indx_most_busy(){
                int worker_indx = 0;
                int max_elem= count_job_[0].load(std::memory_order_acquire); //numero elementi in primo worker 
                for (int j=1; j<n_worker_; j++){
                    int current_el = count_job_[j].load(std::memory_order_acquire);
                    if(current_el > max_elem ){
                        worker_indx = j;
                        max_elem = current_el;
                    }
                }
                if(max_elem == 0){return -1;} //evita steal di nullopt 
                return worker_indx;
            }

            //send generico combina send_round e send_most_free. se job in threadpool alto allora manda a chi piu libero, se basso manda a giro. (alto basso per ora segnato da threadpool_volume_/2)
            //TODO: se viene tolto togliere ache membro threadpool volume
            template<typename F, typename... Args>
            auto send(F&& f,Args... args)-> std::optional<std::future<decltype(f(args...))>>{
                if(get_count_all_job()> threadpool_volume_/2){
                    //std::cout<<"sendFree"<<std::endl;
                    return send_task(f,args...);
                }
                else{
                    //std::cout<<"sendround"<<std::endl;
                    return send_task_round(f,args...);
                }
            }
           //TODO: tutti i send uguali cambia solo scelta indice, ma non si puo fare template<typename F, typename... Args, typename tipo_send> perche templeta parameter deduction è un tutto o niente, come fare allora per semplificare ?
           //lock di tutti i mutex cosi notifica a tutti sara sincronizzata con push e  count++, OSSERVAZIONE: lock di mutex non interrompe worker_loop di chi ha gia job in coda grazie a nuovo worker_loop con try_j all inizio di while 
           //OSS:ora che i distruttore si aspetta esecuzioen di tutti i job non servono piu i future<void> per assivurare esecuzione di job, si potrebbe pensare di ritornare  aversioni conn return=void che nnon perdono tempo a creare vector<future<void>>, PERO' meglio lasciarli perche con future<void> in main con get si puo specificare entro quando vuoi che un job sia eseguito
            template<typename F, typename... Args>
            auto send_task(F&& f,Args... args) -> std::optional<std::future<decltype(f(args...))>>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = std::forward<F>(f), ...args_catturati = std::forward<Args>(args) ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};

                int indx_worker = indx_most_free();
                std::vector<std::unique_lock<std::mutex>> vett_locks(lock_tutti()); // alternativa lock dei mutex direttamente 
                bool flag = workers_[indx_worker]->push_back(j);
                notifica_tutti(); // problema: push e notifica sono sincronizati solo in thread su cui viene fatto push perche mutex che poi leggera è lo stesso bloccato in push.
                                //POSSIBILE SOLUZIONE: fare lock_all() e poi unlock_all() ma cosi ogni send blocca worker_loop di chi ancora non ha superato la cv_.wait(), pero sarebbe sincronizzata la lettura dei count_job ++. 
                if(flag){
                    count_job_[indx_worker].fetch_add(1,std::memory_order_release); // TODO: in realta dato che dentro mutex basta relax ancora piu efficente, però non tutte le letture avvengono dentro mutex quindi non saprei
                    unlock_tutti(std::ref(vett_locks));
                    return fut;
                }
                unlock_tutti(std::ref(vett_locks));
                return std::nullopt;  //OSSERVAZIONE:return optional e non future cosi possibilita di fallire per push e non è necessario fare while(). spostato check se push e quindi send a buon fine fuori da threadpool perche usando hold queue per esempio non puo fallire il push e quindi ci sarebbe un while inutile              
            };

            //send a giro usando struct indxw 
            
            template<typename F, typename... Args>
            auto send_task_round(F&& f,Args... args) -> std::optional<std::future<decltype(f(args...))>>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = std::forward<F>(f), ...args_catturati = std::forward<Args>(args) ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};
            
                std::vector<std::unique_lock<std::mutex>> vett_locks(lock_tutti());
                bool flag = workers_[indxw_.indx_]->push_back(j);
                notifica_tutti(); //sincronizzata solo worker a cui si fa il push ma meglio che niente
                if(flag){
                    count_job_[indxw_.indx_].fetch_add(1,std::memory_order_release);
                    indxw_.next(n_worker_); //dentro mutex per sincronizzazione visione (se solo un thread manda non necessario)
                    unlock_tutti(std::ref(vett_locks));
                    return fut;
                }
                unlock_tutti(std::ref(vett_locks));
                return std::nullopt;
            };

            //send a sola meta di worker per debug/ test di steal job. 
            
            template<typename F, typename... Args>
            auto send_task_only_to_some(F&& f,Args... args) -> std::optional<std::future<decltype(f(args...))>>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = std::forward<F>(f), ...args_catturati = std::forward<Args>(args) ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};
            
                std::vector<std::unique_lock<std::mutex>> vett_locks(lock_tutti());
                bool flag = workers_[indxw_.indx_]->push_back(j);
                notifica_tutti(); //sincronizzata solo worker a cui si fa il push ma meglio che niente
                if(flag){
                    count_job_[indxw_.indx_].fetch_add(1,std::memory_order_release);
                    if (n_worker_ != 1)
                        indxw_.next(n_worker_/2);
                    unlock_tutti(std::ref(vett_locks));
                    return fut;
                }
                unlock_tutti(std::ref(vett_locks));
                return std::nullopt;
            };

            //send a sola  a un worker (0 perche sicuro esiste sempre) per debug/ test di steal job. 
            template<typename F, typename... Args>
            auto send_task_only_to_zero(F&& f,Args... args) -> std::optional<std::future<decltype(f(args...))>>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = std::forward<F>(f), ...args_catturati = std::forward<Args>(args) ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};
            
                std::vector<std::unique_lock<std::mutex>> vett_locks(lock_tutti());
                bool flag = workers_[0]->push_back(j);
                notifica_tutti(); //sincronizzata solo worker a cui si fa il push ma meglio che niente
                if(flag){
                    count_job_[indxw_.indx_].fetch_add(1,std::memory_order_release);
                    unlock_tutti(std::ref(vett_locks));
                    return fut;
                }
                unlock_tutti(std::ref(vett_locks));
                return std::nullopt;
            };

            //PARALLEL_FOR
            //2 tipi a seconda di body function in for loop:
            //      1)body fuction dipendente da i.     body function passate come wrap di funzioni in lambda e quindi unico parametro i: [args](int i){retur fun(args,i);}
            //      2)body fuction INdipendente da i.   niente wrap in lamda, passata direttamente funzione e argomenti 
            //TODO: ci sara modo per poter passare direttamente la funzione senza wrap e separare args ed i cosi da avere un unica funzione parallel_for per tutti i casi
            //TODO: capire se necessario perfect forwarding


            //1
            //OSS: diversificato caso return e void perche void non serve faccia vector da ritornare quindi piu veloce. NO! vedi oss sotto 
            //OSS: meglio return vettore di future void cosi in main si puo garantire con get entro quando si vuole che un job void venga eseguito
             
            template<typename F>
            auto parallel_for(int start, int end, F&& f)-> std::vector<std::optional<std::future<std::invoke_result_t<F, int>>>>{
                using return_type = std::invoke_result_t<F, int>;
                std::vector<std::optional<std::future<return_type>>> ret;
                for(int j=start; j<end; j++){
                    ret.push_back(this->send_task_round(std::forward<F>(f),j));
                }
                return ret;
            }   

            //2
            template<typename F, typename... Args>
            auto parallel_for(int n, F&& f, Args... args)-> std::vector<std::optional<std::future<decltype(f(args...))>>>{
                using return_type = decltype(f(args...));
                std::vector<std::optional<std::future<return_type>>> ret;
                for(int j=0; j<n; j++){
                    ret.push_back(this->send_task_round(std::forward<F>(f),std::forward<Args>(args)...));
                }
                return ret;
            }

            //PARALLEL_FOR_ITERATOR
            //parallel_for_iterator: for_loop scorre cotainer con iterator it e applica funzione a tutti elementi di container f(*it)
            template<typename Iterator, typename F> 
            //TODO: da aggiugere vicolo che iteartor abbia membro value_type
            auto parallel_for_iterator(Iterator begin, Iterator end,F&& f)->std::vector<std::optional<std::future<std::invoke_result_t<F,typename Iterator::value_type>>>>{
                using return_type = std::invoke_result_t<F,typename Iterator::value_type>;
                std::vector<std::optional<std::future<return_type>>> ret;
                for (auto it = begin; it!=end; it++){
                    ret.push_back(this->send_task_round(std::forward<F>(f),*it));
                }
                return ret;
            }
        };
    }
#endif


