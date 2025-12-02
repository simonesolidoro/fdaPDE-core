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

#ifndef __FDAPDE_SYNCHRO_QUEUE_H__
#define __FDAPDE_SYNCHRO_QUEUE_H__

#include "header_check.h"

namespace fdapde{
    namespace internals {
        //define concept (iterator of vector,array,list)
        template <typename Iterator, typename T>
        concept vector_array_list = std::contiguous_iterator<Iterator> || std::same_as<Iterator, typename std::list<T>::iterator> ;
    }

    //access_model:
    struct relaxed{};
    struct deferred{};
    struct blocking{};

    //forward declaration
    template<typename value_type, typename access_model> class synchro_queue;

    template<typename value_type>
    class synchro_queue<value_type,relaxed>{
        public:
            //enumerator state of elem.  
            static constexpr int Empty = 1; //true 1
            static constexpr int Busy = 2;
            static constexpr int Full = 0; //false 0
            // elem relax. //oss: public per poter usarlo in helper function che fa effettivo push/pop come input, alternativa rendere friend anche la helper function dei pop/push (come quelle di index) e mettere privata
            struct elem{
                std::atomic<int> state_ = Empty; //synchro_queue<value_type,relax>::Empty; 
                std::optional<value_type> v_;
            };
        private:
            typedef std::vector<elem> container;
            container queue_;
            int head_ = 0; //indx of first element
            int tail_ = 0; //indx of 1 over last element
            int size_ = 0;
            mutable std::mutex m_;

        public:
            // default constructor 
            synchro_queue() = default;
            // construct whit size of queue_=n;
            synchro_queue(int n): queue_(n){
                size_ = n;
            }
            //construct from vector, array, list
            template <typename Iterator>
            requires internals::vector_array_list<Iterator,value_type>
            synchro_queue(Iterator begin, Iterator end){
                int n = std::distance(begin, end); //itertor of list doesn't support "end-begin"
                std::vector<elem> temp_queue(n);
                for(int i =0; i<n;i++){
                    temp_queue[i].state_.store(Full);
                    temp_queue[i].v_ = *(begin);
                    std::advance(begin,1);
                }
                std::swap(queue_, temp_queue);
                size_ = queue_.size();
            }
        
            synchro_queue(const synchro_queue&) = delete;
            void operator=(const synchro_queue&) = delete;
        
            void resize(int n){
                std::lock_guard<std::mutex> loc(m_);
                std::vector<elem> temp_queue(n);
                std::swap(queue_, temp_queue);
                size_ = n;
                head_ = 0;
                tail_ = 0;
            }
        
            
            int get_size() const {return size_;}
            int get_tail()const {return tail_;}
            int get_head()const {return head_;}
            void clear(){
                std::lock_guard loc(m_);
                queue_.clear();
                head_ = 0;
                tail_ = 0;
            }
            //per debug momentanei
            void print(){
                for (int i=0; i<size_; i++){    
                    if(queue_[i].state_.load(std::memory_order_acquire) == Empty){
                        std::cout<<0<<" ";
                    }
                    else{
                        std::cout<<queue_[i].v_.value()<<"  ";
                    }
                }
                std::cout<<std::endl;
            }

            bool push_front(value_type val){
                std::unique_lock<std::mutex> loc(m_);
                int new_head = (head_ == 0)? (size_-1) : (head_ -1);
                if(queue_[new_head].state_.load(std::memory_order_acquire) != Empty)
                    return -1;
                queue_[new_head].state_.store(Busy, std::memory_order_relaxed);
                head_ = new_head;                         
                loc.unlock();
                //push 
                queue_[new_head].v_ = std::move(val);
                queue_[new_head].state_.store(Full, std::memory_order_release); 
                return true; 
            }

            std::optional<value_type> pop_front(){
                std::unique_lock<std::mutex> loc(m_);
                int h = head_; 
                if(queue_[h].state_.load(std::memory_order_acquire) != Full)
                    return -1;
                queue_[h].state_.store(Busy, std::memory_order_relaxed);
                head_ = (head_ == size_-1)? (0):(head_+1);
                loc.unlock();
                // pop 
                value_type ret = std::move(queue_[h].v_.value());
                queue_[h].v_ = std::nullopt;
                queue_[h].state_.store(Empty, std::memory_order_release);
                return ret;
            }

            bool push_back(value_type val){
                std::unique_lock<std::mutex> loc(m_);
                int t = tail_;
                if(queue_[t].state_.load(std::memory_order_acquire) != Empty)
                    return -1;
                queue_[t].state_.store(Busy, std::memory_order_relaxed); 
                tail_ = (tail_ == size_-1)? (0) : (tail_ + 1); //tail_++
                loc.unlock();
                //push
                queue_[t].v_ = std::move(val);
                queue_[t].state_.store(Full, std::memory_order_release);        
                return true;
            }

            std::optional<value_type> pop_back(){
                std::unique_lock<std::mutex> loc(m_);
                int new_tail = (tail_== 0)? (size_-1) : (tail_-1);
                if(queue_[new_tail].state_.load(std::memory_order_acquire) != Full)
                    return -1;
                queue_[new_tail].state_.store(Busy, std::memory_order_relaxed);
                tail_ = new_tail; 
                loc.unlock();
                //pop
                value_type ret = std::move(queue_[t].v_.value());
                queue_[t].v_ = std::nullopt;
                queue_[t].state_.store(Empty, std::memory_order_release);
                return ret;
            }

            bool empty() const {
                std::lock_guard<std::mutex> loc(m_);
                //lock mutex so head and tail do not change in the meantime
                if(head_ == tail_){
                    //Arrived here either because full or empty, while waiting for possible busy
                    while(true){
                        if(queue_[tail_].state_.load(std::memory_order_acquire)==Empty){ 
                            return true;
                        }
                        if(queue_[tail_].state_.load(std::memory_order_acquire)==Full){ 
                            return false;
                        }
                    }
                }else{
                    return false;
                }
            
            }
    };


    template <typename value_type>  
    class synchro_queue<value_type,deferred>{
        public:
            //enumerator state of elem.  
            static constexpr int Empty = 1; //true 1
            static constexpr int Full = 0; //false 0
            // elem hold
            struct elem{
                int state_ = Empty; 
                std::optional<value_type> v_;
                mutable std::mutex m_el_;
                std::condition_variable cv_ready_to_push_;
                std::condition_variable cv_ready_to_pop_; 
                int count_pop_ = 0; 
            };
        private:
            typedef std::vector<elem> container;
            container queue_;
            int head_ = 0;//indx of first element 
            int tail_ = 0;//indx of 1 over last element
            int size_ = 0;
            bool empty_queue_ = true; 
            mutable std::mutex m_;
        public:
            // default constructor
            synchro_queue() = default;
            // construct whit size of queue_=n;
            synchro_queue(int n): queue_(n){
                size_ = n;
            }
            // constructor from list array vector
            template <typename Iterator>
            requires internals::vector_array_list<Iterator,value_type>
            synchro_queue(Iterator begin, Iterator end){
                int n = std::distance(begin, end); 
                std::vector<elem> temp_queue(n);
                for(int i =0; i<n;i++){
                    temp_queue[i].state_ = Full;
                    temp_queue[i].v_ = *(begin);
                    std::advance(begin,1);        
                }
                std::swap(queue_, temp_queue);
                size_ = queue_.size();
                empty_queue_ = false;
            }

           


            synchro_queue(const synchro_queue&) = delete;
            void operator=(const synchro_queue&) = delete;

            void resize(int n){
                std::lock_guard<std::mutex> loc(m_);
                std::vector<elem> temp_queue(n);
                std::swap(queue_, temp_queue);
                size_ = n;
                head_ = 0;
                tail_ = 0;
                empty_queue_ = true;
            }

            int get_size() const {return size_;}
            int get_tail()const {return tail_;}
            int get_head()const {return head_;}
            void clear(){
                std::lock_guard loc(m_);
                queue_.clear();
                head_ = 0;
                tail_ = 0;
                empty_queue_ = true;
            }

            //per debug momentanei
            void print(){
                for (int i=0; i<size_; i++){
                    if(queue_[i].state_ == Empty){
                        std::cout<<0<<" ";
                    }
                    else{
                        std::cout<<queue_[i].v_.value()<<"  ";
                    }
                }
                std::cout<<std::endl;
            }


            bool push_front(value_type val){
                std::unique_lock<std::mutex> loc(m_);
                if (head_ == tail_ && !empty_queue_ ){
                    return -1;
                }
                empty_queue_ = false;
                int new_head = (head_ == 0)? (size_-1) : (head_ -1);
                head_ = new_head;
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[new_head].m_el_);
                queue_[new_head].cv_ready_to_push_.wait(loc_el,[this,new_head](){return queue_[new_head].state_==Empty;}); // to be sure state_ == Empty
                queue_[new_head].v_ = std::move(val);
                queue_[new_head].state_ = Full; 
                loc_el.unlock();
                queue_[new_head].cv_ready_to_pop_.notify_one(); // notification of any waiting pop on the same element
                return true; 
            }

            std::optional<value_type> pop_front(){
                std::unique_lock<std::mutex> loc(m_);
                if(empty_queue_ ){
                    return -1;
                }
                int h = head_; 
                head_ = (head_ == size_-1)? (0):(head_+1);
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[h].m_el_);
                queue_[h].cv_ready_to_pop_.wait(loc_el,[this,h](){return queue_[h].state_== Full;});
                // pop 
                value_type ret = std::move(queue_[h].v_.value());
                queue_[h].v_ = std::nullopt;
                queue_[h].state_ = Empty;
                queue_[h].count_pop_ --;
                loc_el.unlock();  
                queue_[h].cv_ready_to_push_.notify_one();              
                return ret;                  
            }

            bool push_back(value_type val){
                std::unique_lock<std::mutex> loc(m_);
                if (head_ == tail_ && !empty_queue_ ){return -1;}
                empty_queue_ = false; //maybe already false, so redundant, but avoids if(empty_queue_) {empty_queue_ = false;} 
                int t = tail_; 
                tail_ = (tail_ == size_-1)? (0) : (tail_ + 1); //tail_++
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[t].m_el_);
                queue_[t].cv_ready_to_push_.wait(loc_el,[this,t](){return queue_[t].state_==Empty;});
                //push
                queue_[t].v_ = std::move(val);
                queue_[t].state_ = Full; 
                loc_el.unlock();
                queue_[t].cv_ready_to_pop_.notify_one();

                return true;
            }

            std::optional<value_type> pop_back(){
                std::unique_lock<std::mutex> loc(m_);
                if (empty_queue_){
                    return -1;
                }
                int new_tail = (tail_== 0)? (size_-1) : (tail_-1);
                tail_ = new_tail; 
                if(head_==tail_) {empty_queue_ = true;} 
                queue_[new_tail].count_pop_ ++;
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[new_tail].m_el_);
                queue_[new_tail].cv_ready_to_pop_.wait(loc_el,[this,new_tail](){return queue_[new_tail].state_ == Full;});
                value_type ret = std::move(queue_[new_tail].v_.value());
                queue_[new_tail].v_ = std::nullopt;
                queue_[new_tail].state_ = Empty;
                queue_[new_tail].count_pop_ --;
                loc_el.unlock();
                queue_[new_tail].cv_ready_to_push_.notify_one();
                return ret;
            }

            bool empty() const {
                std::lock_guard<std::mutex> loc(m_);
                if(empty_queue_){
                    //reading count pop inside element's mutex to ensure that if count_pop==0 then the last pop (moving the element out of the queue) has indeed been executed.
                    bool not_empty = true;
                    while(not_empty){//wait untile count_pop == 0
                        std::unique_lock<std::mutex> loc_el(queue_[tail_].m_el_);
                        not_empty = (queue_[tail_].count_pop_ != 0);
                        loc_el.unlock();
                        std::this_thread::yield(); 
                    };
                    return true;
                }
                else
                    return false; 
            }
    };

    
    template <typename value_type>  
    class synchro_queue<value_type, blocking>{
        public:
            // elem hold
            struct elem{
                int state_ = Empty; //1 == true == empty, 0 == false == full. relying on the implicit int-to-bool conversion
                std::optional<value_type> v_;
                mutable std::mutex m_el_;
                std::condition_variable cv_ready_to_push_;
                std::condition_variable cv_ready_to_pop_; 
                int count_pop_ = 0; 
            };
        private:
            typedef std::vector<elem> container;
            container queue_;
            int head_ = 0; //indx of first element
            int tail_ = 0; //indx of 1 over last element
            int size_ = 0;
            bool empty_queue_ = true; 
            mutable std::mutex m_;
            std::condition_variable cv_can_pop_;
            std::condition_variable cv_can_push_;

            //metodi privati per refactoring (evitano riscrivere stesso codice per metodi wait())
            bool push_front_(value_type& val, int new_head){
                cv_can_pop_.notify_one(); // for pop_or_wait
                std::unique_lock<std::mutex> loc_el(queue_[new_head].m_el_);
                queue_[new_head].cv_ready_to_push_.wait(loc_el,[this,new_head](){return queue_[new_head].state_==Empty;});
                queue_[new_head].v_ = std::move(val);
                queue_[new_head].state_ = Full; 
                loc_el.unlock();
                queue_[new_head].cv_ready_to_pop_.notify_one();            
                return true;
            }

            std::optional<value_type> pop_front_(int h){
                cv_can_push_.notify_one();
                std::unique_lock<std::mutex> loc_el(queue_[h].m_el_);
                queue_[h].cv_ready_to_pop_.wait(loc_el,[this,h](){return queue_[h].state_==Full;});
                value_type ret = std::move(queue_[h].v_.value());
                queue_[h].v_ = std::nullopt;
                queue_[h].state_ = Empty;
                queue_[h].count_pop_ --;
                loc_el.unlock();  
                queue_[h].cv_ready_to_push_.notify_one();              
                return ret;
            }

            bool push_back_(value_type& val, int t){
                cv_can_pop_.notify_one();
                std::unique_lock<std::mutex> loc_el(queue_[t].m_el_);
                queue_[t].cv_ready_to_push_.wait(loc_el,[this,t](){return queue_[t].state_ == Empty;});
                push_fb_push<value_type,blocking>(queue_[t],val);
                queue_[t].v_ = std::move(val);
                queue_[t].state_ = Full; 
                loc_el.unlock();
                queue_[t].cv_ready_to_pop_.notify_one();
                return true;
            }

            std::optional<value_type> pop_back(int new_tail){
                cv_can_push_.notify_one();
                std::unique_lock<std::mutex> loc_el(queue_[new_tail].m_el_);
                queue_[new_tail].cv_ready_to_pop_.wait(loc_el,[this,new_tail](){return queue_[new_tail].state_ == Full;});
                value_type ret = std::move(queue_[new_tail].v_.value());
                queue_[new_tail].v_ = std::nullopt;
                queue_[new_tail].state_ = Empty;
                queue_[new_tail].count_pop_ --;
                loc_el.unlock();
                queue_[new_tail].cv_ready_to_push_.notify_one();
                return ret;
            }


        public:
            //enumerator state of elem.  
            static constexpr int Empty = 1; //true 1
            static constexpr int Full = 0; //false 0
            // default constructor 
            synchro_queue() = default;
            // construct whit size of queue_=n;
            synchro_queue(int n): queue_(n){
                size_ = n;
            }
            // constructor from list array vector
            template <typename Iterator>
            requires internals::vector_array_list<Iterator,value_type>
            synchro_queue(Iterator begin, Iterator end){
                int n = std::distance(begin, end); 
                std::vector<elem> temp_queue(n);
                for(int i =0; i<n;i++){
                    temp_queue[i].state_ = Full;
                    temp_queue[i].v_ = *(begin);
                    std::advance(begin,1);       
                }
                std::swap(queue_, temp_queue);
                size_ = queue_.size();
                empty_queue_ = false;
            }


            synchro_queue(const synchro_queue&) = delete;
            void operator=(const synchro_queue&) = delete;

            void resize(int n){
                std::lock_guard<std::mutex> loc(m_);
                std::vector<elem> temp_queue(n);
                std::swap(queue_, temp_queue);
                size_ = n;
                head_ = 0;
                tail_ = 0;
                empty_queue_ = true;
            }

             
            int get_size() const {return size_;}
            int get_tail()const {return tail_;}
            int get_head()const {return head_;}            
            void clear(){
                std::lock_guard loc(m_);
                queue_.clear();
                head_ = 0;
                tail_ = 0;
                empty_queue_ = true;
            }
            //per debug momentanei
            void print(){
                for (int i=0; i<size_; i++){
                    if(queue_[i].state_ == Empty){
                        std::cout<<0<<" ";
                    }
                    else{
                        std::cout<<queue_[i].v_.value()<<"  ";
                    }
                }
                std::cout<<std::endl;
            }
// oss: il check se la coda è piena o vuota una volta bloccatto il mutex della coda non viene fatto nei metodi wait() perché sarebbe superflua

            bool push_front(value_type val){
                std::unique_lock<std::mutex> loc(m_);
                if (head_ == tail_ && !empty_queue_ ){
                    return -1;
                }
                empty_queue_ = false;
                int new_head = (head_ == 0)? (size_-1) : (head_ -1);
                head_ = new_head;                         
                loc.unlock();
                
                return push_front_(val,new_head);
            }

            bool push_front_or_wait_for(value_type val, int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag  = cv_can_push_.wait_for(loc,std::chrono::seconds(s),[this](){return this->head_ != this->tail_ || this->empty_queue_;}); // head != tail implica sicuro non pieno, poi empty_queue perche head==tail magari per vuoto
                if(!flag){return false;}
            
                empty_queue_ = false;
                int new_head = (head_ == 0)? (size_-1) : (head_ -1);
                head_ = new_head; 
                loc.unlock();
                
                return push_front_(val,new_head);
            }

            bool push_front_or_wait(value_type val){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_push_.wait(loc,[this](){return this->head_ != this->tail_ || this->empty_queue_;});
                
                empty_queue_ = false;
                int new_head = (head_ == 0)? (size_-1) : (head_ -1);
                head_ = new_head; 
                loc.unlock();

                return push_front_(val,new_head);
            }

            std::optional<value_type> pop_front(){
                std::unique_lock<std::mutex> loc(m_);
                if(empty_queue_ ){
                    return -1;
                }

                int h = head_; 
                head_ = (head_ == size_-1)? (0):(head_+1);
                if(head_==tail_) {empty_queue_ = true;}
                queue_[h].count_pop_ ++;
                loc.unlock();

                return pop_front_(int h);
            }

            std::optional<value_type> pop_front_or_wait_for(int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag = cv_can_pop_.wait_for(loc,std::chrono::seconds(s),[this](){return !this->empty_queue_;});
                if(!flag){return std::nullopt;}

                int h = head_; 
                head_ = (head_ == size_-1)? (0):(head_+1);
                if(head_==tail_) {empty_queue_ = true;}
                queue_[h].count_pop_ ++;
                loc.unlock();
                
                return pop_front_(int h);
            }

            std::optional<value_type> pop_front_or_wait(){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_pop_.wait(loc,[this](){return !this->empty_queue_;});

                int h = head_; 
                head_ = (head_ == size_-1)? (0):(head_+1);
                if(head_==tail_) {empty_queue_ = true;}
                queue_[h].count_pop_ ++;
                loc.unlock();
                
                return pop_front_(int h);
            }
             
            bool push_back(value_type val){
                std::unique_lock<std::mutex> loc(m_);    
                if (head_ == tail_ && !empty_queue_ ){return -1;}

                empty_queue_ = false; //maybe already false, so redundant, but avoids if(empty_queue_) {empty_queue_ = false;} 
                int t = tail_; //index to return
                tail_ = (tail_ == size_-1)? (0) : (tail_ + 1); //tail_++
                loc.unlock();

                return push_back_(val,t);
            }

            bool push_back_or_wait_for(value_type val, int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag = cv_can_push_.wait_for(loc,std::chrono::seconds(s),[this](){return this->head_ != this->tail_ || this->empty_queue_;});
                if(!flag){return false;}

                empty_queue_ = false; //maybe already false, so redundant, but avoids if(empty_queue_) {empty_queue_ = false;} 
                int t = tail_; //index to return
                tail_ = (tail_ == size_-1)? (0) : (tail_ + 1); //tail_++
                loc.unlock();

                return push_back_(val,t);
            }

            bool push_back_or_wait(value_type val){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_push_.wait(loc,[this](){return this->head_ != this->tail_ || this->empty_queue_;});

                empty_queue_ = false; //maybe already false, so redundant, but avoids if(empty_queue_) {empty_queue_ = false;} 
                int t = tail_; //index to return
                tail_ = (tail_ == size_-1)? (0) : (tail_ + 1); //tail_++
                loc.unlock();
                
                return push_back_(val,t);
            }

            std::optional<value_type> pop_back(){
                std::unique_lock<std::mutex> loc(m_);
                if (empty_queue_){
                    return -1;
                }

                int new_tail = (tail_== 0)? (size_-1) : (tail_-1);
                tail_ = new_tail; 
                if(head_==tail_) {empty_queue_ = true;} 
                queue_[new_tail].count_pop_ ++;
                loc.unlock();
                
                return pop_back_(new_tail);
            }

            std::optional<value_type> pop_back_or_wait_for(int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag = cv_can_pop_.wait_for(loc,std::chrono::seconds(s),[this](){return !this->empty_queue_;}); 
                //copy of the pop_back() code except for the check if the queue is empty, alternative to a direct call to pop_back which, however, leads to having to use a recursive mutex
                if(!flag){return std::nullopt;}

                int new_tail = (tail_== 0)? (size_-1) : (tail_-1);
                tail_ = new_tail; 
                if(head_==tail_) {empty_queue_ = true;} 
                queue_[new_tail].count_pop_ ++;
                loc.unlock();
                
                return pop_back_(new_tail);
            }

            std::optional<value_type> pop_back_or_wait(){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_pop_.wait(loc,[this](){return !this->empty_queue_;}); 

                int new_tail = (tail_== 0)? (size_-1) : (tail_-1);
                tail_ = new_tail; 
                if(head_==tail_) {empty_queue_ = true;} 
                queue_[new_tail].count_pop_ ++;
                loc.unlock();

                return pop_back_(new_tail);
            }

            bool empty() const {
                std::lock_guard<std::mutex> loc(m_);
                if(empty_queue_){
                    bool not_empty = true;  
                    while(not_empty){
                        std::unique_lock<std::mutex> loc_el(queue_[tail_].m_el_);
                        not_empty = (queue_[tail_].count_pop_ != 0);
                        loc_el.unlock();
                        std::this_thread::yield(); 
                    };
                    return true;
                }
                else
                    return false; 
            }
    };
}
#endif

