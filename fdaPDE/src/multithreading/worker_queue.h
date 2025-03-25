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
    typedef std::vector<value_type> container;
        private:
            container queue_;
            int head_ = 0; //indx of 1 over "first" element
            int tail_ = 0; //indx of "last" element
            int size_ = 0;
            bool empty_queue_ = true;
            std::mutex m_;
        public:
            // default constructor credo poi da associare a metodo resize()
            Worker_queue() = default;

            // construct whit size of queue_=n;
            Worker_queue(int n){
                queue_.resize(n);
                size_ = n;
            }

            // TODO: figure out the correct requires
            template<typename Iterator> Worker_queue(Iterator begin, Iterator end) {
                queue_.insert(queue_.begin(),begin,end);
                head_ = queue_.size();
                size_ = head_;
                empty_queue_ = false;
            }

            Worker_queue(const Worker_queue&) = delete;
            void operator=(const Worker_queue&) = delete;


            // TODO: resize to sizes smaller than the current one? Clear the queue when resizing?
            bool resize(int n){

                std::lock_guard<std::mutex> loc(m_);
                if(n < size_){
                    std::cerr << "Cannot resize to smaller size" << std::endl;
                    return 0;
                }
                
                queue_.resize(n);

                return 1;
                
            }

            //TODO: mutexes should be used for operations in the front too since we can go to the back when we reach the end
            bool push_front(value_type t){
                std::lock_guard<std::mutex> loc(m_);
                int new_head = (head_ == size_-1)? (0) : (head_ + 1);
                if (head_ != tail_){
                    queue_[head_] = std::move(t);
                    head_ = new_head;
                    return 1;}
                else if(empty_queue_==true){
                    queue_[head_] = std::move(t);
                    head_= new_head;
                    empty_queue_ = false;
                    return 1;} 
                std::cerr<<"queue full"<<std::endl;
                return 0;   
            }

            std::optional<value_type> pop_front(){
                std::lock_guard<std::mutex> loc(m_);
                if (empty_queue_){
                    std::cerr<<"queue empty"<<std::endl;
                    return std::nullopt;
                }
                int new_head = (head_== 0)? (size_-1) : (head_-1);
                value_type ret = queue_[new_head];
                queue_[new_head] = value_type();
                head_ = new_head;
                if(head_==tail_) {empty_queue_ = true;}  //head_ ==tail_ after pop() means empty, in general means full  
                return ret;
                
            }
            
            //push_back() thread-safe 
            bool push_back(value_type t){
                std::lock_guard<std::mutex> loc(m_);
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
            std::optional<value_type> pop_back(){
                std::lock_guard<std::mutex> loc(m_);

                if(empty_queue_ == true){
                    std::cerr << "Queue is empty" << std::endl;
                    return std::nullopt;
                }
                int new_tail = (tail_ == size_-1)? (0):(tail_+1);
                value_type ret = std::move(queue_[tail_]);
                queue_[tail_] = value_type();
                tail_ = new_tail;
                if(head_==tail_) {empty_queue_ = true;}
                return ret;
            }

            // wrap of function size() empty() of vector thrade-safe
            int size() {
                std::lock_guard<std::mutex> loc(m_);
                return queue_.size();
            }
            bool empty() {
                std::lock_guard<std::mutex> loc(m_);
                return empty_queue_;
            }
            
            // svuota queue_
            void clear(){ 
                std::lock_guard loc(m_);
                queue_.clear();
                head_ = 0;
                tail_ = 0;
                empty_queue_ = true;
            } 
        


            //per debug momentanei
            int get_tail()const {return tail_;}
            int get_head()const {return head_;} 
            void print(){
                for (value_type i : queue_)
                    std::cout<<i<<"  ";
                std::cout<<std::endl;
            }



    };

};

#endif