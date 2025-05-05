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
            //OSS: mettere count_job (contatore non sincronizzato, utile solo per avere aprox di elementi in ogni coda) direttamente dentro synchro_queue se si decide di implementare cosi la theradpool. (non fatto subito perche se non usato count_job viene aggiunto ++/-- in ogni push e pop per niente)
            class Sync_queue_count{
                private: 
                    fdapde::Synchro_queue<job,fdapde::relax_nowait> sync_queue_;
                    int count_job_ = 0;
                public:
                    Sync_queue_count(int n):sync_queue_(n){};

                    bool empty(){
                        return sync_queue_.empty();
                    }

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
        private:
            std::vector<std::shared_ptr<Sync_queue_count>> sync_queues_; 
            std::vector<std::thread> threads_;
            int n_worker_;
            indx_worker indxw_;
            bool stop_ = false; 
        public:
            //n = size code, k = numero worker
            Threadpool(int n, int k):n_worker_(k){
                threads_.reserve(k);
                for(int i=0; i<k; i++){
                    sync_queues_.emplace_back(std::make_shared<Sync_queue_count>(n));
                }
                for(int i=0; i<k; i++){
                    threads_.emplace_back(&Threadpool::worker_loop,this,i);
                }
            };

            ~Threadpool(){
                stop_ = true;
                for(int j = 0; j<n_worker_; j++){
                    threads_[j].join();
                }
            }

            void worker_loop(int i){
                while(!stop_){
                    if(sync_queues_[i]->get_count_job() == 0){
                        std::this_thread::yield();
                    }
                    if(!sync_queues_[i]->empty()){
                        std::optional<job> j = sync_queues_[i]->pop_front();
                        if(j)//esegue se non è nullopt
                            (j.value())(); //esegue funzioni con 0 parametri e void. per non void si dovra fare wrap e associare a promise. per parametri lamda wrap che li cattura cosi no param  
                    }
                    else{ //steal
                        steal_from_most_busy_and_do();  
                    }                                 
                }
            };

            void steal_from_most_busy_and_do(){
                int most_busy = indx_most_busy();
                std::optional<job> j = sync_queues_[most_busy]->pop_back();
                if(j){
                    (j.value())();
                }
            }

            //ridà indice di worker piu libero (lettura di elmenti in sync_queue di worker fatta con metodo count_el non affidabile ma è abbastanza per avere un idea)
            int indx_most_free(){
                int worker_indx = 0;
                int min_elem= sync_queues_[0]->get_count_job(); //numero elementi in primo worker 
                for (int j=1; j<n_worker_; j++){
                    int current_el = sync_queues_[j]->get_count_job();
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
                int max_elem= sync_queues_[0]->get_count_job(); //numero elementi in primo worker 
                for (int j=1; j<n_worker_; j++){
                    int current_el = sync_queues_[j]->get_count_job();
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
                return sync_queues_[indx_worker]->push_back(j);
            };
            //send a giro usando struct indxw 
            bool send_task_round(job j){
                if(sync_queues_[indxw_.indx_]->push_back(j)){
                    indxw_.next(n_worker_);
                    return true;
                }
                return false;
            };
    };
}
#endif