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
    using value_type = T;
    struct elem{
        std::atomic<bool> empty_;
        std::optional<value_type> v_;
    };


    typedef std::vector<elem> container;
        private:
            container queue_;
            int head_ = 0; //indx of 1 over "first" element
            int tail_ = 0; //indx of "last" element
            int size_ = 0;
            bool empty_queue_ = true;
            std::atomic<int> occupied_;
            std::mutex m_;
            std::condition_variable cv_can_pop_; //notif when element add to queue_, can pop
            std::condition_variable cv_can_push_; //notif when element removed from queue_, can push 
            bool active_ = true;
        public:
            // default constructor credo poi da associare a metodo resize()
            Worker_queue() = default;

            // construct whit size of queue_=n;
            Worker_queue(int n): queue_(n){
                size_ = n;
                for(int i =0; i<n;i++)
                    queue_[i].empty_.store(true);

                occupied_.store(0);

            }

            // TODO: figure out the correct requires
            template<typename Iterator> Worker_queue(Iterator begin, Iterator end){
                int n = end - begin;
                std::vector<elem> temp_queue(n);
                for(int i =0; i<n;i++){
                    temp_queue[i].empty_.store(false);
                    temp_queue[i].v_ =  *(begin + i);
                }
                std::swap(queue_, temp_queue);
                head_ = queue_.size();
                size_ = head_;
                empty_queue_ = false;
                occupied_.store(0);

            }
            ~Worker_queue(){
                active_ = false;
                cv_can_pop_.notify_all();
                cv_can_push_.notify_all();
            }

            Worker_queue(const Worker_queue&) = delete;
            void operator=(const Worker_queue&) = delete;

            void resize(int n){

                std::lock_guard<std::mutex> loc(m_);

                std::vector<elem> temp_queue(n);

                std::swap(queue_, temp_queue);

                for(int i =0; i<n;i++)
                    queue_[i].empty_.store(true);

                size_ = n;
                head_ = 0;
                tail_ = 0;
                empty_queue_ = true;

                occupied_.store(0);
                
            }


            bool push_front(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                if (head_ == tail_ && !empty_queue_ ){// coda piena
                    std::cerr<<"queue full"<<std::endl; // per debug poi da togliere
                    return false;
                }
                //se si arruva qui c'è posto, però bisogna aspettare che elemento sia stato liberato (se coda era piena ma viene fatto un pop_front che aggiorna tail_ in modo da head_!= tail_ ma ancora non ha liberato elemento)

                // ora posto libero 
                int h = head_; //index dove inserira elemento
                head_ = (head_ == size_-1)? (0) : (head_ + 1);
                empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente 
                loc.unlock();

                while(!queue_[h].empty_.load( std::memory_order_acquire)){} //finche non diventa vero (elemeto acora da svuotare da pop_frot)
                //push di elemento
                queue_[h].v_ = std::move(t);
                queue_[h].empty_.store(false, std::memory_order_release); //aggiorna stato di elem con release

                occupied_.fetch_add(1,std::memory_order_release);

                cv_can_pop_.notify_one();
                return true; 
            }

            bool push_front_or_wait(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_push_.wait(loc,[this](){return !this->active_ ||  this->head_ != this->tail_ || (this->head_ == this->tail_ && !this->empty_queue_);});
                if(!active_){return false;}
                int h = head_; //index dove inserira elemento
                head_ = (head_ == size_-1)? (0) : (head_ + 1);
                empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente 
                loc.unlock();

                while(!queue_[h].empty_.load( std::memory_order_acquire)){} //finche non diventa vero (elemeto acora da svuotare da pop_frot)
                //push di elemento
                queue_[h].v_ = std::move(t);
                queue_[h].empty_.store(false, std::memory_order_release); //aggiorna stato di elem con release

                occupied_.fetch_add(1,std::memory_order_release);

                cv_can_pop_.notify_one();
                return true; 
            }

            std::optional<value_type> pop_front(){
                std::unique_lock<std::mutex> loc(m_);
                if (empty_queue_){
                    std::cerr<<"queue empty"<<std::endl;
                    return std::nullopt;
                }
                // coda non vuota ma magari un push_back ha modificato indici e non ancora inserito elemento (caso critico coda vuota poi push_back poi pop_front)
                int new_head = (head_== 0)? (size_-1) : (head_-1);

                head_ = new_head;
                if(head_==tail_) {empty_queue_ = true;}  //head_ ==tail_ after pop() means empty, in general means full
                loc.unlock();

                while(queue_[new_head].empty_.load( std::memory_order_acquire)){} // aspetta finche diventa falso (elemnto inserito effettivamente da push_back)

                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = std::move(queue_[new_head].v_.value());
                queue_[new_head].v_ = std::nullopt;
                queue_[new_head].empty_.store(true, std::memory_order_release);

                occupied_.fetch_sub(1,std::memory_order_release);

                cv_can_push_.notify_one();

                return ret;
                   
            }

            std::optional<value_type> pop_front_or_wait(){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_pop_.wait(loc,[this](){return !this->active_ || !this->empty_queue_;});
                if(!active_) return std::nullopt ;

                int new_head = (head_== 0)? (size_-1) : (head_-1);

                head_ = new_head;
                if(head_==tail_) {empty_queue_ = true;}  //head_ ==tail_ after pop() means empty, in general means full
                loc.unlock();

                while(queue_[new_head].empty_.load( std::memory_order_acquire)){} // aspetta finche diventa falso (elemnto inserito effettivamente da push_back)

                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = std::move(queue_[new_head].v_.value());
                queue_[new_head].v_ = std::nullopt;
                queue_[new_head].empty_.store(true, std::memory_order_release);

                occupied_.fetch_sub(1,std::memory_order_release);

                return ret;
            }
            
            //push_back() thread-safe 
            bool push_back(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                if (head_ == tail_ && !empty_queue_ ){// coda piena
                    std::cerr<<"queue full"<<std::endl; // per debug poi da togliere
                    return false;
                }

                int new_tail;
                if(empty_queue_){ // se coda vuota elemento inserito dove punta tail (new_tail=tail) ed head spostato +1
                    new_tail = tail_;
                    head_ = (head_ == size_-1)? (0) : (head_ + 1);
                    empty_queue_ = false; 
                }
                else{
                    new_tail = (tail_ == 0)? (size_-1) : (tail_ -1);
                    tail_ = new_tail;
                }
                loc.unlock();

                while(!queue_[new_tail].empty_.load( std::memory_order_acquire)){}

                queue_[new_tail].v_ = std::move(t);
                queue_[new_tail].empty_.store(false, std::memory_order_release);

                occupied_.fetch_add(1,std::memory_order_release);

                cv_can_pop_.notify_one();
                return true;
            }

            bool push_back_or_wait(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_push_.wait(loc,[this](){return !this->active_ || this->head_ != this->tail_ || (this->head_ == this->tail_ && !this->empty_queue_);});
                if(!active_){return false;}

                int new_tail;
                if(empty_queue_){ // se coda vuota elemento inserito dove punta tail (new_tail=tail) ed head spostato +1
                    new_tail = tail_;
                    head_ = (head_ == size_-1)? (0) : (head_ + 1);
                    empty_queue_ = false; 
                }
                else{
                    new_tail = (tail_ == 0)? (size_-1) : (tail_ -1);
                    tail_ = new_tail;
                }
                loc.unlock();

                while(!queue_[new_tail].empty_.load( std::memory_order_acquire)){}

                queue_[new_tail].v_ = std::move(t);
                queue_[new_tail].empty_.store(false, std::memory_order_release);

                occupied_.fetch_add(1,std::memory_order_release);

                cv_can_pop_.notify_one();
                return true;
            }

            //pop_back() thrade-safe
            std::optional<value_type> pop_back(){
                std::unique_lock<std::mutex> loc(m_);
                if(empty_queue_ == true){
                    std::cerr << "Queue is empty" << std::endl;
                    return std::nullopt;
                }

                int t = tail_; // tmp idice di elmeto da pop
                int new_tail = (tail_ == size_-1)? (0):(tail_+1);
                tail_ = new_tail;
                if(head_==tail_) {empty_queue_ = true;}
                loc.unlock();

                while(queue_[t].empty_.load( std::memory_order_acquire)){} // aspetta finche diventa falso (elemnto inserito effettivamente da push_front)


                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = std::move(queue_[t].v_.value());
                queue_[t].v_ = std::nullopt;
                queue_[t].empty_.store(true, std::memory_order_release);


                occupied_.fetch_sub(1,std::memory_order_release);

                cv_can_push_.notify_one();

                return ret;
            }

            std::optional<value_type> pop_back_or_wait(){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_pop_.wait(loc,[this](){return !this->active_ || !this->empty_queue_;}); // loc mutex, controllo condizione in lamda, se falsa unlock mutex e wait se vera va avanti
                //copia codice di pop_back() tranne check se coda vuota, alternativa a chiamata diretta di pop_back che però porta a dover usare recursive mutex (definito dal libro come il male assoluto)
                if(!active_) return std::nullopt; //se chiamato distruttore distruttore notifica a tutti di verificare condizione wait 
                int t = tail_; // tmp idice di elmeto da pop
                int new_tail = (tail_ == size_-1)? (0):(tail_+1);
                tail_ = new_tail;
                if(head_==tail_) {empty_queue_ = true;}
                loc.unlock();

                while(queue_[t].empty_.load( std::memory_order_acquire)){} // aspetta finche diventa falso (elemnto inserito effettivamente da push_front)


                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = std::move(queue_[t].v_.value());
                queue_[t].v_ = std::nullopt;
                queue_[t].empty_.store(true, std::memory_order_release);


                occupied_.fetch_sub(1,std::memory_order_release);

                return ret;
            }

            // wrap of function size() empty() of vector thrade-safe
            int size() {
                std::lock_guard<std::mutex> loc(m_);
                return queue_.size();
            }
            bool empty() {
                return occupied_.load(std::memory_order_acquire) == 0;  
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
                for (int i=0; i<size_; i++){
                    if(queue_[i].empty_.load() == true){
                        std::cout<<0<<" ";
                    }
                    else{
                        std::cout<<queue_[i].v_.value()<<"  ";
                    }
                }
                std::cout<<std::endl;
            }



    };

};

#endif