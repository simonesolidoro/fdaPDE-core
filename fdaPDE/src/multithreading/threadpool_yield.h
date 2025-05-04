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
                    int count_job_ = 0;
                public:
                    // costruttore con numero elementi di coda
                    Worker(int n):sync_queue_(n),t_(&Worker::worker_loop,this){};
                    
                    ~Worker(){//no mutex perche non dormono su cv i worker_loop quindi anche se cambio di stop arriva dopo male che va worker_loop fa un iterazione di while extra ma non ce deadlock
                        stop_ = true;  
                        t_.join();
                    }

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

                   //lettura non affidabile però è sufficente per dare una aprossimazione utile a implementare  steal e send_task 
                    int get_count_job() const{
                        return count_job_;
                    };

                    //wrap di funzioni per pop e push. //OSS: se count_el non si sistema aggiungere modifica a contatori di numero job in coda direttamente in questi wrap di push e pop
                    bool push_front(job fun){
                        count_job_ ++;
                        return  sync_queue_.push_front(fun);
                    };
                    bool push_back(job fun){
                        count_job_ ++;
                        return  sync_queue_.push_back(fun);
                    };
                    std::optional<job> pop_front(){
                        count_job_ --;
                        return  sync_queue_.pop_front();
                    };
                    std::optional<job> pop_back(){
                        count_job_ --;
                        return  sync_queue_.pop_back();
                    };

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
                return threadpool_[indx_worker]->push_back(j);
            };
            //send a giro usando struct indxw 
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