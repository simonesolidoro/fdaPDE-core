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
    template<typename T,typename... Args>
    class Worker{
        using job = std::function<T(Args... args)>;
        private: 
            fdapde::Synchro_queue<job,fdapde::relax_nowait> sync_queue_;
            std::thread t_;
            bool stop_ = false;
            //std::condition_variable cv_; CV su che mutex ????
        public:
            // costruttore con numero elementi di coda
            Worker(int n):sync_queue_(n),t_(&Worker::worker_loop,this){};
            
            ~Worker(){
                //while(!sync_queue_.empty()){}; //per aspettare che worker finisca i job in coda. PB: a volte empty() chiamato prima di push_back() di metodo send_task di threadpool
                stop_ = true; //PROBLEMA: chiamato distruttore prima che job effettivamente finiti. SOLUZIONE: usare future associato a task in main cosi che future.get() garantisce fine di task prima di chiamata distruttore
                t_.join();
            }

            void worker_loop(){
                while(!stop_){
                    //TODO: capire come mettere a dormire se coda vuota, nel frattempo messo yields()
                    std::optional<job> j = sync_queue_.pop_front();
                    if(j){//esegue se non è nullopt
                        j.value()();
                    }
                    else{
                        std::this_thread::yield(); //da controllo a OS, possibile che stoppi l'esecuzione del thread a favore di altro (usato per mettere una pezza a mancanza condion varibale che fa wait se coda empty)
                    }
                }
            };

            //wrap di funzioni per pop e push
            bool push_front(job fun){
                return  sync_queue_.push_front(fun);
            };
            bool push_back(job fun){
                return  sync_queue_.push_front(fun);
            };
            std::optional<job> pop_front(){
                return  sync_queue_.pop_front();
            };
            std::optional<job> pop_back(){
                return  sync_queue_.pop_back();
            };
    };

    template<typename T,typename... Args>
    class Threadpool{
        using job = std::function<T(Args... args)>;
        struct indx_worker{
            int indx_ = 0;
            void next(int n_worker){
                (indx_ == n_worker-1)? (indx_ = 0):(indx_++);
            };
        };
        private:
            std::vector<std::shared_ptr<fdapde::Worker<T,Args...>>> threadpool_; //vettore di putatori perche non movable e copiable synchro_queue per via di mutex
            std::vector<int> count_task_;
            int n_worker_;
            indx_worker indxw_; 
        public:
            Threadpool(int n, int k):n_worker_(k){
                threadpool_.reserve(k);
                for(int i=0; i<k; i++){
                    threadpool_.emplace_back(std::make_shared<fdapde::Worker<T,Args...>> (n));
                    count_task_.push_back(0);
                }
            };

            //ridà indice di worker piu libero (in realta finche non implementato decremento count ridà indice a cui sono stati mandti meno job non chi ne ha di meno in quel momento )
            int indx_freer(){
                int worker_indx = 0; 
                for (size_t j=1; j<count_task_.size(); j++){
                    if(count_task_[j] < count_task_[worker_indx] ) 
                        worker_indx = j;
                }
                return worker_indx;
            };
            bool send_task(job j){
                int indx_worker = indx_freer();
                if(threadpool_[indx_worker]->push_back(j)){
                    count_task_[indx_worker] ++;
                    return true;
                }
                return false;
            };
            bool send_task_round(job j){
                if(threadpool_[indxw_.indx_]->push_back(j)){
                    indxw_.next(n_worker_);
                    return true;
                }
                return false;
            };
    };
}
#endif