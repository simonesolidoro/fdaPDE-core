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
    enum Stato{
        empty, //elem vuoto OSS: evitera di dover verificare se head==tail (queue piena) 
        busy,// si sta modificando elem
        ready, //elem ha v pronto per essere pop
    };
    struct elem{
        std::atomic<int> stato;
        value_type v;
    };


    typedef std::vector<elem> container;
        private:
            container queue_;
            std::atomic<int> head_; //indx of 1 over "first" element
            std::atomic<int> tail_; //indx of "last" element
            int size_;
            std::atomic<bool> empty_queue_; // va eliminato eigen non lo usa (aggiornalo dopo pop_* vredo renderebbe ogni pop sequenziale e quindi tutto inutile le atomic variabili )
            std::mutex m_;
        public:
            Worker_queue(const Worker_queue&) = delete;
            void operator=(const Worker_queue&) = delete;
            // default constructor credo poi da associare a metodo resize()
            Worker_queue(){
                head_ = 0;
                tail_ = 0;
                empty_queue_ = true;
            };
            // construct whit size of queue_=n;
            Worker_queue(int n):queue_(n){
                head_.store(0);
                tail_.store(0);
                size_ = n;
                empty_queue_ = true;
                for (int i = 0; i < size_; i++)
                    queue_[i].stato.store(Stato::empty, std::memory_order_relaxed);
            }

            /* non possibile fare queue_.resize() perche atomic variable, tolto per il momento
            // resize() di queue_ super easy for the moment
            bool resize(int n){
                std::lock_guard<std::mutex> loc(m_);
                if(n < size_){
                    std::cerr << "Cannot resize to smaller size" << std::endl;
                    return 0;
                }
                
                queue_.resize(n);

                return 1;
                
            }
            */

            bool push_front(value_type t){ //prova a iserire dove puta head_
                int h= head_.load(std::memory_order_relaxed);
                int new_head = (h == size_-1)? (0) : (h + 1);
                elem* e= &queue_[h]; //puntatore ad elem di arry in posizione head (dove si vuole inserire elemento)
                int s= e->stato.load(std::memory_order_relaxed); //copia di stato di elem in head
                if(s!= Stato::empty || !e->stato.compare_exchange_strong(s, Stato::busy, std::memory_order_acquire)) //operazioni di lettura di e->state sincronizzate a dopo questa modifica 
                    return false; //se elem non vuoto o stato di elem nel mentre è stato modificato (da altro thread) 
                head_.store(new_head,std::memory_order_relaxed);
                e->v = std::move(t); 
                e->stato.store(ready,std::memory_order_release);
                return true;   
            }

            value_type pop_front(){ //prova toglire uno dietro head_
                int h= head_.load(std::memory_order_relaxed);
                int new_head = (h== 0)? (size_-1) : (h-1);
                elem* e =& queue_[new_head]; //(puntatore ad elem che si vuole pop)
                int s = e->stato.load(std::memory_order_relaxed);
                if(s!= Stato::ready || !e->stato.compare_exchange_strong(s, Stato::busy, std::memory_order_acquire)) //operazioni di lettura di e->state sincronizzate a dopo questa modifica 
                    return value_type();
                value_type ret= std::move(e->v);
                e->stato.store(Stato::empty, std::memory_order_release);
                head_.store(new_head, std::memory_order_relaxed);  
                return ret;
                
            }
            
            //push_back() thread-safe 
            bool push_back(value_type t){ //prova a inserire uno dietro tail_ 
                std::lock_guard<std::mutex> loc(m_); 
                int tail = tail_.load(std::memory_order_relaxed);
                int new_tail = (tail_ == 0)? (size_-1) : (tail_ -1);
                elem* e = &queue_[new_tail];
                int s = e->stato.load(std::memory_order_relaxed);
                if (s != Stato::empty || !e->stato.compare_exchange_strong(s, Stato::busy, std::memory_order_acquire))
                    return false;
                tail_.store(new_tail, std::memory_order_relaxed);
                e->v=std::move(t);
                e->stato.store(Stato::ready, std::memory_order_release);
                return true;  
            }

            //pop_back() thrade-safe
            value_type pop_back(){ //prova togliere in tail_
                if(Empty()){
                    std::cerr << "Queue is empty" << std::endl;
                    return value_type();
                }
                std::lock_guard<std::mutex> loc(m_);
                int tail = tail_.load(std::memory_order_relaxed);
                int new_tail = (tail_ == size_-1)? (0):(tail_+1);
                elem* e = &queue_[tail];
                int s = e->stato.load(std::memory_order_relaxed);
                if (s != ready || !e->stato.compare_exchange_strong(s, Stato::busy, std::memory_order_acquire))
                    return value_type();
                value_type ret = std::move(e->v);
                e->stato.store(Stato::empty, std::memory_order_release);
                tail_.store(new_tail, std::memory_order_relaxed);
                if(head_==tail_) {empty_queue_ = true;} // da togliere
                return ret;
            }

            // wrap of function size() empty() of vector thrade-safe
            int size(){
                std::lock_guard<std::mutex> loc(m_);
                return queue_.size();
            }
            bool Empty(){
                /* da implementare in modo che non renda tutto sequenziale, ancora non chiaro
                std::lock_guard<std::mutex> loc(m_);
                return empty_queue_;
                */
                int head = head_.load(std::memory_order_acquire);
                for (;;) {
                int tail = tail_.load(std::memory_order_acquire);
                int head1 = head_.load(std::memory_order_relaxed);
                if (head != head1) {
                    head = head1;
                    std::atomic_thread_fence(std::memory_order_acquire);
                    continue;
                }
                
                return (head == tail) && (queue_[head].stato.load() == Stato::empty);
                }

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
                for (int i=0; i<size_; i++)
                    std::cout<<queue_[i].v<<"  ";
                std::cout<<std::endl;
            }



    };

};

#endif