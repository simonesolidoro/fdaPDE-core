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
//
//

#ifndef __FDAPDE_WORKER_QUEUE_H__
#define __FDAPDE_WORKER_QUEUE_H__

#include "header_check.h"

namespace fdapde {
    template <typename T> 
    class Worker_queue{
    using value_type= T;
        private:
            std::vector<value_type> queue_;
            int head_; //indx of 1 over "first" element
            int tail_; //indx of "last" element
            int size_;
            bool empty_queue_; 
            std::mutex m;
        public:
            // default constructor credo poi da associare a metodo resize()
            Worker_queue(){
                head_ = 0;
                tail_ = 0;
                empty_queue_ = true;
            };
            // construct whit size of queue_=n;
            Worker_queue(int n){
                queue_.resize(n);
                head_ = 0;
                tail_ = 0;
                size_ = n;
                empty_queue_ = true;
            }

            bool push_front(value_type t){
                int new_head = (head_ == size_-1)? (0) : (head_ + 1);
                if (head_ != tail_ ){
                    queue_[head_] = t;
                    head_ = new_head;
                    return 1;}
                else if(empty_queue_==true){
                    queue_[head_] = t;
                    head_= new_head;
                    empty_queue_ = false;
                    return 1;} 
                //std::cerr<<"queue full"<<std::endl;
                return 0;   
            }

            T pop_front(){
                value_type new_empty;
                if (empty_queue_){
                    //std::cerr<<"queue empty"<<std::endl;
                    return new_empty;
                }
                int new_head = (head_== 0)? (size_-1) : (head_-1);
                value_type ret = queue_[new_head];
                queue_[new_head] = new_empty;
                head_ = new_head;
                if(head_==tail_) {empty_queue_ = true;}
                return ret;
                
            }
            
            //push_back() thread-safe 
            bool push_back(value_type t){
                std::lock_guard loc(m);
                int new_tail = (tail_ == 0)? (size_-1) : (tail_ -1);
                if (head_ != tail_){ //se non pieno
                    queue_[new_tail] = t;
                    tail_ = new_tail;
                    return 1;}
                else if(empty_queue_==true){
                    queue_[tail_] = t;
                    head_++;
                    empty_queue_ = false;
                    return 1;} 
                //std::cerr<<"queue full"<<std::endl;
                return 0;   
            }

            //pop_back() thrade-safe
            T pop_back(){
                std::lock_guard loc(m);
                value_type new_empty;
                if(empty_queue_ == true){
                    //errore empty queue
                }
                int new_tail = (tail_ == size_-1)? (0):(tail_+1);
                value_type ret = queue_[tail_];
                queue_[tail_] = new_empty;
                tail_ = new_tail;
                if(head_==tail_) {empty_queue_ = true;}
                return ret;
            }

            // wrap of function size() empty() of vector thrade-safe
            int size(){
                std::lock_guard loc(m);
                return queue_.size();
            }
            bool empty(){
                std::lock_guard loc(m);
                return queue_.empy();
            }
            
            // svuota queue_
            void flush(){ 
                std::lock_guard loc(m);
                value_type new_empty;
                if (head_ > tail_)
                    for(int i = tail_ ; i< head_ ; i++){
                        queue_[i] = new_empty; 
                    }
                else if(head_ < tail_)
                    for(int i = head_ ; i< tail_ ; i++){
                        queue_[i] = new_empty; 
                    }
                if (head_==tail_){
                    for(int i=0; i<size_; i++)
                        queue_[i] = new_empty;
                } 
                /*forse piu efficente direttamente svuotare tutto senza vedere dovè pieno
                        for(int i=0; i<size_; i++)
                            queue_[i] = new_empty;
                */
            } 
        


            //per debug momentanei
            int get_tail()const {return tail_;}
            int get_head()const {return head_;} 
            void print(){
                for (T i : queue_)
                    std::cout<<i<<"  ";
                std::cout<<std::endl;
            }



    };

};

#endif