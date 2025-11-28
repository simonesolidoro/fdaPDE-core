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

    template <steal T> class threadpool{
        using job = std::function<void()>;
        
        struct indx_worker{
            //OSS: not thread-safe because only main thread send job, if future develop like fork-join (jobs send jobs) add mutex.
            int indx_ = 0;
            void next(int n_worker){
                (indx_ == n_worker-1)? (indx_ = 0):(indx_++);
            };
        };
        public:
            class worker{
                private:
                    int indx_; 
                    fdapde::synchro_queue<job,fdapde::relaxed> sync_queue_;
                    std::thread t_;
                    bool stop_ = false;
                    mutable std::mutex m_;
                    mutable std::condition_variable cv_; 
                public:
                    friend class threadpool; 
                    worker(int n, void (threadpool::*worker_loop)(int),threadpool* Th, int idx):indx_(idx),sync_queue_(n),t_(worker_loop,Th,idx){}; 

                    //avoidable wraps, but use to make code clearer
                    std::unique_lock<std::mutex> get_loc()const{
                        std::unique_lock<std::mutex> loc(m_);
                        return loc;          
                    }
                    void notifica()const{cv_.notify_one();}
                    void set_stop(bool s){stop_ = s;}
                    void join_thread(){t_.join();} 
                    bool push_front(job& fun){return sync_queue_.push_front(fun);};//reference in input because is only a wrap-function (intermediate step), than synchro_queue store a copy 
                    bool push_back(job& fun){return sync_queue_.push_back(fun);};
                    std::optional<job> pop_front(){return sync_queue_.pop_front();};
                    std::optional<job> pop_back(){return sync_queue_.pop_back();};
            };
        private:
            std::vector<std::shared_ptr<worker>> workers_; //vector of pointers because worker not movable, not copiable
            std::deque<std::atomic<int>> count_job_; //deque because atomic<int> not movable 
            int n_worker_ ;
            int queue_size_; 
            indx_worker indxw_; 
            mutable std::shared_mutex m_threadpool_; //mutable per tenere const get_indx_from_worker()
            std::condition_variable_any cv_threadpool_;
            bool active_ = false;
            mutable std::mt19937 gen_;
            std::unordered_map<std::thread::id, int> map_thread_worker_; //map of : thread_ID of worker's thread, index of worker in workers_
            mutable std::shared_mutex m_map_;
        public:
            friend class worker; 
            //n = size_synchro_queue, k = number of workers
            threadpool(int size_queue,int n_worker):n_worker_(n_worker),queue_size_(size_queue),gen_(std::random_device{}()){
                std::unique_lock<std::shared_mutex> loc(m_threadpool_);
                workers_.reserve(n_worker);
                for(int i=0; i<n_worker; i++){
                    workers_.emplace_back(std::make_shared<worker> (size_queue,&threadpool::worker_loop,this,i));
                    count_job_.emplace_back(0);
                }
                active_ = true;
                loc.unlock();
                cv_threadpool_.notify_all();
            };

            //constructor default number of worker 
            threadpool(int size_queue): threadpool(size_queue, std::thread::hardware_concurrency()){}

            //default constructor
            threadpool(): threadpool(256){}

            ~threadpool(){
                while(get_count_all_job()>0){
                    std::this_thread::yield(); 
                }; 
                for (int i =0; i<n_worker_; i++){
                    while(!workers_[i]->sync_queue_.empty()){}
                }
                //let's finish all worker_loop
                for(int j = 0; j<n_worker_; j++){
                    std::unique_lock<std::mutex> loc(workers_[j]->get_loc());
                    workers_[j]->set_stop(true);
                    workers_[j]->notifica();
                }
                for(int j = 0; j<n_worker_; j++){
                    workers_[j]->join_thread();
                }
            }

            //getter
            int get_n_worker()const{return n_worker_;};
            int get_index_worker_from_thread()const{
                std::shared_lock<std::shared_mutex> loc(m_map_); //aggiunto lock perché non saprei che altro possa causare errore out of range in cluster  
                std::thread::id id = std::this_thread::get_id(); 
                try {
                     return map_thread_worker_.at(id);
                    }
                catch(const std::out_of_range& e){
                    std::cerr << "il Thread con id: " << id << " non è stato trovato in map_thread_worker_ " << std::endl;
                    std::cerr<< "elenco dei (in teoria) " <<n_worker_<<" worker presenti in threadpool, thrad_id e indx: "<<std::endl;
                    for(auto it = map_thread_worker_.begin() ; it != map_thread_worker_.end(); it++ ){
                        std::cerr<<"id : "<<it->first<<"; indx: "<<it->second<<std::endl;
                    } 
                    std::terminate(); // termina tutto immediatamente 
                    }
                //return map_thread_worker_.at(std::this_thread::get_id());
            }

            // indx = index of the worker from whom the job j was taken
            bool try_do(std::optional<job> j, int indx){ 
                if(j){
                    (j.value())(); 
                    count_job_[indx].fetch_sub(1,std::memory_order_release); 
                    return true;
                }
                return false;
            }

            void worker_loop(int i){// i = index of worker in workers_

                std::shared_lock<std::shared_mutex> lock_shared(m_threadpool_);
                cv_threadpool_.wait(lock_shared,[this](){return active_;}); 
                lock_shared.unlock();

                
                //upload map_thread_worker thread-safe. //shared mutex in modalità scrittura, così in metodo che deve solo leggere usato in modalità lettura tra tutti i worker che quindi una volta superata questa fase di inizializzazione della mappa hanno comportamento non-blocking (ovviamente intendo solo per lettura mappa )
                std::unique_lock<std::shared_mutex> loc_map(m_map_);
                map_thread_worker_[std::this_thread::get_id()] = i;
                loc_map.unlock();

                bool done_own_job = true; 
                int indx_steal = -1; //index to steal from
                while(!workers_[i]->stop_){
                    done_own_job = try_do(workers_[i]->pop_front(),i);
                    if(done_own_job){
                        continue; //avoid lock mutex in the end 
                    }
                    //try steal
                    if constexpr(T == steal::random){ indx_steal = indx_random_from_busy();}
                    if constexpr(T == steal::most_busy){ indx_steal = indx_most_busy();}
                    if constexpr(T == steal::random_half_most_busy){ indx_steal = indx_random_from_half_most_busy();} 
                    if( indx_steal != -1){ 
                        try_do(workers_[indx_steal]->pop_back(),indx_steal);
                        continue;
                    }
                    //chek for job whit mutex lock, eventualy go to sleep
                    std::unique_lock<std::mutex> loc(workers_[i]->m_); 
                    workers_[i]->cv_.wait(loc,[&](){return count_job_[i].load(std::memory_order_acquire) > 0 || get_count_all_job()>0 || workers_[i]->stop_;});  
                    loc.unlock();
                    if(workers_[i]->stop_){return;}
                }
            };

            int indx_most_free()const{
                int worker_indx = 0;
                int min_elem= count_job_[0].load(std::memory_order_acquire);
                int current_el = 0;  
                for (int j=1; j<n_worker_; j++){
                    current_el = count_job_[j].load(std::memory_order_acquire);
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
                int current_el = 0;
                for (int j=1; j<n_worker_; j++){
                    current_el = count_job_[j].load(std::memory_order_acquire);
                    if(current_el > max_elem ){
                        worker_indx = j;
                        max_elem = current_el;
                    }
                }
                if(max_elem == 0){return -1;} //avoid steal of nullopt 
                return worker_indx;
            }
            // return random int in  [0, size-1]
            int random_int(int size)const{
                std::uniform_int_distribution<> distrib(0,size-1);
                return distrib(gen_);
            }

            int indx_random_from_busy()const{
                std::vector<int> indxs; //vector of index of worker busy
                for(int k = 0; k<n_worker_; k++){
                    // oss: not avoid himself, to avoid extra if() or split range(worker k find in (0,...,k-1,k+1,...,n_worker-1))
                    if(count_job_[k].load(std::memory_order_acquire) > 0){
                        indxs.push_back(k);
                    }
                }
                int size = indxs.size();
                switch(size){ //avoid random gen_eration if not need it
                case 0: 
                    return -1;
                case 1:
                    return indxs[0];
                default:
                    return indxs[random_int(size)];   
                }                
            }

            int indx_random_from_half_most_busy()const{ 
                std::vector<std::pair<int,int>> indxs; //vector of pair: (index worker busy, number of job in queue)
                int tmp_count_job = 0;
                for(int k = 0; k<n_worker_; k++){
                    tmp_count_job = count_job_[k].load(std::memory_order_acquire);
                    if(tmp_count_job > 0){
                        indxs.emplace_back(k,tmp_count_job);
                    }
                }
                int size = indxs.size();
                if(size == 0) {return -1;}
                //sort from mostbusy to least
                std::sort(indxs.begin(), indxs.end(), [](std::pair<int,int> &a, std::pair<int,int> &b) {return a.second > b.second;}); 
                switch (size)
                {
                case 1:
                    return indxs[0].first;
                case 2:
                    return indxs[0].first;
                case 3:
                    return indxs[0].first;
                default: 
                    return indxs[random_int(size/2)].first;  
                }
            }
            
            int get_count_all_job()const{
                int somma = 0;
                for (int i = 0; i<n_worker_; i++)
                    somma += count_job_[i].load(std::memory_order_acquire);
                return somma;
            }


           //send_task_mostfree
            template<typename F, typename... Args>
            auto send_task_mostfree_weak(F&& f,Args&&... args) -> std::optional<std::future<decltype(f(args...))>>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = f, ...args_catturati = args ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};

                int indx_worker = indx_most_free();
                std::unique_lock<std::mutex> lock(workers_[indx_worker]->get_loc()); 
                bool flag = workers_[indx_worker]->push_back(j);
                if(flag){
                    count_job_[indx_worker].fetch_add(1,std::memory_order_release); 
                    lock.unlock();
                    workers_[indx_worker]->cv_.notify_one();  
                    return fut;
                }
                lock.unlock();
                return std::nullopt; 
            };
            //send mostbusy, for sure (while-loop as long as push ends successfully)
            template<typename F, typename... Args>
            auto send_task_mostfree(F&& f,Args&&... args) -> std::future<decltype(f(args...))>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = f, ...args_catturati = args ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};

                int indx_worker = indx_most_free();
                std::unique_lock<std::mutex> lock(workers_[indx_worker]->get_loc()); 
                bool flag = workers_[indx_worker]->push_back(j);
                while(!flag){ flag = workers_[indx_worker]->push_back(j);}
                count_job_[indx_worker].fetch_add(1,std::memory_order_release); 
                lock.unlock();
                workers_[indx_worker]->cv_.notify_one();  
                return fut;
            };

            //send round using struct indxw             
            template<typename F, typename... Args>
            auto send_task_round_weak(F&& f,Args&&... args) -> std::optional<std::future<decltype(f(args...))>>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = f, ...args_catturati = args ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};
            
                std::unique_lock<std::mutex> lock(workers_[indxw_.indx_]->get_loc());
                bool flag = workers_[indxw_.indx_]->push_back(j);
                if(flag){
                    count_job_[indxw_.indx_].fetch_add(1,std::memory_order_release);
                    lock.unlock();
                    indxw_.next(n_worker_); 
                    workers_[indxw_.indx_]->cv_.notify_one();
                    return fut;
                }
                lock.unlock();
                return std::nullopt;
            };

            //send round using struct indxw, for sure (while-loop as long as push ends successfully)             
            template<typename F, typename... Args>
            auto send_task_round(F&& f,Args&&... args) -> std::future<decltype(f(args...))>{
                //wrap 
                using return_type = decltype(f(args...));
                std::shared_ptr<std::packaged_task<return_type()>> ptr_task = std::make_shared<std::packaged_task<return_type()>> ([fun = f, ...args_catturati = args ]()mutable{return fun(args_catturati...);});
                std::future<return_type> fut = ptr_task->get_future();
                job j = [ptr_task](){(*ptr_task)();};
            
                std::unique_lock<std::mutex> lock(workers_[indxw_.indx_]->get_loc());
                bool flag = workers_[indxw_.indx_]->push_back(j);
                while(!flag){flag = workers_[indxw_.indx_]->push_back(j);}
                count_job_[indxw_.indx_].fetch_add(1,std::memory_order_release);
                lock.unlock();
                indxw_.next(n_worker_); 
                workers_[indxw_.indx_]->cv_.notify_one();
                return fut;
            };

            
/*//only for test logic of steal.
            //per notificare tutte le CV_ dei worker
            void notifica_tutti()const{ //possibile const perche cv di singoli worker sono messe mutable
                for(int i=0; i<n_worker_; i++){
                    workers_[i]->cv_.notify_one();
                }
            }
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
            //only for test logic of steal
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
*/
            struct info_par{
                int n_job;
                int granularity;
                int granularity_last;
            };
            info_par info_parallel (int range, int jpw){
                info_par ret;
                int tot_job = jpw * n_worker_;
                ret.granularity = std::max(range / tot_job,1);
                ret.granularity_last = ret.granularity; //poi aggiornata se c'è resto di divisione
                ret.n_job = range / ret.granularity; //cosi sicuro lasta job ha granularity miniore di granularity, però non più max jpw job per worker ma circa jpw job per worker. es: range=37, n_worker=10, jpw=1--> tot_job = 10, gran = 37/10 = 3, n_job=37/3 = 12 poi +1 per resto, quindi tutti worker 1 job, tranne primi 3 che ne hanno 2
                // if(n_job % n_worker != 0){
                //     granularity_last = range%granularity;
                //     n_job ++;
                // }else{
                //     //spalma
                //     /* bisognerebbe dare info che primi  range%granularity  job devono avere granularity+1
                //         non è comodo da usare come info, quindi lascerei perdere questa ottimizzazione e 
                //         farei sempre granularity ultimo job =  range%granularity a prescindere se i worker hanno o meno già tutti lo stesso numero di job */
                // }
                if(range%ret.granularity != 0){
                    ret.granularity_last = range%ret.granularity;
                    ret.n_job++; 
                }
                return ret;
            }

            //parallel_for //TODO: controllo start<end va aggiunto ?
            //OVERLOAD:
            // parallel_for(int,int,F&&) --> granularity = 1
            // parallel_for(int,int,F&&,function<int(int)> incr) --> custom increment to scroll range, granularity = 1
            // parallel_for(int,int,F&&,granularity) --> parallel_for whit granularity in input. if granularuty == -1 use defaul-granularity. default-granularity is s.t. every worker receives about 1 job 
            // parallel_for(int,int,F&&,vector<int>) --> divide range in vect.size() block, granularity of block j = vect[j]

            //F = body_function, firm of F: void (int i)
            template<typename F> 
            requires std::is_same_v<std::invoke_result_t<F,int>, void>
            void parallel_for(int start, int end, F&& f){
                using return_type = void; 
                std::vector<std::future<return_type>> ret_fut;
                ret_fut.reserve(end-start);
                for (int j = start; j<end; j++){
                    ret_fut.emplace_back(this->send_task_round(f,j));
                }
                for(std::future<void>& fut : ret_fut){
                    fut.get(); 
                }
                return;
            } 
            
            template<typename F> 
            requires std::is_same_v<std::invoke_result_t<F,int>, void>
            void parallel_for(int start, int end, F&& f, std::function<int(int)> incr){
                using return_type = void; 
                std::vector<std::future<return_type>> ret_fut;
                ret_fut.reserve(end-start);
                for (int j = start; j<end; j = incr(j)){
                    ret_fut.emplace_back(this->send_task_round(f,j));
                }
                for(std::future<void>& fut : ret_fut){
                    fut.get(); 
                }
                return;
            } 

/*vecchio parallel_for con granularity in input che spalma ma che non ha tmp_obj come variadic così da crearne uno solo per job */
            // template<typename F> 
            // requires std::is_same_v<std::invoke_result_t<F,int>, void> 
            // void parallel_for(int start, int end, F&& f,int granularity){  
            //     using return_type = void;
            //     //range: [start, end) --> end-start= dim range
            //     int range = (end-start);
            //     if(granularity > range){granularity = range;}////oss: se granularity > range allora tutto range fatto da unico worker, messo granularity=range per evitare errori poi. magari mettere un warning ??? 
            //     int n_job = range / std::max(1,granularity); //se gran non valida (= 0) non da errore
            //     //default gran se mandato valore non valido, non più solo -1. cosi evitiamo controllo se granularity valida          
            //     if(granularity <= 0) {// 1 job per worker max (quindi resto spalmato), se range<n_worker allora n_job = range 
            //         if(range<n_worker_){
            //             granularity = 1;
            //             n_job = range;
            //         }else{
            //             granularity = range / n_worker_;
            //             n_job = n_worker_;
            //         }
            //     }
            //     int gran_last = granularity;
            //     int plus_one = 0;
            //     int resto = range%granularity; 
            //     if((n_job%n_worker_) == 0 && resto >0){ // spalma perché fare un ultimo job con iterazioni di resto sbilancia
            //         plus_one = resto;
            //     }
            //     if((n_job%n_worker_) != 0 && resto >0){ // ultimo job contiente resto di iterazioni (non spalmate perché c'é (almeno 1) worker che ha 1 job meno di worker0, e quindi le da a lui)
            //         gran_last = resto;
            //         n_job ++;
            //     }
            //     std::vector<std::future<return_type>> ret_fut;
            //     ret_fut.reserve(n_job); 
            //     // se non ha spalmato allora plus_one == 0 e questo for lo salta
            //     for (int j= 0; j<plus_one; j++){
            //         ret_fut.emplace_back(this->send_task_round([granularity = granularity +1,j,start,fun = f]()mutable{ 
            //                 int stop = (j+1)*granularity+start;
            //                 for(int k=j*granularity+start; k<stop; k++ ){
            //                     fun(k);
            //                 }
            //             }));
            //     }
            //     for (int j= plus_one; j<n_job-1; j++){
            //         ret_fut.emplace_back(this->send_task_round([granularity,plus_one,j,start,fun = f]()mutable{ 
            //                 int stop = (j+1)*granularity+plus_one+start;
            //                 for(int k=j*granularity+plus_one+start; k<stop; k++ ){
            //                     fun(k);
            //                 }
            //             }));
            //     }
            //     //last job (puo essere o gran_last o granularity normale) inviato separatamente per non dover fare if
            //     ret_fut.emplace_back(this->send_task_round([gran_last,end,fun = f]()mutable{ 
            //             for(int k=end-gran_last; k<end; k++ ){
            //                 fun(k);
            //             }
            //         }));
                
            //     //get futures
            //     for(std::future<void>& fut : ret_fut){fut.get();}
            //     return;
            // } 


            /*non so se va lasciato*/
            template<typename F, typename... Args> 
            requires std::is_same_v<std::invoke_result_t<F,int,int,Args&...>, void> && (! std::is_reference_v<Args> && ...)
            void parallel_for(int start, int end, F&& f,std::vector<int> granularities, Args... args){ //copia di vettore gran è più sicuro in multithreading
                using return_type = void;
                if(std::reduce(granularities.cbegin(),granularities.cend(),0) != (end-start)){
                    std::cerr<<"somma di elem in granularities deve essere uguale a range (end-start)"<<std::endl;
                    return;
                }
                std::vector<int> seq={start}; //seq vettore di somme parziali (es np.cumsum) con primo elemento però 0 cosi da pterlo usare in divisione di for piu comodamente
                int sum_seq = start;
                size_t vect_size = granularities.size();
                for(size_t l = 0; l<vect_size; l++){
                    sum_seq += granularities[l];
                    seq.push_back(sum_seq);
                }
                std::vector<std::future<return_type>> ret_fut;
                ret_fut.reserve(vect_size);
                for (size_t j = 0; j<vect_size; j++){
                    ret_fut.emplace_back(this->send_task_round([&seq,j,fun = f, ... args = args, this]()mutable{
                            int index_worker = this->get_index_worker_from_thread();
                            for(int k=seq[j]; k<seq[j+1]; k++ ){
                                fun(k, index_worker, args...);
                            }
                        }));
                }            
                for(std::future<void>& fut : ret_fut){
                    fut.get();
                }
                return;
            } 


            //prova di parallel_for cono variadic template di tmp object per ogni job, per poter usare parallel_for con granularity in input al posto di parallel_for gran=1 con divisione in job a mano
            template<typename F, typename... Args> 
            requires std::is_same_v<std::invoke_result_t<F,int,int,Args&...>, void> && (! std::is_reference_v<Args> && ...)//F in input ha: int i (loop index), index worker,  reference a  Args (tmp ogetti copie per ogni job), però Args non reference perché in lambda che viene inviata alla threadpool si deve copiare oggetto non reference ad oggetto
            //TODO: i require che Args non siano reference e che F prenda in input reference di args non funzionano, da capire come forzarli.
            //TODO: capire come forzare F a prende in input solo reference di Args, perché se body_function prede in input copia ricrea tmp ad ogni iterazione e siamo punto e a capo. vogliamo un solo tmp per job e body_function che prendere reference a copia tmp di job
            void parallel_for(int start, int end, F&& f, int granularity, Args... args){//n universal reference per Args perché ne voglio una copia per ogni job
                using return_type = void;
                //range: [start, end) --> end-start= dim range
                int range = (end-start);
                if(granularity > range){granularity = range;}////oss: se granularity > range allora tutto range fatto da unico worker, messo granularity=range per evitare errori poi. magari mettere un warning ??? 
                int n_job = range / std::max(1,granularity); //se gran non valida (= 0) non da errore
                //default gran se mandato valore non valido, non più solo -1. cosi evitiamo controllo se granularity valida          
                if(granularity <= 0) {// 1 job per worker max (quindi resto spalmato), se range<n_worker allora n_job = range 
                    if(range<n_worker_){
                        granularity = 1;
                        n_job = range;
                    }else{
                        granularity = range / n_worker_;
                        n_job = n_worker_;
                    }
                }
                std::vector<int> it_add; // vettore di iterazioni aggiuntive al primo job di ogni worker
                int resto = range%granularity; 
                if(resto >0){
                    if((n_job%n_worker_) != 0){
                        n_job ++;    
                    }
                    else{// spalma perché fare un ultimo job con iterazioni di resto sbilancia
                        int iter_add = resto / n_worker_;
                        int resto_di_resto = resto % n_worker_;// tutti i worker si prendono iter_add iterazioni extra e quello che rimane dato +1 ai primi resto_di_resto worker
                        for(int w = 0; w< resto_di_resto; w++){
                            it_add.push_back(iter_add + 1);
                        }
                        for(int w = resto_di_resto; w<n_worker_ ; w++){
                            it_add.push_back(iter_add);
                        }
                    } 
                }
                std::vector<std::future<return_type>> ret_fut;
                ret_fut.reserve(n_job); 
                // se non ha spalmato allora it_add.size() == 0 e questo for lo salta
                // primi job con eventuale itrazioni aggiuntive rispetto granularity
                int stop_local = start;
                for (int j= 0; j<it_add.size(); j++){
                    stop_local += (granularity+it_add[j]);
                    ret_fut.emplace_back(this->send_task_round([start = start,stop = stop_local,j,fun = f, ... args = args, this]()mutable{ 
                            int index_worker = this->get_index_worker_from_thread();
                            for(int k=start; k<stop; k++ ){
                                fun(k,index_worker,args...);//f deve prendere in input reference a variadic template altrimenti tutto inutile
                            }
                        }));
                    start = stop_local;//job successivo parte da fine di precedente
                }
                if(it_add.size()==n_job){//per evitare di arrivare a send di last job con gran_last.  
                    //get futures
                    for(std::future<void>& fut : ret_fut){fut.get();}
                    return;
                }
                for (int j= it_add.size(); j<n_job-1; j++){
                    stop_local = granularity+start;
                    ret_fut.emplace_back(this->send_task_round([start,stop = stop_local,fun = f, ... args = args, this]()mutable{// usato resto_spalmato perché magari c'è resto ma non viene spalmato eviene inserito in ultimo job e quindi qui le iterazioni vanno da j*granularity+start a (j+1)*granularity+start 
                            int index_worker = this->get_index_worker_from_thread();
                            for(int k=start; k<stop; k++ ){
                                fun(k,index_worker,args...);
                            }
                        }));
                    start = stop_local;
                }
                //last job (puo essere o gran_last o granularity normale) inviato separatamente per non dover fare if
                ret_fut.emplace_back(this->send_task_round([start,end,fun = f, ... args = args, this]()mutable{ 
                        int index_worker = this->get_index_worker_from_thread();
                        for(int k=start; k<end; k++ ){
                            fun(k,index_worker, args...);
                        }
                    }));
                
                //get futures
                for(std::future<void>& fut : ret_fut){fut.get();}
                return;
            } 

            //PARALLEL_FOR_ITERATOR:
            //body_function f = void(iterator)
            //1)void parallel_for_iterator(It start, It end, F&& f)                     per tutti i container, =1 default
            //2)void parallel_for_iterator(It start, It end, F&& f,int granularity)    per random acces container,  come input
            //3)void parallel_for_iterator(It start, It end, F&& f,int granularity)    per NON random acces container,  come input

            //1
            template<typename F,typename It> 
            requires std::is_same_v<std::invoke_result_t<F,It>, void> && (std::random_access_iterator<It> || (!std::random_access_iterator<It>)) //sicuro c'è concept per itertor type e non si fa così ma dopo ci penso
            void parallel_for(It start, It end, F&& f){
                using return_type = void; 
                std::vector<std::future<return_type>> ret_fut;
                for (It j = start; j!=end; ++j){
                    ret_fut.emplace_back(this->send_task_round(f,j));
                }
                for(std::future<void>& fut : ret_fut){
                    fut.get();
                }
                return;
            } 

            //TODO: correggere divione di ultime iterazioni come in paralle_for con int, non più plus_one che era sbagliato perché presupponeva massimo +1 per job iniziale, ma it_add
            //2 (it+n ok)
            template<typename F,typename It, typename... Args> 
            requires std::is_same_v<std::invoke_result_t<F,It,int,Args&...>, void> && (! std::is_reference_v<Args> && ...) // && std::random_access_iterator<It>// PER IL MOMEMNTO NON CHECK SE RANDOM ACCESS PERCHé VOGLIO PROVARLO IN ASSEMBLE 
            void parallel_for(It start, It end, F&& f,int granularity,Args... args){
                //std::cout<<"usato random access"<<std::endl;
                using return_type = void;
                int range = (end-start);
                if(granularity > range){granularity = range;}////oss: se granularity > range allora tutto range fatto da unico worker, messo granularity=range per evitare errori poi. magari mettere un warning ??? 
                int n_job = range / std::max(1,granularity); //se gran non valida (= 0) non da errore
                //default gran se mandato valore non valido, non più solo -1. cosi evitiamo controllo se granularity valida          
                if(granularity <= 0) {// 1 job per worker max (quindi resto spalmato), se range<n_worker allora n_job = range 
                    if(range<n_worker_){
                        granularity = 1;
                        n_job = range;
                    }else{
                        granularity = range / n_worker_;
                        n_job = n_worker_;
                    }
                }
                int gran_last = granularity;
                std::vector<int> it_add; // vettore di iterazioni aggiuntive al primo job di ogni worker
                
                int resto = range%granularity; 
                if(resto >0){
                    if((n_job%n_worker_) != 0){
                        n_job ++;    
                    }
                    else{// spalma perché fare un ultimo job con iterazioni di resto sbilancia
                        int iter_add = resto / n_worker_;
                        int resto_di_resto = resto % n_worker_;// tutti i worker si prendono iter_add iterazioni extra e quello che rimane dato +1 ai primi resto_di_resto worker
                        for(int w = 0; w< resto_di_resto; w++){
                            it_add.push_back(iter_add + 1);
                        }
                        for(int w = resto_di_resto; w<n_worker_ ; w++){
                            it_add.push_back(iter_add);
                        }
                    } 
                }
                std::vector<std::future<return_type>> ret_fut;
                ret_fut.reserve(n_job);

                for (int j= 0; j<it_add.size(); j++){
                    ret_fut.emplace_back(this->send_task_round([granularity = granularity+it_add[j],fun = f, ...args = args, this](auto it)mutable{ 
                            int index_worker = this-> get_index_worker_from_thread();
                            for(int k=0; k<granularity; k++ ){
                                fun(it,index_worker,args...);
                                ++it;
                            }
                        },start));
                    start+=(granularity+it_add[j]);//manda avanti start
                }
                if(it_add.size()==n_job){//per evitare di arrivare a send di last job con gran_last.  
                    //get futures
                    for(std::future<void>& fut : ret_fut){fut.get();}
                    return;
                }

                for (int j= it_add.size(); j<n_job-1; j++){
                    ret_fut.emplace_back(this->send_task_round([granularity ,fun = f, ...args = args, this](auto it)mutable{ 
                            int index_worker = this-> get_index_worker_from_thread();
                            for(int k=0; k<granularity; k++ ){
                                fun(it,index_worker,args...);
                                ++it;
                            }
                        },start));
                    start+=granularity;//manda avanti start
                }
                ret_fut.emplace_back(this->send_task_round([gran_last,fun = f, ...args = args, this](auto it)mutable{ 
                    int index_worker = this-> get_index_worker_from_thread();
                    for(int k=0; k<gran_last; k++ ){
                        fun(it,index_worker,args...);
                        ++it;
                    }
                }, start));
                for(std::future<void>& fut : ret_fut){fut.get();}
                return;
            }

//MOMENTANEAMENTE COMMENTATO PERCHé VOGLIO USARE PARALLEL_FOR CON ITEARTOR E GRANULARITY PER ASSEMBLE, MA ITERATOR DI CELLE ANCHE SE HANNO OPERATOR +n non sono ancora random access.
//TODO: NON FUNZIONA FA DIVISIONE DI RANGE IN JOB MALE C'è AULCOSA CHE NON VA (IN TEST ASSEMBLE UTILIZZATO PER SBAGLIO E STESSA CELLA RIPETUTA DA 2 WORKER)
//        DA SISTEMARE
            // //3 (it+n NONok)
            // template<typename F,typename It, typename... Args> 
            // requires std::is_same_v<std::invoke_result_t<F,It,int,Args&...>, void> && (!std::random_access_iterator<It>) && (! std::is_reference_v<Args> && ...)
            // void parallel_for(It start, It end, F&& f,int granularity, Args... args){
            //     using return_type = void;
            //     std::cout<<"usato parallel_for iterator NON random acc granularity"<<std::endl;
            //     //Let's first scroll through the entire range so that we can copy iterators at each start + k * granularity into the vector its
            //     int range = 0;
            //     std::vector<It> its; 
            //     its.push_back(start);
            //     for (It it= start; it!= end; ++it){
            //         range ++;
            //         if(range % granularity == 0){
            //             its.push_back(it);
            //         }
            //     } 
            //     int n_job = range / granularity; 
            //     std::vector<std::future<return_type>> ret_fut;
            //     ret_fut.reserve(n_job+1);
            //     for(int j = 0; j<n_job; j++){
            //         ret_fut.emplace_back(this->send_task_round([granularity,fun = f, ...args = args, this](auto it)mutable{ 
            //                 int index_worker = this-> get_index_worker_from_thread();
            //                 for(int k=0; k<granularity; k++ ){
            //                     fun(it,index_worker,args...);
            //                     ++it;
            //                 }
            //             },its[j])); 
            //     }
            //     int granularity_last_job = range % granularity;
            //     if(granularity_last_job > 0){
            //         ret_fut.emplace_back(this->send_task_round([granularity_last_job,fun = f, ...args = args,this](auto it)mutable{ 
            //             int index_worker = this-> get_index_worker_from_thread();
            //             for(int k=0; k<granularity_last_job; k++ ){
            //                 fun(it,index_worker,args...);
            //                 ++it;
            //             }
            //         }, its[n_job])); 
            //     }
            //     for(std::future<void>& fut : ret_fut){fut.get();}
            //     return;        
            // }


            //TODO: SE LASCIATI MODIFICA CON STRONG SEND 
            //PARALLEL_FOR_REDUCE (versione simile a #pragma omp parallel for reduction)
            //pair<minimo,argmin> parallel_for_reduce_min/max(int start, int end, F&& f,int granularity)   F = return_type(int), ogni body function ritorna un valore e in parallelo ogni worker calcola il minimo tra tutti i job che ha eseguito. poi sequenziale calcolo minimo tra minimi di worker 
            

            //reduce min con parallel for , idea: ogni worker avrà suo min, ogni job ha suo min e worker che lo esegue lo confronta con proprio e se migliore lo sostituisce, cosi poi alla fine sequenziale solo il confronto tra n_worker value per trovare min
            //F bodyfunction deve restituire valore da confrontare
            template<typename F> 
            requires (!std::is_same_v<std::invoke_result_t<F,int>, void>)
            auto parallel_for_reduce_min(int start, int end, F&& f,int granularity)->std::pair<std::invoke_result_t<F, int>,int>{ //TODO cambiare granularity on 
                using return_type = std::invoke_result_t<F, int>;
                //range va da start a end-1--> end-start= dimensione range
                int range = (end-start); 
                int n = range / granularity; //numero job con granularity iterazioni, se poi c'è resto le ultime iterazioni messe in ultimo job
                //scelta valore ottimo di  se passato -1 (vedi parallel_for per sppiegazioe scelta 10)
                if(granularity == -1) { 
                    granularity = range/n_worker_; 
                    if(granularity < 1){//less iteration than worker, so granularity = 1 
                        granularity = 1; 
                    }
                    n = range/granularity;    
                }
                std::vector<std::future<void>> ret_fut; //no optinal<future> perché se nullopt non pushato quindi solo future
                ret_fut.reserve(n+1); //per evitare riallocameto memoria, +1 per eventuale ultimo job fatto da ultime (end-start)%granularity iterazioni  

                //min_curr,index_in_loop_associato di ogni worker (bodyFunction F(k)=value, ci salviamo min(value) e k=argmin F(j) )
                //TODO: allineare le coppie per evitare false sharing ?
                std::vector<std::pair<return_type,int>> min_workers(n_worker_ ,std::make_pair(std::numeric_limits<return_type>::max(),start-1)); //argmin inizializzato a start-1 e non -1 perche  se loop parte con start<0 non va bene -1

                int j = 0;
                while(j< n){
                    std::optional<std::future<void>> opt_fut = this->send_task_round([granularity,j,start,map_thread_worker_ =this->map_thread_worker_,fun = f, &min_workers]()mutable{ //j catturato come copia perchè modificato detro job (j+1) quidi se catturi come reference si sballa tutto !!!
                        int stop = (j+1)*granularity+start;
                        //minimi locali in job
                        return_type min_job_local = std::numeric_limits<return_type>::max();
                        return_type min_job_curr;
                        int argmin_job_local;
                        for(int k=j*granularity+start; k<stop; k++ ){
                            min_job_curr = fun(k);
                            if(min_job_curr < min_job_local){
                                min_job_local = min_job_curr;
                                argmin_job_local = k;
                            }
                        }
                        //job ha confrontato valore di ogni iterazione e ha trovato il migliore ora se meglio di worker_min (minimo di worker che ha eseguito il job) lo aggiorna
                        int indx_worker_from_thread = map_thread_worker_[std::this_thread::get_id()];
                        if(min_job_local < min_workers[indx_worker_from_thread].first){
                            min_workers[indx_worker_from_thread].first = min_job_local;
                            min_workers[indx_worker_from_thread].second = argmin_job_local;
                        }
                    });
                    if(opt_fut){
                        //se send andato a buon push di fut in ret_fut e incrementa j
                        ret_fut.push_back(std::move(opt_fut.value())); //move perche future non copiabili
                        j++;
                    }
                }
                //?= possibile evitare questo if mettendo sopra while(j<n+1) e poi ultimo se range % granularity == 0 ultimo job mandato sara for(k=end, k<end){f()} cioè job "vuoto"
                //risposta: no perchè poi bisognerebbe mettere controllo se (j+1)*granularity+start > end, perchè in ultimo job di avanzi k<end non k<(j+1)*granularity+start 
                if(range % granularity > 0){ //inviamo ultimo job con iterazioni rimanenti 
                    j=0;
                    while(j<1){
                        std::optional<std::future<void>> opt_fut = this->send_task_round([granularity,n,start,end,j,map_thread_worker_ =this->map_thread_worker_,fun = f,&min_workers]()mutable{ //j gia catturata in & credo non serve
                        //minimi locali in job
                        return_type min_job_local = std::numeric_limits<return_type>::max();
                        return_type min_job_curr;
                        int argmin_job_local;
                        for(int k=n*granularity+start; k<end; k++ ){
                            min_job_curr = fun(k);
                            if(min_job_curr < min_job_local){
                                min_job_local = min_job_curr;
                                argmin_job_local = k;
                            }
                        }
                        //job ha confrontato valore di ogni iterazione e ha trovato il migliore ora se meglio di worker_min (minimo di worker che ha eseguito il job) lo aggiorna
                        int indx_worker_from_thread = map_thread_worker_[std::this_thread::get_id()];
                        if(min_job_local < min_workers[indx_worker_from_thread].first){
                            min_workers[indx_worker_from_thread].first = min_job_local;
                            min_workers[indx_worker_from_thread].second = argmin_job_local;
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

                /*
                // per debug vedere minimo di ogni worker 
                for (size_t k = 0; k<n_worker_; k++){
                    std::cout<<"worker indx: "<<k<<" minimo: "<<min_workers[k].first<<" argmin: "<<min_workers[k].second<<std::endl;
                }
                */
                //minimo di minimi di ogni worker
                return_type min = min_workers[0].first;
                int indx_best_in_min_workers = 0;
                for (size_t k = 1; k<n_worker_; k++){
                    if(min_workers[k].first < min){
                        indx_best_in_min_workers = k;
                        min = min_workers[k].first;
                    }
                }
                return min_workers[indx_best_in_min_workers];
            }


            //reduce max con parallel for , idea: ogni worker avrà suo max, ogni job ha suo max e worker che lo esegue lo confronta con proprio e se migliore lo sostituisce, cosi poi alla fine sequenziale solo il confronto tra n_worker value per trovare min
            //F bodyfunction deve restituire valore da confrontare
            template<typename F> 
            requires (!std::is_same_v<std::invoke_result_t<F,int>, void>)
            auto parallel_for_reduce_max(int start, int end, F&& f,int granularity)->std::pair<std::invoke_result_t<F, int>,int>{ //TODO cambiare granularity on 
                using return_type = std::invoke_result_t<F, int>;
                //range va da start a end-1--> end-start= dimensione range
                int range = (end-start); 
                int n = range / granularity; //numero job con granularity iterazioni, se poi c'è resto le ultime iterazioni messe in ultimo job
                //scelta valore ottimo di  se passato -1 (vedi parallel_for per sppiegazioe scelta 100)
                if(granularity == -1) { 
                    granularity = range/n_worker_; 
                    if(granularity < 1){//less iteration than worker, so granularity = 1 
                        granularity = 1; 
                    }
                    n = range/granularity;    
                }
                std::vector<std::future<void>> ret_fut; //no optinal<future> perché se nullopt non pushato quindi solo future
                ret_fut.reserve(n+1); //per evitare riallocameto memoria, +1 per eventuale ultimo job fatto da ultime (end-start)%granularity iterazioni  

                //min_curr,index_in_loop_associato di ogni worker (bodyFunction F(k)=value, ci salviamo min(value) e k=argmin F(j) )
                std::vector<std::pair<return_type,int>> max_workers(n_worker_ ,std::make_pair(std::numeric_limits<return_type>::min(),start-1)); //argmin inizializzato a start-1 e non -1 perche  se loop parte con start<0 non va bene -1

                int j = 0;
                while(j< n){
                    std::optional<std::future<void>> opt_fut = this->send_task_round([granularity,j,start,map_thread_worker_ =this->map_thread_worker_,fun = f, &max_workers]()mutable{ //j catturato come copia perchè modificato detro job (j+1) quidi se catturi come reference si sballa tutto !!!
                        int stop = (j+1)*granularity+start;
                        //minimi locali in job
                        return_type max_job_local = std::numeric_limits<return_type>::min();
                        return_type max_job_curr;
                        int argmax_job_local;
                        for(int k=j*granularity+start; k<stop; k++ ){
                            max_job_curr = fun(k);
                            if(max_job_curr > max_job_local){
                                max_job_local = max_job_curr;
                                argmax_job_local = k;
                            }
                        }
                        //job ha confrontato valore di ogni iterazione e ha trovato il migliore ora se meglio di worker_min (minimo di worker che ha eseguito il job) lo aggiorna
                        int indx_worker_from_thread = map_thread_worker_[std::this_thread::get_id()];
                        if(max_job_local > max_workers[indx_worker_from_thread].first){
                            max_workers[indx_worker_from_thread].first = max_job_local;
                            max_workers[indx_worker_from_thread].second = argmax_job_local;
                        }
                    });
                    if(opt_fut){
                        //se send andato a buon push di fut in ret_fut e incrementa j
                        ret_fut.push_back(std::move(opt_fut.value())); //move perche future non copiabili
                        j++;
                    }
                }
                //?= possibile evitare questo if mettendo sopra while(j<n+1) e poi ultimo se range % granularity == 0 ultimo job mandato sara for(k=end, k<end){f()} cioè job "vuoto"
                //risposta: no perchè poi bisognerebbe mettere controllo se (j+1)*granularity+start > end, perchè in ultimo job di avanzi k<end non k<(j+1)*granularity+start 
                if(range % granularity > 0){ //inviamo ultimo job con iterazioni rimanenti 
                    j=0;
                    while(j<1){
                        std::optional<std::future<void>> opt_fut = this->send_task_round([granularity,n,start,end,j,map_thread_worker_ =this->map_thread_worker_,fun = f,&max_workers]()mutable{ //j gia catturata in & credo non serve
                        //minimi locali in job
                        return_type max_job_local = std::numeric_limits<return_type>::min();
                        return_type max_job_curr;
                        int argmax_job_local;
                        for(int k=n*granularity+start; k<end; k++ ){
                            max_job_curr = fun(k);
                            if(max_job_curr > max_job_local){
                                max_job_local = max_job_curr;
                                argmax_job_local = k;
                            }
                        }
                        //job ha confrontato valore di ogni iterazione e ha trovato il migliore ora se meglio di worker_min (minimo di worker che ha eseguito il job) lo aggiorna
                        int indx_worker_from_thread = map_thread_worker_[std::this_thread::get_id()];
                        if(max_job_local > max_workers[indx_worker_from_thread].first){
                            max_workers[indx_worker_from_thread].first = max_job_local;
                            max_workers[indx_worker_from_thread].second = argmax_job_local;
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

                /*
                // per debug vedere max di ogni worker 
                for (size_t k = 0; k<n_worker_; k++){
                    std::cout<<"worker indx: "<<k<<" massimo: "<<min_workers[k].first<<" argmax: "<<min_workers[k].second<<std::endl;
                }
                */
                //max di max di ogni worker
                return_type max = max_workers[0].first;
                int indx_best_in_max_workers = 0;
                for (size_t k = 1; k<n_worker_; k++){
                    if(max_workers[k].first > max){
                        indx_best_in_max_workers = k;
                        max = max_workers[k].first;
                    }
                }
                return max_workers[indx_best_in_max_workers];
            }

            //TODO: reduce sum e product
            // ogni body function f(int i) ridà un return_type, metodo restituisce somma di tutti con logica di reduce come in parallel_for_reduce_min
            template<typename F> 
            requires (!std::is_same_v<std::invoke_result_t<F,int>, void>)
            auto parallel_for_reduce_sum(int start, int end, F&& f,int granularity)->std::invoke_result_t<F, int>{ //TODO cambiare granularity on 
                using return_type = std::invoke_result_t<F, int>;
                //range va da start a end-1--> end-start= dimensione range
                int range = (end-start); 
                int n = range / granularity; //numero job con granularity iterazioni, se poi c'è resto le ultime iterazioni messe in ultimo job
                //scelta valore ottimo di  se passato -1 (vedi parallel_for per sppiegazioe scelta 100)
                if(granularity == -1) { 
                    granularity = range/n_worker_; 
                    if(granularity < 1){//less iteration than worker, so granularity = 1 
                        granularity = 1; 
                    }
                    n = range/granularity;    
                }
                std::vector<std::future<void>> ret_fut; //no optinal<future> perché se nullopt non pushato quindi solo future
                ret_fut.reserve(n+1); //per evitare riallocameto memoria, +1 per eventuale ultimo job fatto da ultime (end-start)%granularity iterazioni  

                //min_curr,index_in_loop_associato di ogni worker (bodyFunction F(k)=value, ci salviamo min(value) e k=argmin F(j) )
                std::vector<return_type> sum_workers(n_worker_);

                int j = 0;
                while(j< n){
                    std::optional<std::future<void>> opt_fut = this->send_task_round([granularity,j,start,map_thread_worker_ =this->map_thread_worker_,fun = f, &sum_workers]()mutable{ 
                        int stop = (j+1)*granularity+start;
                        int indx_worker_from_thread = map_thread_worker_[std::this_thread::get_id()]; // per chiarezza non serve fare variabile
                        for(int k=j*granularity+start; k<stop; k++ ){
                            sum_workers[indx_worker_from_thread] += fun(k);
                        }
                    });
                    if(opt_fut){
                        //se send andato a buon push di fut in ret_fut e incrementa j
                        ret_fut.push_back(std::move(opt_fut.value())); //move perche future non copiabili
                        j++;
                    }
                }
                if(range % granularity > 0){ //inviamo ultimo job con iterazioni rimanenti 
                    j=0;
                    while(j<1){
                        std::optional<std::future<void>> opt_fut = this->send_task_round([granularity,n,start,end,j,map_thread_worker_ =this->map_thread_worker_,fun = f,&sum_workers]()mutable{ //j gia catturata in & credo non serve
                        int indx_worker_from_thread = map_thread_worker_[std::this_thread::get_id()]; // per chiarezza non serve fare variabile
                        for(int k=n*granularity+start; k<end; k++ ){
                            sum_workers[indx_worker_from_thread] += fun(k);
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
                //reduce sum finale tra sum dei workers
                return_type sum = sum_workers[0];
                for (size_t k = 1; k<n_worker_; k++){
                    sum += sum_workers[k];
                }
                return sum;
            }  
            
            template<typename F> 
            requires (!std::is_same_v<std::invoke_result_t<F,int>, void>)
            auto parallel_for_reduce_dot(int start, int end, F&& f,int granularity)->std::invoke_result_t<F, int>{ //TODO cambiare granularity on 
                using return_type = std::invoke_result_t<F, int>;
                //range va da start a end-1--> end-start= dimensione range
                int range = (end-start); 
                int n = range / granularity; //numero job con granularity iterazioni, se poi c'è resto le ultime iterazioni messe in ultimo job
                if(granularity == -1) { 
                    granularity = range/n_worker_; 
                    if(granularity < 1){//less iteration than worker, so granularity = 1 
                        granularity = 1; 
                    }
                    n = range/granularity;    
                }
                std::vector<std::future<void>> ret_fut; //no optinal<future> perché se nullopt non pushato quindi solo future
                ret_fut.reserve(n+1); //per evitare riallocameto memoria, +1 per eventuale ultimo job fatto da ultime (end-start)%granularity iterazioni  

                //min_curr,index_in_loop_associato di ogni worker (bodyFunction F(k)=value, ci salviamo min(value) e k=argmin F(j) )
                std::vector<return_type> dot_workers(n_worker_,1.0); //inizializzato ad elemento nullo per moltiplicazione

                int j = 0;
                while(j< n){
                    std::optional<std::future<void>> opt_fut = this->send_task_round([granularity,j,start,map_thread_worker_ =this->map_thread_worker_,fun = f, &dot_workers]()mutable{ 
                        int stop = (j+1)*granularity+start;
                        int indx_worker_from_thread = map_thread_worker_[std::this_thread::get_id()]; // per chiarezza non serve fare variabile
                        for(int k=j*granularity+start; k<stop; k++ ){
                            dot_workers[indx_worker_from_thread] *= fun(k);
                        }
                    });
                    if(opt_fut){
                        //se send andato a buon push di fut in ret_fut e incrementa j
                        ret_fut.push_back(std::move(opt_fut.value())); //move perche future non copiabili
                        j++;
                    }
                }
                if(range % granularity > 0){ //inviamo ultimo job con iterazioni rimanenti 
                    j=0;
                    while(j<1){
                        std::optional<std::future<void>> opt_fut = this->send_task_round([granularity,n,start,end,j,map_thread_worker_ =this->map_thread_worker_,fun = f,&dot_workers]()mutable{ //j gia catturata in & credo non serve
                        int indx_worker_from_thread = map_thread_worker_[std::this_thread::get_id()]; // per chiarezza non serve fare variabile
                        for(int k=n*granularity+start; k<end; k++ ){
                            dot_workers[indx_worker_from_thread] *= fun(k);
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
                //reduce sum finale tra sum dei workers
                return_type dot = dot_workers[0];
                for (size_t k = 1; k<n_worker_; k++){
                    dot *= dot_workers[k];
                }
                return dot;
            }  
        };
    }
#endif

        





        