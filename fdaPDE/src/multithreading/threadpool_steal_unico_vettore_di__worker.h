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
                //CAUSA SEGMENTATION FAULT: PASSAGGIO DI REFERENCE A TUTTA THREADPOOL. PERCHE DIVENTA INVALIDA APPENA CHIAMATO DISTRUTTORE. SOLUZIONE: PASSARE REF A VETTORE DI WORKER CHE DIVENTA INVALIDO SOLO DOPO ESECUZIONE CORPO DISTRUTTORE.
                //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
                //CORREZIONE IN threadpool_steal_unicvettworker_mutex.h QUINDI QUI NON HO VOGLIA TANTO QUESTO .h NON SERVE
                    fdapde::Threadpool& threadpool_; //per far accedere worker a metodi di threadpool e poter fare steal job ma problemi con distruttori segmentation fault
                    fdapde::Synchro_queue<job,fdapde::relax_nowait> sync_queue_;
                    std::thread t_;
                    bool stop_ = false;
                    std::mutex m_;
                    std::condition_variable cv_; 
                    int count_job_ = 0;
                public:
                    // costruttore con numero elementi di coda
                    Worker(int n, fdapde::Threadpool& tp):threadpool_(tp),sync_queue_(n),t_(&Worker::worker_loop,this){};
                    
                    ~Worker(){
                        std::unique_lock<std::mutex> loc(m_);
                        stop_ = true; //PROBLEMA: chiamato distruttore prima che job effettivamente finiti. SOLUZIONE: usare future associato a task in main cosi che future.get() garantisce fine di task prima di chiamata distruttore
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
                    void set_stop(bool s){
                        stop_ = s;
                    }
                    void join_thread(){
                        t_.join();
                    }

                    void worker_loop(){
                        while(!stop_){
                            if(!sync_queue_.empty()){
                                std::optional<job> j = pop_front();
                                if(j)//esegue se non è nullopt
                                    (j.value())(); //esegue funzioni con 0 parametri e void. per non void si dovra fare wrap e associare a promise. per parametri lamda wrap che li cattura cosi no param  
                            }
                            else{ //steal
                                steal_from_most_busy_and_do();  
                            }                                 
                        }
                    };
                    
                    
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

                    
                    //per rubare job da back a chi è piu impegnato ed eseguirlo
                    void steal_from_most_busy_and_do(){//PROBLEMA: SEMBRA IMPOSSIBILE VERIFICARE CHE DISTRUTTORE DI THREADPOOL NON SIA STATO CHIAMATO SENZA ACCEDERE A threadpool_ OSS: anche solo per tentare di bloccare mutex di Threadpool: std::unique_lock<std::mutex> loc(threadpool_.get_lock()) si deve accedere e si fa segmentation fault
                            int most_busy = threadpool_.indx_most_busy();
                            std::optional<job> j = (threadpool_.get_worker(most_busy))->pop_back();
                            if(j){
                                (j.value())();
                            }
                    }
                    

            };
        private:
            std::vector<std::shared_ptr<Worker>> threadpool_; //vettore di putatori perche non movable e copiable synchro_queue per via di mutex
            int n_worker_;
            indx_worker indxw_; 
            std::mutex m_; //usato per far si che indx_most_busy ridia -1 se chiamato distruttore di threadpool, cosi si evita che thread di worker diano segmentation faukt perche provano ad accedere a worker distrutto durante steal_job
            bool stop_ = false;
        public:
            friend class Worker;
            //n = size code, k = numero worker
            Threadpool(int n, int k):n_worker_(k){
                threadpool_.reserve(k);
                for(int i=0; i<k; i++){
                    threadpool_.emplace_back(std::make_shared<Worker> (n,std::ref(*this)));
                }
            };
            ~Threadpool(){
                //facciamo terminare tutti worker_loop cosi che nessun worker acceda a worker distrutti o a threadpool (perche quando il resto del distruttore di threadpool verra chiamato, cioe finito il corpo di questo distruttore, tutti i worker avranno terminato worker_loop grazie a join())
                for(int j = 0; j<n_worker_; j++){
                    threadpool_[j]->set_stop(true);
                }
                for(int j = 0; j<n_worker_; j++){
                    threadpool_[j]->join_thread();
                }
            }

            std::shared_ptr<Worker> get_worker(int indx){
                return threadpool_[indx];
            }

            std::unique_lock<std::mutex> get_lock(){
                std::unique_lock<std::mutex> loc(m_);
                return loc;
            }
            bool get_stop(){
                return stop_;
            }
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
                bool flag = threadpool_[indx_worker]->push_back(j);
                if(flag){
                    return true;
                }
                return false;
            };
            //send a giro usando struct indxw 
            bool send_task_round(job j){
                bool flag = threadpool_[indxw_.indx_]->push_back(j);
                if(flag){
                    indxw_.next(n_worker_);
                    return true;
                }
                return false;
            };
    };
}
#endif