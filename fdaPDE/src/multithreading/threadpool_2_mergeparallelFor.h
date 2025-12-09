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

namespace fdapde {

namespace internals {
// concept per parallel_for con iteratore che scorre il range (rilassa richiesta rispetto a random access iterator)
template <typename It>
concept parallel_iterator = requires(It a, It b, int n) {
    { a += n } -> std::same_as<It&>;
    { a - b } -> std::convertible_to<int>;
};
}   // namespace internals


struct max_load_stealing{   
    int pick(std::deque<std::atomic<int>>& count_job, int n_worker)const{
        int worker_indx = 0;
        int max_elem = count_job[0].load(std::memory_order_acquire);   // numero elementi in primo worker
        int current_el = 0;
        for (int j = 1; j < n_worker; j++) {
            current_el = count_job[j].load(std::memory_order_acquire);
            if (current_el > max_elem) {
                worker_indx = j;
                max_elem = current_el;
            }
        }
        if (max_elem == 0) { return -1; }   // avoid steal of nullopt
        return worker_indx;
    };
};

struct random_stealing{
    mutable std::mt19937 gen_;
    random_stealing():gen_(std::random_device {}()){};

    // return random int in  [0, size-1]
    int random_int(int size) const {
        std::uniform_int_distribution<> distrib(0, size - 1);
        return distrib(gen_);
    };
    int pick(std::deque<std::atomic<int>>& count_job, int n_worker)const{
        std::vector<int> indxs;   // vector of index of worker busy
        for (int k = 0; k < n_worker; k++) {
            // oss: not avoid himself, to avoid extra if() or split range(worker k find in
            // (0,...,k-1,k+1,...,n_worker-1))
            if (count_job[k].load(std::memory_order_acquire) > 0) { indxs.push_back(k); }
        }
        int size = indxs.size();
        switch (size) {   // avoid random gen_eration if not need it
        case 0:
            return -1;
        case 1:
            return indxs[0];
        default:
            return indxs[random_int(size)];
        }
    };

};

struct top_half_random_stealing{
    mutable std::mt19937 gen_;

    top_half_random_stealing():gen_(std::random_device {}()){};
    
    // return random int in  [0, size-1]
    int random_int(int size) const {
        std::uniform_int_distribution<> distrib(0, size - 1);
        return distrib(gen_);
    };
    int pick(std::deque<std::atomic<int>>& count_job, int n_worker)const{
        std::vector<std::pair<int, int>> indxs;   // vector of pair: (index worker busy, number of job in queue)
        int tmp_count_job = 0;
        for (int k = 0; k < n_worker; k++) {
            tmp_count_job = count_job[k].load(std::memory_order_acquire);
            if (tmp_count_job > 0) { indxs.emplace_back(k, tmp_count_job); }
        }
        int size = indxs.size();
        if (size == 0) { return -1; }
        // sort from mostbusy to least
        std::sort(indxs.begin(), indxs.end(), [](std::pair<int, int>& a, std::pair<int, int>& b) {
            return a.second > b.second;
        });
        if(size < 4){
            return indxs[0].first;
        }else{
            return indxs[random_int(size / 2)].first;
        }
    };
};

struct round_robin_scheduling{
    int indx_ = 0;
    int pick(std::deque<std::atomic<int>>& count_job, int n_worker){//qui non serve count_job_ solo n_worker, però per avere firma uguale a tutti altri lasciato
        int index = indx_;
        (indx_ == n_worker - 1) ? (indx_ = 0) : (indx_++);//manda avanti indice circolarmente
        return index;
    }
};

struct least_loaded_scheduling{
    int pick(std::deque<std::atomic<int>>& count_job, int n_worker)const{//non serve n_worker ma lasciato per aver firma identicia ad altri
        int worker_indx = 0;
        int min_elem = count_job[0].load(std::memory_order_acquire);
        int current_el = 0;
        for (int j = 1; j < n_worker; j++) {
            current_el = count_job[j].load(std::memory_order_acquire);
            if (current_el < min_elem) {
                worker_indx = j;
                min_elem = current_el;
            }
        }
        return worker_indx;
    };
};


    
    

template <typename SchedulingStrategy = round_robin_scheduling,typename StealingStrategy = random_stealing> class threadpool {
    using job = std::function<void()>;
   public:
    class worker {
       private:
        int indx_;
        fdapde::synchro_queue<job, fdapde::relaxed> sync_queue_;
        std::thread t_;
        bool stop_ = false;
        mutable std::mutex m_;
        mutable std::condition_variable cv_;
       public:
        friend class threadpool;
        worker(int n, void (threadpool::*worker_loop)(int), threadpool* Th, int idx) :
            indx_(idx), sync_queue_(n), t_(worker_loop, Th, idx) {};

        // avoidable wraps, but use to make code clearer
        std::unique_lock<std::mutex> get_lock() const {
            std::unique_lock<std::mutex> loc(m_);
            return loc;
        }
        void notify() const { cv_.notify_one(); }
        void stop() { stop_ = true; }
        void start() { stop_ = false; }
        void join() { t_.join(); }
        bool push_front(job& fun) {
            return sync_queue_.push_front(fun);
        };   // reference in input because is only a wrap-function (intermediate step), than synchro_queue store a copy
        bool push_back(job& fun) { return sync_queue_.push_back(fun); };
        std::optional<job> pop_front() { return sync_queue_.pop_front(); };
        std::optional<job> pop_back() { return sync_queue_.pop_back(); };
    };
   private:
    std::vector<std::shared_ptr<worker>> workers_;   // vector of pointers because worker not movable, not copiable
    std::deque<std::atomic<int>> count_job_;         // deque because atomic<int> not movable
    int n_worker_;
    int queue_size_;
    mutable std::shared_mutex m_threadpool_;   // mutable per tenere const get_indx_from_worker()
    std::condition_variable_any cv_threadpool_;
    bool active_ = false;
    
    std::unordered_map<std::thread::id, int>
      map_thread_worker_;   // map of : thread_ID of worker's thread, index of worker in workers_
    mutable std::shared_mutex m_map_;
    StealingStrategy steal_policy_;
    SchedulingStrategy schedule_policy_;

    //helper function per workerloop. indx = index of the worker from whom the job j was taken
    bool try_do_(std::optional<job> j, int indx) {
        if (j) {
            (j.value())();
            count_job_[indx].fetch_sub(1, std::memory_order_release);
            return true;
        }
        return false;
    }
   public:
    friend class worker;
    // n = size_synchro_queue, k = number of workers
    threadpool(int size_queue, int n_worker) :
        n_worker_(n_worker), queue_size_(size_queue) {
        std::unique_lock<std::shared_mutex> loc(m_threadpool_);
        workers_.reserve(n_worker);
        for (int i = 0; i < n_worker; i++) {
            workers_.emplace_back(std::make_shared<worker>(size_queue, &threadpool::worker_loop, this, i));
            count_job_.emplace_back(0);
        }
        active_ = true;
        loc.unlock();
        cv_threadpool_.notify_all();
    };

    // constructor default number of worker
    threadpool(int size_queue) : threadpool(size_queue, std::thread::hardware_concurrency()) { }

    // default constructor
    threadpool() : threadpool(256) { }

    ~threadpool() {
        while (n_jobs() > 0) { std::this_thread::yield(); };
        for (int i = 0; i < n_worker_; i++) {
            while (!workers_[i]->sync_queue_.empty()) { }
        }
        // let's finish all worker_loop
        for (int j = 0; j < n_worker_; j++) {
            std::unique_lock<std::mutex> loc(workers_[j]->get_lock());
            workers_[j]->stop();
            workers_[j]->notify();
        }
        for (int j = 0; j < n_worker_; j++) { workers_[j]->join(); }
    }

    // getter
    int n_workers() const { return n_worker_; };
    int index_worker() const {
        std::shared_lock<std::shared_mutex> loc(
          m_map_);   // aggiunto lock perché non saprei che altro possa causare errore out of range in cluster
        return map_thread_worker_.at(std::this_thread::get_id());
    }


    void worker_loop(int i) {   // i = index of worker in workers_

        std::shared_lock<std::shared_mutex> lock_shared(m_threadpool_);
        cv_threadpool_.wait(lock_shared, [this]() { return active_; });
        lock_shared.unlock();

        // upload map_thread_worker thread-safe. //shared mutex in modalità scrittura, così in metodo che deve solo
        // leggere usato in modalità lettura tra tutti i worker che quindi una volta superata questa fase di
        // inizializzazione della mappa hanno comportamento non-blocking (ovviamente intendo solo per lettura mappa )
        std::unique_lock<std::shared_mutex> loc_map(m_map_);
        map_thread_worker_[std::this_thread::get_id()] = i;
        loc_map.unlock();

        int indx_steal = -1;   // index to steal from
        while (!workers_[i]->stop_) {

            if (try_do_(workers_[i]->pop_front(), i)) {
                continue;   // avoid lock mutex in the end
            }
            // try steal
            indx_steal = steal_policy_.pick(count_job_,n_worker_); 
            if (indx_steal != -1) {
                try_do_(workers_[indx_steal]->pop_back(), indx_steal);
                continue;
            }
            // chek for job whit mutex lock, eventualy go to sleep
            std::unique_lock<std::mutex> loc(workers_[i]->m_);
            workers_[i]->cv_.wait(loc, [&]() {
                return count_job_[i].load(std::memory_order_acquire) > 0 || n_jobs() > 0 ||
                       workers_[i]->stop_;
            });
            loc.unlock();
            if (workers_[i]->stop_) { return; }
        }
    };

    int n_jobs() const {
        int somma = 0;
        for (int i = 0; i < n_worker_; i++) somma += count_job_[i].load(std::memory_order_acquire);
        return somma;
    }

   
    // send 
    template <typename F, typename... Args>
    auto send(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        // wrap
        using return_type = decltype(f(args...));
        std::shared_ptr<std::packaged_task<return_type()>> ptr_task =
          std::make_shared<std::packaged_task<return_type()>>(
            [fun = f, ... args_catturati = args]() mutable { return fun(args_catturati...); });
        std::future<return_type> fut = ptr_task->get_future();
        job j = [ptr_task]() { (*ptr_task)(); };

        int indx_worker = schedule_policy_.pick(count_job_,n_worker_);
        std::unique_lock<std::mutex> lock(workers_[indx_worker]->get_lock());
        bool flag = workers_[indx_worker]->push_back(j);
        while (!flag) { flag = workers_[indx_worker]->push_back(j); }
        count_job_[indx_worker].fetch_add(1, std::memory_order_release);
        lock.unlock();
        workers_[indx_worker]->cv_.notify_one();
        return fut;
    };


    // parallel_for //TODO: controllo start<end va aggiunto ?
    // OVERLOAD:
    //  parallel_for(int,int,F&&) --> granularity = 1
    //  parallel_for(int,int,F&&,function<int(int)> incr) --> custom increment to scroll range, granularity = 1
    //  parallel_for(int,int,F&&,granularity) --> parallel_for whit granularity in input. if granularuty <= 0 use
    //  defaul-granularity. default-granularity is s.t. every worker receives about 1 job
    //  parallel_for(int,int,F&&,vector<int>) --> divide range in vect.size() block, granularity of block j = vect[j]

    // F = body_function, firm of F: void (int i)
    template <typename F, typename Index>
        requires std::is_same_v<std::invoke_result_t<F, Index>, void>
    void parallel_for(Index start, Index end, F&& f) {
        using return_type = void;
        std::vector<std::future<return_type>> ret_fut;
        if constexpr(std::is_integral_v<Index>) {ret_fut.reserve(end - start);}
        for (int j = start; j != end; j++) { ret_fut.emplace_back(this->send(f, j)); }
        for (std::future<void>& fut : ret_fut) { fut.get(); }
        return;
    }


    template <typename F>
        requires std::is_same_v<std::invoke_result_t<F, int>, void>
    void parallel_for(int start, int end, F&& f, std::function<int(int)> incr) {
        using return_type = void;
        std::vector<std::future<return_type>> ret_fut;
        ret_fut.reserve(end - start);
        for (int j = start; j < end; j = incr(j)) { ret_fut.emplace_back(this->send(f, j)); }
        for (std::future<void>& fut : ret_fut) { fut.get(); }
        return;
    }

    template <typename F, typename... Args>
        requires std::is_same_v<std::invoke_result_t<F, int, int, Args&...>, void>
    void parallel_for(
      int start, int end, F&& f, std::vector<int> granularities,
      Args... args) {   // copia di vettore gran è più sicuro in multithreading
        using return_type = void;
        if (std::reduce(granularities.cbegin(), granularities.cend(), 0) != (end - start)) {
            std::cerr << "somma di elem in granularities deve essere uguale a range (end-start)" << std::endl;
            return;
        }
        std::vector<int> seq = {start};   // seq vettore di somme parziali (es np.cumsum)
        int sum_seq = start;
        size_t vect_size = granularities.size();
        for (size_t l = 0; l < vect_size; l++) {
            sum_seq += granularities[l];
            seq.push_back(sum_seq);
        }
        std::vector<std::future<return_type>> ret_fut;
        ret_fut.reserve(vect_size);
        for (size_t j = 0; j < vect_size; j++) {
            ret_fut.emplace_back(this->send([&seq, j, fun = f, ... args = args, this]() mutable {
                int index_worker = this->index_worker();
                for (int k = seq[j]; k < seq[j + 1]; k++) { fun(k, index_worker, args...); }
            }));
        }
        for (std::future<void>& fut : ret_fut) { fut.get(); }
        return;
    }

    
    void granularity_calculation(int range, int& granularity, int& n_job, std::vector<int>& it_add,std::vector<std::future<void>>& ret_fut){
        if (granularity > range) {
            granularity = range;
        }
        n_job = range / std::max(1, granularity);  
        if (granularity <= 0) {   
            if (range < n_worker_) {
                granularity = 1;
                n_job = range;
            } else {
                granularity = range / n_worker_;
                n_job = n_worker_;
            }
        }

        int resto = range % granularity;
        if (resto > 0) {
            if ((n_job % n_worker_) != 0) {
                n_job++;
            } else {   // spalma perché fare un ultimo job con iterazioni di resto sbilancia
                int iter_add = resto / n_worker_;
                int resto_di_resto = resto % n_worker_;   // tutti i worker si prendono iter_add iterazioni extra e
                                                          // quello che rimane dato +1 ai primi resto_di_resto worker
                for (int w = 0; w < resto_di_resto; w++) { it_add.push_back(iter_add + 1); }
                for (int w = resto_di_resto; w < n_worker_; w++) { it_add.push_back(iter_add); }
            }
        }
        ret_fut.reserve(n_job);
    }

    template <typename F, typename... Args>
        requires std::is_same_v<
          std::invoke_result_t<F, int, int, Args&...>,
          void>   // F in input ha: int i (loop index), index worker,  reference a  Args (tmp ogetti copie per ogni
                  // job), però Args non reference perché in lambda che viene inviata alla threadpool si deve copiare
                  // oggetto non reference ad oggetto
                  void parallel_for(
                    int start, int end, F&& f, int granularity,
                    Args... args) {   
        using return_type = void;
        int n_job;
        std::vector<int> it_add;   // vettore di iterazioni aggiuntive al primo job di ogni worker
        std::vector<std::future<return_type>> ret_fut;
        granularity_calculation(end - start, granularity, n_job, it_add, ret_fut);

        int stop_local = start;
        for (int j = 0; j < it_add.size(); j++) {
            stop_local += (granularity + it_add[j]);
            ret_fut.emplace_back(
              this->send([start = start, stop = stop_local, j, fun = f, ... args = args, this]() mutable {
                  int index_worker = this->index_worker();
                  for (int k = start; k < stop; k++) {
                      fun(
                        k, index_worker,
                        args...);   // f deve prendere in input reference a variadic template altrimenti tutto inutile
                  }
              }));
            start = stop_local;   // job successivo parte da fine di precedente
        }
        if (it_add.size() == n_job) {   // per evitare di arrivare a send di last job con gran_last.
            // get futures
            for (std::future<void>& fut : ret_fut) { fut.get(); }
            return;
        }
        for (int j = it_add.size(); j < n_job - 1; j++) {
            stop_local = granularity + start;
            ret_fut.emplace_back(this->send(
              [start, stop = stop_local, fun = f, ... args = args,
               this]() mutable {   // usato resto_spalmato perché magari c'è resto ma non viene spalmato eviene inserito
                                   // in ultimo job e quindi qui le iterazioni vanno da j*granularity+start a
                                   // (j+1)*granularity+start
                  int index_worker = this->index_worker();
                  for (int k = start; k < stop; k++) { fun(k, index_worker, args...); }
              }));
            start = stop_local;
        }
        // last job (puo essere o gran_last o granularity normale) inviato separatamente per non dover fare if
        ret_fut.emplace_back(this->send([start, end, fun = f, ... args = args, this]() mutable {
            int index_worker = this->index_worker();
            for (int k = start; k < end; k++) { fun(k, index_worker, args...); }
        }));

        // get futures
        for (std::future<void>& fut : ret_fut) { fut.get(); }
        return;
    }

    // PARALLEL_FOR_ITERATOR:
    // body_function f = void(iterator)
    // 1)void parallel_for_iterator(It start, It end, F&& f)                     per tutti i container, =1 default
    // 2)void parallel_for_iterator(It start, It end, F&& f,int granularity)    per random acces container,  come input
    // 3)void parallel_for_iterator(It start, It end, F&& f,int granularity)    per NON random acces container,  come
    // input


    
    // 2 (it+n ok)
    template <typename F, typename It, typename... Args>
        requires std::is_same_v<std::invoke_result_t<F, It, int, Args&...>, void> &&
                 internals::parallel_iterator<
                   It>// require non più random access ma solo +=,- (concept definito inizio file: parallel_iterator)   
    void parallel_for(It start, It end, F&& f, int granularity, Args... args) {
        using return_type = void;
        int n_job;
        std::vector<int> it_add;   // vettore di iterazioni aggiuntive al primo job di ogni worker
        std::vector<std::future<return_type>> ret_fut;
        granularity_calculation(end - start, granularity, n_job, it_add, ret_fut);

        for (int j = 0; j < it_add.size(); j++) {
            ret_fut.emplace_back(this->send(
              [granularity = granularity + it_add[j], it = start, fun = f, ... args = args, this]() mutable {
                  int index_worker = this->index_worker();
                  for (int k = 0; k < granularity; k++) {
                      fun(it, index_worker, args...);
                      ++it;
                  }
              }));
            start += (granularity + it_add[j]);   // manda avanti start
        }
        if (start == end) {   
            for (std::future<void>& fut : ret_fut) { fut.get(); }
            return;
        }

        for (int j = it_add.size(); j < n_job - 1; j++) {
            ret_fut.emplace_back(
              this->send([granularity, it = start, fun = f, ... args = args, this]() mutable {
                  int index_worker = this->index_worker();
                  for (int k = 0; k < granularity; k++) {
                      fun(it, index_worker, args...);
                      ++it;
                  }
              }));
            start += granularity;   // manda avanti start
        }
        ret_fut.emplace_back(this->send([start, end, fun = f, ... args = args, this]() mutable {
            int index_worker = this->index_worker();
            for (auto it = start; it != end; ++it) { fun(it, index_worker, args...); }
        }));
        for (std::future<void>& fut : ret_fut) { fut.get(); }
        return;
    }

    // no default granularity. 
    // //3 (it+n NONok)
    template <typename F, typename It, typename... Args>
        requires std::is_same_v<std::invoke_result_t<F, It, int, Args&...>, void> && (!internals::parallel_iterator<It>)
    void parallel_for(It start, It end, F&& f, int granularity, Args... args) {
        using return_type = void;
        if (granularity <= 0) {
            std::cerr << "granularity must be positive";
            return;
        }
        // Let's first scroll through the entire range so that we can copy iterators at each start + k * granularity
        // into the vector its
        int range = 0;
        std::vector<It> its;
        its.push_back(start);
        for (It it = start; it != end; ++it) {
            range++;
            if (range % granularity == 0) { its.push_back(it); }
        }
        int n_job = its.size();
        std::vector<std::future<return_type>> ret_fut;
        ret_fut.reserve(n_job);
        for (int j = 0; j < n_job - 1; j++) {
            ret_fut.emplace_back(
              this->send([start = its[j], stop = its[j + 1], fun = f, ... args = args, this]() mutable {
                  int index_worker = this->index_worker();
                  for (auto it = start; it != stop; ++it) { fun(it, index_worker, args...); }
              }));
        }
        ret_fut.emplace_back(
          this->send([start = its[n_job - 1], end, fun = f, ... args = args, this]() mutable {
              int index_worker = this->index_worker();
              for (auto it = start; it != end; ++it) { fun(it, index_worker, args...); }
          }));
        for (std::future<void>& fut : ret_fut) { fut.get(); }
        return;
    }

};
}   // namespace fdapde
#endif
