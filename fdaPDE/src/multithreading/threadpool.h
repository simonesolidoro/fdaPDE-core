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
    class Worker{
        using job = std::function<void()>;
        private: 
            fdapde::Synchro_queue<job,fdapde::relax_nowait> sync_queue_;
            std::thread t_;
            bool stop_ = false;
            //std::condition_variable cv_; CV su che mutex ????
        public:
            // costruttore con numero elementi di coda
            Worker(int n):sync_queue_(n),t_(&Worker::worker_loop,this){};
            
            ~Worker(){
                stop_ = true;
                t_.join();
            }
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

            void worker_loop(){
                while(!stop_){
                    std::optional<job> j = pop_front();
                    job jj;
                    if(j){
                        jj = j.value(); //esegue se non è nullopt
                        jj();
                    }
                }
            };
    };

    class Threadpool{
        using job = std::function<void()>;
        private:
            std::vector<std::shared_ptr<fdapde::Worker>> threadpool_; //vettore di putatori perche non movable e copiable synchro_queue per via di mutex
            std::vector<int> count_task_;
        public:
            Threadpool(int n, int k){
                threadpool_.reserve(k);
                for(int i=0; i<k; i++){
                    threadpool_.emplace_back(std::make_shared<fdapde::Worker> (n));
                    count_task_.push_back(0);
                }
            };
            //rida indice di worker piu libero
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
    };
}
#endif