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

#ifndef __FDAPDE_SYNCHRO_QUEUE_3_H__
#define __FDAPDE_SYNCHRO_QUEUE_3_H__

#include "header_check.h"

namespace fdapde{
    namespace internals {
        //define concept (iterator of vector,array,list)
        template <typename Iterator, typename T>
        concept vector_array_list = std::contiguous_iterator<Iterator> || std::same_as<Iterator, typename std::list<T>::iterator> ;
    }

    struct relax_nowait{};
    struct hold_nowait{};
    struct hold_wait{};
    //forward declaration
    template<typename T, typename E> class Synchro_queue;

    //reso elem struct template cosi da poter usare stesse template helper fuction e passare come argomento diverso elem
    template<typename T,typename M>
    struct elem;

    template<typename T>
    struct elem<T,relax_nowait>{
        std::atomic<char> state_ = Synchro_queue<T,relax_nowait>::Empty; 
        std::optional<T> v_;
    };

    template<typename T>
    struct elem<T,hold_nowait>{
        bool state_ = true; //stato binario true = empty / false = full
        std::optional<T> v_;
        mutable std::mutex m_el_;
        std::condition_variable cv_ready_to_push_;
        std::condition_variable cv_ready_to_pop_; 
        int count_pop_ = 0; // per ora lasciati da vedere se possibile con stato intermedio //OSS: tolto count_push tanto non serve basta verificare che count_pop sia a 0. prima serviva perche la cv_empty andava notificata da pop solo se count_push = 0. scherzo non serviva nemmeno prima bastava notificare quando count_pop era =0
    };

    template<typename T>
    struct elem<T,hold_wait> : elem<T,hold_nowait>{};


    //forward declaration of helper function: indx TODO: capire se meglio togliere friend e passare solo emmbri interessati con reference
    template<typename T,typename M> 
    int push_f_indx(Synchro_queue<T,M> & S);

    template<typename T,typename M> 
    int pop_f_indx(Synchro_queue<T,M> & S);

    template<typename T, typename M>
    int push_b_indx(Synchro_queue<T,M> & S);

    template<typename T,typename M> 
    int pop_b_indx(Synchro_queue<T,M> & S);

    //forward declaration of helper function: push/pop OSS: non serve che siano friend perche passati membri privati tramite reference
    template<typename T,typename M> 
    void push_fb_push(elem<T,M>& E,T& new_value);

    template<typename T,typename M> 
    T pop_fb_pop(elem<T,M>& E);

    template<typename T>
    class Synchro_queue<T,relax_nowait>{
        using value_type = T;
        
        typedef std::vector<elem<value_type,relax_nowait>> container;
        private:
            container queue_;
            int head_ = 0; //indx of 1 over "first" element
            int tail_ = 0; //indx of "last" element
            int size_ = 0;
            //toltta empty_queue_, per distinguere se piena o vuota in caso head == tail usato stato elemento queeu_[tail]. e poi in push e pop tolto check su stato coda perchè implicito in check stato elemento, empty() modificata cosi che non lo usa piu 
            mutable std::mutex m_;
            //bool active_ = true; non serve in relax_nowait 

        public:
            //enumerator state of elem.  
            static constexpr char Empty = 'e';
            static constexpr char Busy = 'b';
            static constexpr char Full = 'f';
            // default constructor 
            Synchro_queue() = default;
            // construct whit size of queue_=n;
            Synchro_queue(int n): queue_(n){
                size_ = n;
            }
        
            //construct from vector, array, list
            template <typename Iterator>
            requires internals::vector_array_list<Iterator,T>
            Synchro_queue(Iterator begin, Iterator end){
                int n = std::distance(begin, end); //itertor di list non supportano end-begin
                std::vector<elem<T,relax_nowait>> temp_queue(n);
                for(int i =0; i<n;i++){
                    temp_queue[i].state_.store(Full);
                    temp_queue[i].v_ = *(begin);
                    std::advance(begin,1);         // list non supporta begin + 1
                }
                std::swap(queue_, temp_queue);
                size_ = queue_.size();
            }
        
            Synchro_queue(const Synchro_queue&) = delete;
            void operator=(const Synchro_queue&) = delete;
        
            void resize(int n){
                std::lock_guard<std::mutex> loc(m_);
                std::vector<elem<T,relax_nowait>> temp_queue(n);
                std::swap(queue_, temp_queue);
                size_ = n;
                head_ = 0;
                tail_ = 0;
            }
        
            
            int get_size() const {
                return size_;
            }
        
            // svuota queue_
            void clear(){
                std::lock_guard loc(m_);
                queue_.clear();
                head_ = 0;
                tail_ = 0;
            }
        

            int get_tail()const {return tail_;}
            int get_head()const {return head_;}

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

            //dichiarazioni di amicizia
            friend int push_f_indx<T,relax_nowait>(Synchro_queue<T,relax_nowait> & S);
            friend int pop_f_indx<T,relax_nowait>(Synchro_queue<T,relax_nowait> & S);
            friend int push_b_indx<T,relax_nowait>(Synchro_queue<T,relax_nowait> & S);
            friend int pop_b_indx<T,relax_nowait>(Synchro_queue<T,relax_nowait> & S);


            bool push_front(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                int h = push_f_indx<T,relax_nowait>(*this);
                loc.unlock();
                if(h<0) return false; //check extra dovuto a refactorig, ?=ne vale la pena?. OSS:spostato fuoti da lock perchè non serve fare questo check con mutex lock
                //push di elemento
                push_fb_push<T,relax_nowait>(queue_[h], t);
                return true; 
            }

            std::optional<value_type> pop_front(){
                std::unique_lock<std::mutex> loc(m_);
                int new_head = pop_f_indx<T,relax_nowait>(*this);
                loc.unlock();
                if(new_head<0) return std::nullopt;
                // pop 
                value_type ret = pop_fb_pop<T,relax_nowait>(queue_[new_head]);
                return ret;
            }

            bool push_back(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                int new_tail = push_b_indx<T,relax_nowait>(*this);
                loc.unlock();
                if(new_tail<0) return false;

                push_fb_push<T,relax_nowait>(queue_[new_tail], t);                
                return true;
            }

            std::optional<value_type> pop_back(){
                std::unique_lock<std::mutex> loc(m_);
                int t = pop_b_indx<T,relax_nowait>(*this);
                loc.unlock();
                if(t<0) return std::nullopt;

                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = pop_fb_pop<T,relax_nowait>(queue_[t]);

                return ret;
            }

            bool empty() const {
                std::lock_guard<std::mutex> loc(m_);
                if(head_ == tail_ && queue_[tail_].state_.load(std::memory_order_acquire)!=Full){ //OSS: massimo un pop che deve acora eseguire perchè altri vedono busy e non si mettoo in coda
                    while(queue_[tail_].state_.load(std::memory_order_acquire) != Empty){} //momentaneo while finche non troviamo altro modo
                    return true;
                }
                else
                    return false; 
            }

    };


    template <typename T>  
    class Synchro_queue<T,hold_nowait>{
        using value_type = T;

        typedef std::vector<elem<value_type,hold_nowait>> container;
        private:
            container queue_;
            int head_ = 0; //indx of 1 over "first" element
            int tail_ = 0; //indx of "last" element
            int size_ = 0;
            bool empty_queue_ = true; // per distinguere head==tail-> vuota / head==tail-> piena. OSS: qui tenuto al contraio di relax_nowait, perchè stato di elemento puo cambiare perchè modificato fuori da mutex (bisognrebbe mettere contatori e un while per aspettare tutte le modifiche, ma insensato), invece empty_queue_ modificato nel primo mutex quindi la sua lettura nel primo mutex è affidabile
            mutable std::mutex m_;
            bool active_ = true; //TODO: valutare se ora che or_wait meotdi hanno wait_for serve ancora active e distruttore, penso di si. SI: active e meccanismo tipo raii serve per sicurezza in caso si throw exception
        public:
            // default constructor credo poi da associare a metodo resize()
            Synchro_queue() = default;
            // construct whit size of queue_=n;
            Synchro_queue(int n): queue_(n){
                size_ = n;
            }

            // constructor from list array vector
            template <typename Iterator>
            requires internals::vector_array_list<Iterator,T>
            Synchro_queue(Iterator begin, Iterator end){
                int n = std::distance(begin, end); //itertor di list non supportano end-begin
                std::vector<elem<value_type,hold_nowait>> temp_queue(n);
                for(int i =0; i<n;i++){
                    temp_queue[i].state_ = false;
                    temp_queue[i].v_ = *(begin);
                    std::advance(begin,1);         // list non supporta begin + 1
                }
                std::swap(queue_, temp_queue);
                size_ = queue_.size();
                empty_queue_ = false;
            }

            ~Synchro_queue(){
                active_ = false;
                for(elem<T,hold_nowait> & e : queue_){
                    std::lock_guard<std::mutex> loc(e.m_el_); //add lock mutex per avere notifica coerente
                    e.cv_ready_to_pop_.notify_all();
                    e.cv_ready_to_push_.notify_all();
                } 
            };


            Synchro_queue(const Synchro_queue&) = delete;
            void operator=(const Synchro_queue&) = delete;

            void resize(int n){
                std::lock_guard<std::mutex> loc(m_);
                std::vector<elem<value_type,hold_nowait>> temp_queue(n);
                std::swap(queue_, temp_queue);
                size_ = n;
                head_ = 0;
                tail_ = 0;
                empty_queue_ = true;
            }

            // wrap of function size() of vector thrade-safe
            int get_size() const {
                return size_;
            }

            // svuota queue_
            void clear(){
                std::lock_guard loc(m_);
                queue_.clear();
                head_ = 0;
                tail_ = 0;
                empty_queue_ = true;
            }


            int get_tail()const {return tail_;}
            int get_head()const {return head_;}
            //per debug momentanei
            void print(){
                for (int i=0; i<size_; i++){
                    if(queue_[i].state_){
                        std::cout<<0<<" ";
                    }
                    else{
                        std::cout<<queue_[i].v_.value()<<"  ";
                    }
                }
                std::cout<<std::endl;
            }

            //dichiarazioni di amicizia
            friend int push_f_indx<T,hold_nowait>(Synchro_queue<T,hold_nowait> & S);
            friend int pop_f_indx<T,hold_nowait>(Synchro_queue<T,hold_nowait> & S);
            friend int push_b_indx<T,hold_nowait>(Synchro_queue<T,hold_nowait> & S);
            friend int pop_b_indx<T,hold_nowait>(Synchro_queue<T,hold_nowait> & S);

            bool push_front(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                int h = push_f_indx<T,hold_nowait>(*this);
                loc.unlock();
                if(h<0) return false;

                std::unique_lock<std::mutex> loc_el(queue_[h].m_el_);
                queue_[h].cv_ready_to_push_.wait(loc_el,[this,h](){return queue_[h].state_ || !active_;}); // to be sure state_ = true (empty)
                if(!active_){return false;}
                push_fb_push<T,hold_nowait>(queue_[h],t);
                loc_el.unlock();
                return true; 
            }

            std::optional<value_type> pop_front(){
                std::unique_lock<std::mutex> loc(m_);
                int new_head = pop_f_indx<T,hold_nowait>(*this);
                loc.unlock();
                if(new_head<0) return std::nullopt;

                //OSS: importate lasciare new_head perche poi head_ potrebbe essere modificata da altri thread
                std::unique_lock<std::mutex> loc_el(queue_[new_head].m_el_);
                queue_[new_head].cv_ready_to_pop_.wait(loc_el,[this,new_head](){return !queue_[new_head].state_ || !active_;});
                if(!active_) return std::nullopt;
                // pop 
                value_type ret = pop_fb_pop<T,hold_nowait>(queue_[new_head]);
                loc_el.unlock();                
                return ret;                  
            }

            bool push_back(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                int new_tail = push_b_indx<T,hold_nowait>(*this);
                loc.unlock();
                if(new_tail<0) return false;

                std::unique_lock<std::mutex> loc_el(queue_[new_tail].m_el_);
                queue_[new_tail].cv_ready_to_push_.wait(loc_el,[this,new_tail](){return queue_[new_tail].state_ || !active_;});
                if(!active_){return false;}
                push_fb_push<T,hold_nowait>(queue_[new_tail],t);
                loc_el.unlock();

                return true;
            }

            std::optional<value_type> pop_back(){
                std::unique_lock<std::mutex> loc(m_);
                int t = pop_b_indx<T,hold_nowait>(*this);
                loc.unlock();
                if(t<0) return std::nullopt;

                std::unique_lock<std::mutex> loc_el(queue_[t].m_el_);
                queue_[t].cv_ready_to_pop_.wait(loc_el,[this,t](){return !queue_[t].state_ || !this->active_;}); //TODO: possiile non passare a lambda tutto this e h ma solo queue_[t]
                if(!active_) return std::nullopt;
                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = pop_fb_pop<T,hold_nowait>(queue_[t]);
                loc_el.unlock();
                

                return ret;
            }

            bool empty() const {
                std::lock_guard<std::mutex> loc(m_);
                if(empty_queue_){
                    while(queue_[tail_].count_pop_ != 0){}; //wait untile count_pop == 0 OSS: while al posto di condition variable, ma contatori lasciati perche altrimenti stao == empty anche tra un pop e un psuh successivo gia in coda
                    return true;
                }
                else
                    return false; 
            }
    };

    
    template <typename T>  
    class Synchro_queue<T, hold_wait>{
        using value_type = T;

        typedef std::vector<elem<value_type,hold_wait>> container;
        private:
            container queue_;
            int head_ = 0; //indx of 1 over "first" element
            int tail_ = 0; //indx of "last" element
            int size_ = 0;
            bool empty_queue_ = true; // per distinguere head==tail-> vuota / head==tail-> piena
            mutable std::mutex m_;
            std::condition_variable cv_can_pop_;
            std::condition_variable cv_can_push_;
            bool active_ = true; //TODO: valutare se ora che or_wait meotdi hanno wait_for serve ancora active e distruttore, penso di si. SI: active e meccanismo tipo raii serve per sicurezza in caso si throw exception
        public:
            // default constructor credo poi da associare a metodo resize()
            Synchro_queue() = default;
            // construct whit size of queue_=n;
            Synchro_queue(int n): queue_(n){
                size_ = n;
            }

            // constructor from list array vector
            template <typename Iterator>
            requires internals::vector_array_list<Iterator,T>
            Synchro_queue(Iterator begin, Iterator end){
                int n = std::distance(begin, end); //itertor di list non supportano end-begin
                std::vector<elem<value_type,hold_wait>> temp_queue(n);
                for(int i =0; i<n;i++){
                    temp_queue[i].state_ = false;
                    temp_queue[i].v_ = *(begin);
                    std::advance(begin,1);         // list non supporta begin + 1
                }
                std::swap(queue_, temp_queue);
                size_ = queue_.size();
                empty_queue_ = false;
            }
            ~Synchro_queue(){ 
                std::lock_guard<std::mutex> loc(m_);
                active_ = false;
                cv_can_pop_.notify_all();
                cv_can_push_.notify_all();  
                for(elem<T,hold_wait> & e : queue_){
                    std::lock_guard<std::mutex> loc_el(e.m_el_);
                    active_ = false; //TODO: dubbio: ripetuto perchè non certo che modifica in mutex globale garantisca visione coerente in mutex singoli elementi 
                    e.cv_ready_to_pop_.notify_all();
                    e.cv_ready_to_push_.notify_all();
                } 
            }

            Synchro_queue(const Synchro_queue&) = delete;
            void operator=(const Synchro_queue&) = delete;

            void resize(int n){
                std::lock_guard<std::mutex> loc(m_);
                std::vector<elem<value_type,hold_wait>> temp_queue(n);
                std::swap(queue_, temp_queue);
                size_ = n;
                head_ = 0;
                tail_ = 0;
                empty_queue_ = true;
            }

            // 
            int get_size() const {
                return size_;
            }

            // svuota queue_
            void clear(){
                std::lock_guard loc(m_);
                queue_.clear();
                head_ = 0;
                tail_ = 0;
                empty_queue_ = true;
            }


            int get_tail()const {return tail_;}
            int get_head()const {return head_;}
            
            //per debug momentanei
            void print(){
                for (int i=0; i<size_; i++){
                    if(queue_[i].state_){
                        std::cout<<0<<" ";
                    }
                    else{
                        std::cout<<queue_[i].v_.value()<<"  ";
                    }
                }
                std::cout<<std::endl;
            }

            //dichiarazioni di amicizia
            friend int push_f_indx<T,hold_wait>(Synchro_queue<T,hold_wait> & S);
            friend int pop_f_indx<T,hold_wait>(Synchro_queue<T,hold_wait> & S);
            friend int push_b_indx<T,hold_wait>(Synchro_queue<T,hold_wait> & S);
            friend int pop_b_indx<T,hold_wait>(Synchro_queue<T,hold_wait> & S);

            bool push_front(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                int h = push_f_indx<T,hold_wait>(*this);
                loc.unlock();
                if(h<0) return false;

                std::unique_lock<std::mutex> loc_el(queue_[h].m_el_);
                queue_[h].cv_ready_to_push_.wait(loc_el,[this,h](){return queue_[h].state_ || !active_;}); // to be sure state_ = true (empty)
                if(!active_){return false;}
                push_fb_push<T,hold_wait>(queue_[h],t);
                loc_el.unlock();
            
                return true; 
            }

            bool push_front_or_wait_for(value_type t, int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag  = cv_can_push_.wait_for(loc,std::chrono::seconds(s),[this](){return !this->active_ ||  this->head_ != this->tail_ || this->empty_queue_;});
                if(!active_){return false;}
                if(!flag){return false;}
                int h = push_f_indx<T,hold_wait>(*this);
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[h].m_el_);
                queue_[h].cv_ready_to_push_.wait(loc_el,[this,h](){return queue_[h].state_ || !active_;}); // to be sure state_ = true (empty)
                if(!active_){return false;}
                push_fb_push<T,hold_wait>(queue_[h],t);
                loc_el.unlock();
            
                return true; 

            }

            bool push_front_or_wait(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_push_.wait(loc,[this](){return !this->active_ ||  this->head_ != this->tail_ || this->empty_queue_;});
                if(!active_){return false;}
                int h = push_f_indx<T,hold_wait>(*this);
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[h].m_el_);
                queue_[h].cv_ready_to_push_.wait(loc_el,[this,h](){return queue_[h].state_ || !this->active_;}); // to be sure state_ = true (empty)
                if(!active_){return false;}
                push_fb_push<T,hold_wait>(queue_[h],t);
                loc_el.unlock();
            
                return true; 

            }

            std::optional<value_type> pop_front(){
                std::unique_lock<std::mutex> loc(m_);
                int new_head = pop_f_indx<T,hold_wait>(*this);
                loc.unlock();
                if(new_head<0) return std::nullopt;

                //OSS: importate lasciare new_head perche poi head_ potrebbe essere modificata da altri thread
                std::unique_lock<std::mutex> loc_el(queue_[new_head].m_el_);
                queue_[new_head].cv_ready_to_pop_.wait(loc_el,[this,new_head](){return !queue_[new_head].state_ || !this->active_;});
                if(!active_) return std::nullopt;
                // pop 
                value_type ret = pop_fb_pop<T,hold_wait>(queue_[new_head]); 
                loc_el.unlock();                
                return ret;
                
            }

            std::optional<value_type> pop_front_or_wait_for(int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag = cv_can_pop_.wait_for(loc,std::chrono::seconds(s),[this](){return !this->active_ || !this->empty_queue_;});
                if(!active_) return std::nullopt;
                if(!flag){return std::nullopt;}
                // new_head = index di elemento da rimuovere
                int new_head = pop_f_indx<T,hold_wait>(*this);
                loc.unlock();

                //OSS: importate lasciare new_head perche poi head_ potrebbe essere modificata da altri thread
                std::unique_lock<std::mutex> loc_el(queue_[new_head].m_el_);
                queue_[new_head].cv_ready_to_pop_.wait(loc_el,[this,new_head](){return !queue_[new_head].state_ || !this->active_;});
                if(!active_) return std::nullopt;
                // pop 
                value_type ret = pop_fb_pop<T,hold_wait>(queue_[new_head]);
                loc_el.unlock();                

                return ret;
                        
            }

            std::optional<value_type> pop_front_or_wait(){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_pop_.wait(loc,[this](){return !this->active_ || !this->empty_queue_;});
                if(!active_) return std::nullopt;
                // new_head = index di elemento da rimuovere
                int new_head = pop_f_indx<T,hold_wait>(*this);
                loc.unlock();

                //OSS: importate lasciare new_head perche poi head_ potrebbe essere modificata da altri thread
                std::unique_lock<std::mutex> loc_el(queue_[new_head].m_el_);
                queue_[new_head].cv_ready_to_pop_.wait(loc_el,[this,new_head](){return !queue_[new_head].state_ || !this->active_;});
                if(!active_) return std::nullopt;
                // pop 
                value_type ret = pop_fb_pop<T,hold_wait>(queue_[new_head]);
                loc_el.unlock();                

                return ret;
                        
            }
            
            //push_back() thread-safe 
            bool push_back(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                int new_tail = push_b_indx<T,hold_wait>(*this);
                loc.unlock();
                if(new_tail<0) return false;

                std::unique_lock<std::mutex> loc_el(queue_[new_tail].m_el_);
                queue_[new_tail].cv_ready_to_push_.wait(loc_el,[this,new_tail](){return queue_[new_tail].state_ || !this->active_;});
                if(!active_){return false;}
                push_fb_push<T,hold_wait>(queue_[new_tail],t);
                loc_el.unlock();

                return true;
            }

            bool push_back_or_wait_for(value_type t, int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag = cv_can_push_.wait_for(loc,std::chrono::seconds(s),[this](){return !this->active_ || this->head_ != this->tail_ || this->empty_queue_;});
                if(!active_){return false;}
                if(!flag){return false;}
                int new_tail = push_b_indx<T,hold_wait>(*this);
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[new_tail].m_el_);
                queue_[new_tail].cv_ready_to_push_.wait(loc_el,[this,new_tail](){return queue_[new_tail].state_ || !this->active_;});
                if(!active_){return false;}
                push_fb_push<T,hold_wait>(queue_[new_tail],t);
                loc_el.unlock();
                
                return true;
            }

            bool push_back_or_wait(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_push_.wait(loc,[this](){return !this->active_ || this->head_ != this->tail_ || this->empty_queue_;});
                if(!active_){return false;}
                int new_tail = push_b_indx<T,hold_wait>(*this);
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[new_tail].m_el_);
                queue_[new_tail].cv_ready_to_push_.wait(loc_el,[this,new_tail](){return queue_[new_tail].state_ || !this->active_;});
                if(!active_){return false;};
                push_fb_push<T,hold_wait>(queue_[new_tail],t);
                loc_el.unlock();
                
                return true;
            }

            //pop_back() thrade-safe
            std::optional<value_type> pop_back(){
                std::unique_lock<std::mutex> loc(m_);
                int t = pop_b_indx<T,hold_wait>(*this);
                loc.unlock();
                if(t<0) return std::nullopt;

                std::unique_lock<std::mutex> loc_el(queue_[t].m_el_);
                queue_[t].cv_ready_to_pop_.wait(loc_el,[this,t](){return !queue_[t].state_ || !this->active_;});
                if(!active_) return std::nullopt;
                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = pop_fb_pop<T,hold_wait>(queue_[t]);
                loc_el.unlock();
                return ret;
            }

            std::optional<value_type> pop_back_or_wait_for(int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag = cv_can_pop_.wait_for(loc,std::chrono::seconds(s),[this](){return !this->active_ || !this->empty_queue_;}); // loc mutex, controllo condizione in lamda, se falsa unlock mutex e wait se vera va avanti
                //copia codice di pop_back() tranne check se coda vuota, alternativa a chiamata diretta di pop_back che però porta a dover usare recursive mutex (definito dal libro come il male assoluto)
                if(!active_) return std::nullopt; //se chiamato distruttore distruttore notifica a tutti di verificare condizione wait 
                if(!flag){return std::nullopt;}
                int t = pop_b_indx<T,hold_wait>(*this);
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[t].m_el_);
                queue_[t].cv_ready_to_pop_.wait(loc_el,[this,t](){return !queue_[t].state_ || !this->active_;});
                if(!active_) return std::nullopt;
                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = pop_fb_pop<T,hold_wait>(queue_[t]);
                loc_el.unlock();
                return ret;
            }

            std::optional<value_type> pop_back_or_wait(){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_pop_.wait(loc,[this](){return !this->active_ || !this->empty_queue_;}); // loc mutex, controllo condizione in lamda, se falsa unlock mutex e wait se vera va avanti
                //copia codice di pop_back() tranne check se coda vuota, alternativa a chiamata diretta di pop_back che però porta a dover usare recursive mutex (definito dal libro come il male assoluto)
                if(!active_) return std::nullopt; //se chiamato distruttore distruttore notifica a tutti di verificare condizione wait 
                int t = pop_b_indx<T,hold_wait>(*this);
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[t].m_el_);
                queue_[t].cv_ready_to_pop_.wait(loc_el,[this,t](){return !queue_[t].state_ || !this->active_;});
                if(!active_) return std::nullopt;
                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = pop_fb_pop<T,hold_wait>(queue_[t]);
                loc_el.unlock();
                return ret;
            }

            
            bool empty() const {
                std::lock_guard<std::mutex> loc(m_);
                if(empty_queue_){
                    while(queue_[tail_].count_pop_ != 0){}; //wait untile count_pop == 0 OSS: while al posto di condition variable, ma contatori lasciati perche altrimenti stao == empty anche tra un pop e un psuh successivo gia in coda
                    return true;
                }
                else
                    return false; 
            }
    };
}
#include"Synchro_queue_3_helpfun.h"
#endif

