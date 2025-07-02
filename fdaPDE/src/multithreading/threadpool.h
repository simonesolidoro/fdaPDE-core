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

    enum class op {sum,mult};

    template <steal T> class Threadpool{
        using job = std::function<void()>;
        //usato per send_task_round 
        //OSS: non thread-safe perché unico thread a modificarlo é main-thread che fa send, se poi implementate funzioni per cui job() eseguito da worker consiste in send() altri job a threadpool allora va reso thread-safe 
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
                    mutable std::mutex m_;
                    mutable std::condition_variable cv_; 
                public:
                    friend class Threadpool; //TODO: classi friend quindi inutili tuti i getter ecc.. ripulire codice. lasciare magari wrap di alcune funzioni per chiarezza in uso in threadpool(es workers_[i]->notifica() al posto di workers_[i]->cv_.notify_one())
                    // costruttore con numero elementi di coda
                    //inizializza thread con funzione membro worker_loop di Threadpoool cosi che accessibili altre code senza passaggio di puntatore/reference a threadpool in worker   
                    Worker(int n, void (Threadpool::*worker_loop)(int),Threadpool* Th, int idx):indx_(idx),sync_queue_(n),t_(worker_loop,Th,idx){}; 

                    //  TUTTI WRAP "INUTILI" BASTA FATTO CHE SIA FRIEND PER ACCESSO DIRETTO, FORSE PIU LEGGIBILE USARLI PERO.
                    //per poter bloccare il mutex di Worker m_ in threadpool
                    std::unique_lock<std::mutex> get_loc()const{
                        std::unique_lock<std::mutex> loc(m_);
                        return loc;          
                    }
                    //per poter notificare da threadpool
                    void notifica()const{
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
            std::deque<std::atomic<int>> count_job_; //deque perché atomic<int> non movable e quindi non si puo fare vector,ideale sarebbe array perché cache friendly (utile perché get_count_all_job lo scorre) ma non va bene threadpool<sizeArray>
            int n_worker_ ;
            int queue_size_; // forse non costante poi
            indx_worker indxw_; 
            std::shared_mutex m_threadpool_; 
            std::condition_variable_any cv_threadpool_;
            bool active_ = false; //TODO oss: possibile evitare stop_ di singolo worker e usare active, ma stop_ in singolo worker rende worker indipendente da threadpool (puo terminare anche se threadpool non distrutta) e questo puo essere utile magari in seguito 
            mutable std::mt19937 gen;
        public:
            friend class Worker; //poi togliere tutti get e sostituire con accesso diretto
            //n = size code, k = numero worker
            Threadpool(int n,int k):n_worker_(k),queue_size_(n),gen(std::random_device{}()){
                std::unique_lock<std::shared_mutex> loc(m_threadpool_);
                //if(k> std::thread::hardware_concurrency()){std::cout<<"thread richiesti > thread supportati: "<<std::thread::hardware_concurrency()<<std::endl; }
                workers_.reserve(k);
                for(int i=0; i<k; i++){
                    workers_.emplace_back(std::make_shared<Worker> (n,&Threadpool::worker_loop,this,i));
                    count_job_.emplace_back(0);
                }
                active_ = true;
                loc.unlock();
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
                //aspetta che code siano vuote una per volta, messo dopo count_job perche questo chiama empty e quindi blocca interventi su coda e rallenta tutto, dopo while con get_count_all_job elemeti in coda dovreero essere zero o quasi quindi empty rallenta ma molto meno
                for (int i =0; i<n_worker_; i++){
                    while(!workers_[i]->sync_queue_.empty()){}
                }

                //TODO: ora deve aspettare che worker abbiano effettivamente finito i job dopo averli pop da coda
                //RISPOSTA: in realta ora sicuro che che code sono vuote quindi eseguito pop su tutte, dopo viene messo stop_ = true ma dato che siamo dopo il pop prima di check su stop_ cronologicamente in workerloop avviene esecuzioe job :)

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

            //prova a eseguire job
            //OSS: non fatto bool try_do(i) con workers_[i]->pop dentro funzione perchè pop da front ma in steal pop da back 
            bool try_do(std::optional<job> j, int indx){ //j job da eseguire se non nullopt, i indx di worker da cui si prende il job.
                if(j){//esegue se non è nullopt. 
                    (j.value())(); 
                    //spostato decremento dopo esecuzione job, perche:
                    //usando send mostfree da job a primi worker loro fanno subito pop e quindi da secondi job a primi worker che hanno coda vuota ma stanno eseguendo il primo job al posto di darlo a worker ultimi liberi.
                    //questo è un problema da quando messo che send non notifica tutti e quindi non sveglia per steal gli ultimi worker quando i primi hanno job in coda
                    count_job_[indx].fetch_sub(1,std::memory_order_release); //TODO: capire miglior memory order (forse relax che lascia compilatore piu libero di ottimizzare, perche tanto non è sincronizzato i realise con gli acquire)
                    return true;
                }
                return false;
            }

            void worker_loop(int i){// i = indice di worker in workers_
                //per assicurare che thread partano a fare worker_loop solo dopo che tutti siano stati inizializzati
                //shared cosi che tutt i thread possano partire contemporaneamente
                std::shared_lock<std::shared_mutex> lock_shared(m_threadpool_);
                cv_threadpool_.wait(lock_shared,[this](){return active_;}); //cv.wait(loc) quando ritenta a verificare condition dopo notifica fa loc.lock() (chiama membro lock() di loc) e con loc=shared_lock<std::shared_mutex> il membro lock() è shared_lock(), prima con passaggio di loc= shared_mutex il membro lock() era lock() (cioe blocco in modalita scrittura) :()
                lock_shared.unlock();
                bool done_own_job = true; //spostato fuori da while cosi non locale e creato una volta sola 
                int indx_steal = -1; //indice da cui rubare
                while(!workers_[i]->stop_){
                    //TODO è meglio fare unico thread_local job per ogni thread e riusare sempre quello ?. forse si perche tanto std::fuction<> ha membro operator = quindi permesso copia assegazione non solo inizializzazione
                    done_own_job = try_do(workers_[i]->pop_front(),i);
                    if(done_own_job){
                        continue; //passa a iterazione di while successiva evitando steal ecc
                    }
                    if( get_count_all_job()>0){ //aggiunto tentativo di steal qui cosi che si passi da mutex (che ricordiamo viene lock anche ad ogni send) solo quando non c'è job in nessuna coda. 
                        //steal
                        if constexpr(T == steal::random){
                            indx_steal = indx_random_from_busy();
                        }
                        if constexpr(T == steal::most_busy){
                            indx_steal = indx_most_busy();
                        }
                        if constexpr(T == steal::random_half_most_busy){
                            indx_steal = indx_random_from_half_most_busy();
                            //oss: steal_random_from_most_busy_and_do() ha senso solo per N > 5 (perche dimezza i worker con job tra cui sceglie random)
                        } 

                        if(indx_steal != -1){
                            //do job steal
                            try_do(workers_[indx_steal]->pop_back(),indx_steal);
                        }
                        continue;                             
                    }
                    //arrivare qui significa nessun job in coda probabilmente e quindi va a dormire in CV, non certo perche count_job non sincronizzato quindi non attendibile, ma cosi si evita blocco di mutex che viene bloccato anche ad ogni send e quindi rallentava molto il parallel_for_sure se range_for grande
                    std::unique_lock<std::mutex> loc(workers_[i]->m_); //OSS:mutex in Worker serve solo per avere cv che manda a dormire. get_count_job_all() invece vede sincronizzati solo i count_job[i]++ perche avvengono in mutex con push, pro: se ce push chi non lha ricevuto si sveglia per rubare, contro: possibile svegliarsi e invece non ce niente da rubare perche count_job[i]-- gia fatto ma non letto perche non sincornizzato 
                    workers_[i]->cv_.wait(loc,[&](){return get_count_all_job()>0 || count_job_[i].load(std::memory_order_acquire) > 0 || workers_[i]->stop_;}); // get_count_all_job > n_worker cosi da svegliare per steal solo se c'è da rubare per tutti (questa è l'idea, non proprio precisa l'esecuzione ma vabbè), poi count_job[i] cosi worker si sveglia se è inviato job a lui (è certo che si svegli se send job a lui perchè count_job sincronizzati da mutex di CV qui e di send in send)
                    loc.unlock();
                    if(workers_[i]->stop_){return;}
                    done_own_job = try_do(workers_[i]->pop_front(),i); //riprova a fare proprio job, (se svegliato per count_job[i]>0)
                    if(!done_own_job){ //steal.
                        if constexpr(T == steal::random){
                            indx_steal = indx_random_from_busy();
                        }
                        if constexpr(T == steal::most_busy){
                            indx_steal = indx_most_busy();
                        }
                        if constexpr(T == steal::random_half_most_busy){
                            indx_steal = indx_random_from_half_most_busy();
                            //oss: steal_random_from_most_busy_and_do() ha senso solo per N > 5 (perche dimezza i worker con job tra cui sceglie random)
                        } 

                        if(indx_steal != -1){
                            //do job steal
                            try_do(workers_[indx_steal]->pop_back(),indx_steal);
                        }                             
                    }
                }
            };

            int indx_most_free()const{
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

            int indx_most_busy()const{
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


            // ridà int a caso tra 0 e size-1
            int random_int(int size)const{
                std::uniform_int_distribution<> distrib(0,size-1);
                return distrib(gen);
            }

            //indx random tra i worker con job in coda ovviamente
            int indx_random_from_busy()const{
                std::vector<int> indxs; //vector di indici di worker in workers che hanno job in coda
                for(int k = 0; k<n_worker_; k++){
                    // oss: non evitati se stessi (k!=j) perche sarebbe piu costoso (range di for diminusice di un elemento, ma ad ogni iterazione ci sarebbe if aggiuntivo)
                    if(count_job_[k].load(std::memory_order_acquire) > 0){
                        indxs.push_back(k);
                    }
                }
                int size = indxs.size();
                switch(size){
                case 0: 
                    return -1;
                case 1:
                    return indxs[0];
                default:
                    return indxs[random_int(size)];   
                }                
            }

            //m numero di worker con job, i ladri fanno steal a worker casuale tra i primi m/2 con piu job in coda
            //obbiettivo evitare starvation di steal_from_most_busy ma preservare la sua proprietà stabilizzante (steal_from_most_busy tende a riequilibrare i count_job, steal_random mantiene squilibrio )
            //OSS: importante equilibrare count_job perchè count_job[] squilibrati porta poi a maggiore penalita per starvation
            int indx_random_from_half_most_busy()const{ 
                std::vector<std::pair<int,int>> indxs; //vettore di coppie j,count_job[j]. j indice di worker in workers
                int tmp_count_job = 0;
                //chi ha count_job()>0 inserito in indxs
                for(int k = 0; k<n_worker_; k++){
                    tmp_count_job = count_job_[k];
                    if(tmp_count_job > 0){
                        indxs.emplace_back(k,tmp_count_job);
                    }
                }
                int size = indxs.size();
                if(size == 0) {return -1;}
                std::sort(indxs.begin(), indxs.end(), [](std::pair<int,int> &a, std::pair<int,int> &b) {return a.second > b.second;}); //ordina da chi ha piu job (indx[].second) a chi ne ha meno
                switch (size)
                {
                case 1:
                    return indxs[0].first; //evita chiamata a generazioe random number inutile
                case 2:
                    return indxs[0].first; //evita chiamata a generazioe random number inutile size/2 = 2/2 = 1 -> random_int(1)= numero casuale in {0,(1-1)} cioè = 0 sempre
                case 3:
                    return indxs[0].first; //evita chiamata a generazioe random number inutile size/2 = 3/2 = 1 -> "
                default: 
                    return indxs[random_int(size/2)].first; //sceglie a caso tra i primi size/2 con count_job maggiore. //TODO: per ora size/2, sarebbe meglio aggiungere dipendenza da N-size (N-size = numero worker ladri) o simile. 
                }
            }
            
            int get_count_all_job()const{
                int somma = 0;
                for (int i = 0; i<n_worker_; i++)
                    somma += count_job_[i].load(std::memory_order_acquire);
                return somma;
            }
            //per notificare tutte le CV_ dei worker
            void notifica_tutti()const{ //possibile const perche cv di singoli worker sono messe mutable
                for(int i=0; i<n_worker_; i++){
                    workers_[i]->cv_.notify_one();
                }
            }
            //per acquisire tutti i mutex dei worker
            std::vector<std::unique_lock<std::mutex>> lock_tutti()const{
                std::vector<std::unique_lock<std::mutex>> vett_lock;
                vett_lock.reserve(n_worker_);
                for(int i=0; i<n_worker_; i++){
                    std::unique_lock<std::mutex> loc_w(workers_[i]->m_);
                    vett_lock.push_back(std::move(loc_w));
                }
                return vett_lock;
            }
            //per rilasciare tutti i mutex dei worker
            void unlock_tutti(std::vector<std::unique_lock<std::mutex>>& vett_lock)const{
                for(int i=0; i<n_worker_; i++){
                    vett_lock[i].unlock();
                }
            }


           //TODO: tutti i send uguali cambia solo scelta indice, ma non si puo fare template<typename F, typename... Args, typename tipo_send> perche templeta parameter deduction è un tutto o niente, come fare allora per semplificare ?

           //lock di tutti i mutex cosi notifica a tutti sara sincronizzata con push e  count++, OSSERVAZIONE: lock di mutex non interrompe worker_loop di chi ha gia job in coda grazie a nuovo worker_loop con try_j all inizio di while 
           // non piu lock di tutti ma lock e notifica solo di chi riceve, piu efficiente e non è un problema perche il send non serve svegli per lo steal, steal deve avvenire in fase finale quando per qualche ragione un worker è rimasto indietro e qualcuno finisce prima, ma questo accade dopo send 

           //OSS:ora che i distruttore si aspetta esecuzioen di tutti i job non servono piu i future<void> per assivurare esecuzione di job, si potrebbe pensare di ritornare  aversioni conn return=void che nnon perdono tempo a creare vector<future<void>>, PERO' meglio lasciarli perche con future<void> in main con get si puo specificare entro quando vuoi che un job sia eseguito

           //send_task_mostfree
            template<typename F, typename... Args>
            auto send_task(F&& f,Args&&... args) -> std::optional<std::future<decltype(f(args...))>>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = std::forward<F>(f), ...args_catturati = std::forward<Args>(args) ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};

                int indx_worker = indx_most_free();
                std::unique_lock<std::mutex> lock(workers_[indx_worker]->get_loc()); // lock solo di worker a cui si manda job. altri non avranno lettura di count_job[indx_worker]++ sincronizzato, ma molto piu efficiente di lock tutti i mutex (i pro battono i contro) 
                bool flag = workers_[indx_worker]->push_back(j);
                if(flag){
                    count_job_[indx_worker].fetch_add(1,std::memory_order_release); // TODO: in realta dato che dentro mutex basta relax ancora piu efficente, però non tutte le letture avvengono dentro mutex quindi non saprei
                    lock.unlock();
                    workers_[indx_worker]->cv_.notify_one(); // problema: push e notifica sono sincronizati solo in thread su cui viene fatto push perche mutex che poi leggera è lo stesso bloccato in push.
                                //POSSIBILE SOLUZIONE: fare lock_all() e poi unlock_all() ma cosi ogni send blocca worker_loop di chi ancora non ha superato la cv_.wait(), pero sarebbe sincronizzata la lettura dei count_job ++. 
                    return fut;
                }
                lock.unlock();
                return std::nullopt;  //OSSERVAZIONE:return optional e non future cosi possibilita di fallire per push e non è necessario fare while(). spostato check se push e quindi send a buon fine fuori da threadpool perche usando hold queue per esempio non puo fallire il push e quindi ci sarebbe un while inutile              
            };

            //send a giro usando struct indxw             
            template<typename F, typename... Args>
            auto send_task_round(F&& f,Args&&... args) -> std::optional<std::future<decltype(f(args...))>>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = std::forward<F>(f), ...args_catturati = std::forward<Args>(args) ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};
            
                std::unique_lock<std::mutex> lock(workers_[indxw_.indx_]->get_loc());
                bool flag = workers_[indxw_.indx_]->push_back(j);
                if(flag){
                    count_job_[indxw_.indx_].fetch_add(1,std::memory_order_release);
                    indxw_.next(n_worker_); //dentro mutex per sincronizzazione visione (se solo un thread manda non necessario)
                    lock.unlock();
                    workers_[indxw_.indx_]->cv_.notify_one();// notifica solo a chi riceve e non notifica a tutti, perchè tanto cv manda a dormire quando non ci sono job e poi quando vengono inviati ogni worker riceve una notifica un job ogni $n_worker inviati, tanto basta 
                    return fut;
                }
                lock.unlock();
                return std::nullopt;
            };

            //send a sola meta di worker per debug/ test di steal job. 
            template<typename F, typename... Args>
            auto send_task_only_to_some(F&& f,Args&&... args) -> std::optional<std::future<decltype(f(args...))>>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = std::forward<F>(f), ...args_catturati = std::forward<Args>(args) ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};
            
                std::unique_lock<std::mutex> lock(workers_[indxw_.indx_]->get_loc());
                bool flag = workers_[indxw_.indx_]->push_back(j);
                if(flag){
                    count_job_[indxw_.indx_].fetch_add(1,std::memory_order_release);
                    if (n_worker_ != 1)
                        indxw_.next(n_worker_/2);
                    lock.unlock();
                    notifica_tutti(); //qui rimane notifica a tutti perchè alcuni worker non ricevono mai job e quindi non riceverebbero mai notifica
                    return fut;
                }
                lock.unlock();
                return std::nullopt;
            };

            //send sola a un worker (0 perche sicuro esiste sempre) per debug/ test di steal job. 
            template<typename F, typename... Args>
            auto send_task_only_to_zero(F&& f,Args&&... args) -> std::optional<std::future<decltype(f(args...))>>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = std::forward<F>(f), ...args_catturati = std::forward<Args>(args) ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};
            
                std::unique_lock<std::mutex> lock(workers_[indxw_.indx_]->get_loc());
                bool flag = workers_[0]->push_back(j);
                if(flag){
                    count_job_[indxw_.indx_].fetch_add(1,std::memory_order_release);
                    lock.unlock();
                    notifica_tutti();
                    return fut;
                }
                lock.unlock();
                return std::nullopt;
            };

            //parallel_for 
            //TODO: capire se necessario perfect forwarding-> si evita copie di fuzioi se passate come lambda rvalue

            //lasciato versione con body function con return perche magari parallelizzando salvare risultati in vettore non è thread-safe (bisognerebbe salvare in array cosi che non si riallochi la memoria, oppure usare thread safe vector)
            //ma non chiaro in realta probabimnte non serve 

            //F = body_function di loop, function con input indice i di loop. firma: return_type (int i)
            template<typename F> 
            requires (!std::is_same_v<std::invoke_result_t<F,int>, void>)
            auto parallel_for_sure(int start, int end, F&& f)-> std::vector<std::invoke_result_t<F, int>>{
                using return_type = std::invoke_result_t<F, int>;
                std::vector<std::future<return_type>> ret_fut;
                ret_fut.reserve(end-start);
                std::vector<return_type> ret;
                ret.reserve(end-start);
                int j = start;
                //while con controllo se job send, se non inviato elimina nullopt e non aggiorna j cosi da riprovare finche non lo invia
                //molto costoso ma necessario per avere certezza send all job
                //OSS: se push in syncro_queue fosse garantito (es push_or_wait di hold_wait version) non sarebbe necessario while(){if()} ma basterebbe for
                //TODO: scrivere diverso parallel_for a seconda di tipo coda in threadpool 
                while(j<end){
                    std::optional<std::future<return_type>> opt_fut= this->send_task_round(std::forward<F>(f),j);
                    if(opt_fut){
                        ret_fut.push_back(std::move(opt_fut.value()));
                        j++;
                    }
                }
                // get di tutti i future cosi da assicurarsi che tutte le body_function di ciclo for siano eseguite prima di termine funzione parallel_for
                // e copia di return di ogni body_function per return vector<return_type> di parallel_for
                for (size_t k= 0; k<ret_fut.size(); k++){
                    ret.push_back(ret_fut[k].get());
                }
                return ret;
            }

            
            //OVERLOAD per distiguere tipi di parallel_for
            // parallel_for(int,int,F&&) --> ogni iterazione diventa un job (n_block=range)
            // parallel_for(function<int(int)> incr, int, int, F&&) --> scorre range con incr personalizzato non piu solo i++ 
            // parallel_for(int,int,int n,F&&) --> divide range in n blocchi
            // parallel_for(int,int,vector<int>,F&&) --> divide range in vect.size() blocchi ognuno con numero iterazioni = vect[j] 

            //F = body_function di loop, function con input indice i di loop. firma: void (int i)
            template<typename F> 
            requires std::is_same_v<std::invoke_result_t<F,int>, void>
            void parallel_for_sure(int start, int end, F&& f){
                using return_type = std::invoke_result_t<F, int>; // sarebbe void
                std::vector<std::future<return_type>> ret_fut;
                ret_fut.reserve(end-start);
                int j = start;
                while(j<end){
                    std::optional<std::future<return_type>> opt_fut= this->send_task_round(std::forward<F>(f),j);
                    if(opt_fut){
                        ret_fut.push_back(std::move(opt_fut.value()));
                        j++;
                    }
                }
                
                for(std::future<void>& fut : ret_fut){
                    fut.get(); //OSS: parallelizzare i get()--> NON SI PUO, poi si dovrebbe fare get() dei get()
                }
                return;
            } 

            // per iterare con incremento di i personalizzato (es i+2  incr = [](int i){return i+2;}), ogni iterazioni un job
            template<typename F> 
            requires std::is_same_v<std::invoke_result_t<F,int>, void>
            void parallel_for_sure(std::function<int(int)> incr, int start, int end, F&& f){
                using return_type = std::invoke_result_t<F, int>; // sarebbe void
                std::vector<std::future<return_type>> ret_fut;
                ret_fut.reserve(end-start);
                int j = start;
                while(j<end){
                    std::optional<std::future<return_type>> opt_fut= this->send_task_round(std::forward<F>(f),j);
                    if(opt_fut){
                        ret_fut.push_back(std::move(opt_fut.value()));
                        j = incr(j);
                    }
                }
                
                for(std::future<void>& fut : ret_fut){
                    fut.get(); //OSS: parallelizzare i get()--> NON SI PUO, poi si dovrebbe fare get() dei get()
                }
                return;
            } 

            //versione che parallelizza dividendo il range iniziale in n blocchi: n = numero job inviati a threadpool.
            //ridurre i send, job() non sono l' esecuzione della singola body function f(i) ma sono for(k in subset_of_({start-end})){f(k)}
            template<typename F> 
            requires std::is_same_v<std::invoke_result_t<F,int>, void>
            void parallel_for_sure(int start, int end,int n, F&& f){
                using return_type = std::invoke_result_t<F, int>;
                //range va da start a end-1--> end-start= dimensione range
                int range = (end-start);
                int n_body_fun = range / n; //numero body_function in ogni blocco(job) 
                std::vector<std::future<return_type>> ret_fut; //no optinal<future> perché se nullopt non pushato quindi solo future
                ret_fut.reserve(n+1); //per evitare riallocameto memoria, +1 per eventuale ultimo job fatto da ultime (end-start)%n iterazioni  
                int j = 0;
                while(j< n){
                    std::optional<std::future<return_type>> opt_fut = this->send_task_round([&,j,fun = std::forward<F>(f)](){ //j catturato come copia perchè modificato detro job (j+1) quidi se catturi come reference si sballa tutto !!!
                        for(int k=j*n_body_fun; k<(j+1)*n_body_fun; k++ ){
                            fun(k);
                        }
                    });
                    if(opt_fut){
                        //se send andato a buon push di fut in ret_fut e incrementa j
                        ret_fut.push_back(std::move(opt_fut.value())); //move perche future non copiabili
                        j++;
                    }
                }
                if(range % n != 0){ //inviamo ultimo job con iterazioni rimanenti 
                    j=0;
                    while(j<1){
                        std::optional<std::future<return_type>> opt_fut = this->send_task_round([&,j,fun = std::forward<F>(f)](){ //j gia catturata in & credo non serve
                        for(int k=n*n_body_fun; k<(end-start); k++ ){
                            fun(k);
                        }
                    });
                    if(opt_fut){
                        //se send andato a buon push di fut in ret_fut e incrementa j
                        ret_fut.push_back(std::move(opt_fut.value())); //move perche future non copiabili
                        j++;
                    }
                    }
                }
                //get dei future void per assicurarsi che tutti i job siano stati eseguiti dopo uso parallel_for in main
                for(std::future<void>& fut : ret_fut){
                    fut.get();
                }
                return;
        
            } 

            //riceve vettore per far scegliere a utente come suddividere le iterazioni nei blocchi 
            // es vect=[1,5,5,4] for(0,15)  lo divide in 4 blocchi primo 1 it, secondo 5 it ecc...
            //utile se si conosce gia sbilanciamento in iterazioni di for
            template<typename F> 
            requires std::is_same_v<std::invoke_result_t<F,int>, void>
            void parallel_for_sure(int start, int end,std::vector<int>& vect, F&& f){
                using return_type = std::invoke_result_t<F, int>;
                //range va da start a end-1--> end-start= dimensione range
                if(std::reduce(vect.cbegin(),vect.cend(),0) != (end-start)){
                    std::cerr<<"somma di elem in vect deve essere uguale a range (end-start)"<<std::endl;
                    return;
                }
                std::vector<int> seq={0}; //seq sara vettore di somme parziali (es np.cumsum) con primo elemento pero 0 cosi da pterlo usare in divisione di for piu comodamente
                int sum_seq = 0;
                size_t vect_size = vect.size();
                for(size_t l = 0; l<vect_size; l++){
                    sum_seq += vect[l];
                    seq.push_back(sum_seq);
                }
                std::vector<std::future<return_type>> ret_fut;
                ret_fut.reserve(vect_size);
                size_t j = 0;
                while(j< vect_size){
                    std::optional<std::future<return_type>> opt_fut = this->send_task_round([&,j,fun = std::forward<F>(f)](){ //j gia catturata in & credo non serve
                        for(int k=seq[j]; k<seq[j+1]; k++ ){
                            fun(k);
                        }
                    });
                    if(opt_fut){
                        ret_fut.push_back(std::move(opt_fut.value()));
                        j++;
                    }
                }
            
                for(std::future<void>& fut : ret_fut){
                    fut.get();
                }
                return;
            } 



            //parallel reduce con funzioni dipendenti da j
            template<typename F>
            auto parallel_reduce_sum(int start, int end, F&& f)-> std::invoke_result_t<F, int>{
                using return_type = std::invoke_result_t<F, int>;

                std::vector<return_type> results = parallel_for_sure(start,end,std::forward<F>(f));

                return_type ret = results[0];
                                
                for(auto it = results.begin()+1; it < results.end(); it++){
                    ret += *it;
                }
                return ret;
            }
            //parallel reduce con funzioni dipendenti da j
            template<typename F>
            auto parallel_reduce_dot(int start, int end, F&& f)-> std::invoke_result_t<F, int>{
                using return_type = std::invoke_result_t<F, int>;

                std::vector<return_type> results = parallel_for_sure(start,end,std::forward<F>(f));

                return_type ret = results[0];

                for(auto it = results.begin()+1; it < results.end(); it++){
                        ret *= *it;
                }
                      
                return ret;
            }
            
        };

        





        
        //threadpool senza steal job
        class Threadpool_nosteal{
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
                    friend class Threadpool_nosteal; //TODO: classi friend quindi inutili tuti i getter ecc.. ripulire codice. lasciare magari wrap di alcune funzioni per chiarezza in uso in threadpool(es workers_[i]->notifica() al posto di workers_[i]->cv_.notify_one())
                    // costruttore con numero elementi di coda
                    //inizializza thread con funzione membro worker_loop di Threadpoool cosi che accessibili altre code senza passaggio di puntatore/reference a threadpool in worker che causava segmentation fault  
                    Worker(int n, void (Threadpool_nosteal::*worker_loop)(int),Threadpool_nosteal* Th, int idx):indx_(idx),sync_queue_(n),t_(worker_loop,Th,idx){}; //oss non serve <N> compila e funziona lo stesso ma non so perche

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
            indx_worker indxw_; 
            std::shared_mutex m_threadpool_; 
            std::condition_variable_any cv_threadpool_;
            bool active_ = false; //TODO oss: possibile evitare stop_ di singolo worker e usare active, ma stop_ in singolo worker rende worker indipendente da threadpool (puo terminare anche se threadpool non distrutta) e questo puo essere utile magari in seguito 
        public:
            friend class Worker; //poi togliere tuti get e sostituire con accesso diretto
            //n = size code, k = numero worker
            Threadpool_nosteal(int n,int k):n_worker_(k),queue_size_(n){
                //non serve aspettare che tuuti siano inizializzati perche tanto non c'è possibilita segmentation fault dovuta ad accesso a worker non inizializzati perche non c'è steal
                workers_.reserve(k);
                for(int i=0; i<k; i++){
                    workers_.emplace_back(std::make_shared<Worker> (n,&Threadpool_nosteal::worker_loop,this,i));
                    count_job_.emplace_back(0);
                }
            };
            //default constructor
            Threadpool_nosteal(){
                n_worker_ = std::thread::hardware_concurrency(); //OSS: forse n_worker_ = std::thread::hardware_concurrency()-1; perche un thread è quello del main
                queue_size_ = 256*n_worker_;
                workers_.reserve(n_worker_);
                for(int i=0; i<n_worker_; i++){
                    workers_.emplace_back(std::make_shared<Worker> (queue_size_,&Threadpool_nosteal::worker_loop,this,i));
                    count_job_.emplace_back(0);
                }
            }
            //constructor default numero di worker ma specifica size code
            Threadpool_nosteal(int n):queue_size_(n){
                std::unique_lock<std::shared_mutex> loc(m_threadpool_);
                n_worker_ = std::thread::hardware_concurrency(); //OSS: forse n_worker_ = std::thread::hardware_concurrency()-1; perche un thread è quello del main
                workers_.reserve(n_worker_);
                for(int i=0; i<n_worker_; i++){
                    workers_.emplace_back(std::make_shared<Worker> (queue_size_,&Threadpool_nosteal::worker_loop,this,i));
                    count_job_.emplace_back(0);
                }
                active_ = true;
                loc.unlock();
                cv_threadpool_.notify_all();
            }
            ~Threadpool_nosteal(){
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
                std::shared_lock<std::shared_mutex> lock_shared(m_threadpool_);
                cv_threadpool_.wait(lock_shared,[this](){return active_;}); //cv.wait(loc) quando ritenta a verificare condition dopo notifica fa loc.lock() (chiama membro lock() di loc) e con loc=shared_lock<std::shared_mutex> il membro lock() è shared_lock(), prima con passaggio di loc= shared_mutex il membro lock() era lock() (cioe blocco in modalita scrittura) :()
                lock_shared.unlock();
                while(!workers_[i]->stop_){
                    std::optional<job> try_j = workers_[i]->pop_front();
                    if(try_j){//esegue se non è nullopt
                        count_job_[i].fetch_sub(1,std::memory_order_release);
                        (try_j.value())();
                    }
                    else{
                        std::unique_lock<std::mutex> loc(workers_[i]->m_);
                        workers_[i]->cv_.wait(loc,[&](){return count_job_[i]>0  || workers_[i]->stop_ ;}); 
                        loc.unlock();
                        if(workers_[i]->stop_){return;}  
                        try_j = workers_[i]->pop_front();
                        if(try_j){//esegue se non è nullopt
                            count_job_[i].fetch_sub(1,std::memory_order_release);
                            (try_j.value())();
                        }
                    }
                }
            };
            
            int get_count_all_job(){
                int somma = 0;
                for (int i = 0; i<n_worker_; i++)
                    somma += count_job_[i].load(std::memory_order_acquire);
                return somma;
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

        
          
            template<typename F, typename... Args>
            auto send_task(F&& f,Args... args) -> std::optional<std::future<decltype(f(args...))>>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = std::forward<F>(f), ...args_catturati = std::forward<Args>(args) ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};

                int indx_worker = indx_most_free();
                std::unique_lock<std::mutex> lock(workers_[indx_worker]->get_loc());
                bool flag = workers_[indx_worker]->push_back(j);
                workers_[indx_worker]->notifica();
                if(flag){
                    count_job_[indx_worker].fetch_add(1,std::memory_order_release); // TODO: in realta dato che dentro mutex basta relax ancora piu efficente, però non tutte le letture avvengono dentro mutex quindi non saprei
                    lock.unlock();
                    return fut;
                }
                lock.unlock();
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
            
                std::unique_lock<std::mutex> lock(workers_[indxw_.indx_]->get_loc());
                bool flag = workers_[indxw_.indx_]->push_back(j);
                workers_[indxw_.indx_]->notifica();
                if(flag){
                    count_job_[indxw_.indx_].fetch_add(1,std::memory_order_release);
                    indxw_.next(n_worker_); //dentro mutex per sincronizzazione visione (se solo un thread manda non necessario)
                    lock.unlock();
                    return fut;
                }
                lock.unlock();
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

            //OSS: probabilmente inutile perche basterebbe catturare container in lamda nel main e poi nella lambda usare accesso tramite i
            //     ES: [& V](int i){ ...V[i]...}
            //     OSS: unico pro di scorrere con iteartor direttamente è che evita di dover catturare ref a V.
            //PARALLEL_FOR_ITERATOR
            //parallel_for_iterator: for_loop scorre cotainer con iterator it e applica funzione con tutti elementi di container f(*it)
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

            //parallel_for_iterator_ref: for_loop scorre cotainer con iterator it e applica funzione con tutti elementi di container tramite reference f(std::ref(*it))
            template<typename Iterator, typename F> 
            //TODO: da aggiugere vicolo che iteartor abbia membro value_type
            auto parallel_for_iterator_ref(Iterator begin, Iterator end,F&& f)->std::vector<std::optional<std::future<std::invoke_result_t<F,typename Iterator::value_type&>>>>{
                using return_type = std::invoke_result_t<F,typename Iterator::value_type&>;
                std::vector<std::optional<std::future<return_type>>> ret;
                for (auto it = begin; it!=end; it++){
                    ret.push_back(this->send_task_round(std::forward<F>(f),std::ref(*it)));
                }
                return ret;
            }
        };
    }
#endif


