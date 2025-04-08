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

    //define concept (iterator of vector,array,list)
    template <typename Iterator, typename T>
    concept vector_array_list = std::contiguous_iterator<Iterator> || std::same_as<Iterator, typename std::list<T>::iterator> ;

    // concept per verificare E aia membri v_ ed state_.  TODO: un po inutile, bisognerebbe verificare che membri v_ e state_ possano fare cio che fanno in costruttore da conteiner
    template <typename E>
    concept value = requires{E::v_;};

    template <typename E>
    concept status = requires{E::state_;};


    enum Memory_order {
        relax, // non aspetta se elem busy
        hold, // aspetta se elem busy (no while ma Condition_Variable)
    };

    //enumerator  state of elem
    static constexpr char Empty = 'e';
    static constexpr char Busy = 'b';
    static constexpr char Full = 'f';

    template<typename T>
    struct elem_relax{
        std::atomic<char> state_ = Empty; //
        std::optional<T> v_;
    };

    template<typename T>
    struct elem_hold{
        std::atomic<char> state_ = Empty; //stato binario Empty / Full  TODO: valutare se meglio atomic flag
        std::optional<T> v_;
        mutable std::mutex m_el_;
        // OSS: due cv necessarie perché garantisce pop e push si alternino (se chiamo su stessa cella push pop push primo push quando finisce notifica se solo una cv puo essere arrivi notifica a push,  e non a pop,che si sveglia vede non piu busy e sovrascrive, se in cv.wait oltre a check se !=busy aggiungo check !=full  cosi si sveglia vede full e torna a dormire -> deadlock di pop che aspetta per sempre)
        std::condition_variable cv_ready_to_pop_; // per avvisare che possibile fare pop (notufy da un push)
        std::condition_variable cv_ready_to_push_; // per avvisare che possibile fare push (notify da un pop)
    };


    template <typename T,Memory_order U, typename E>  //TODO: costrain concept per garantire che E abbia membro state_ e v_, usati per costruttore da iteratori di conteiner
    requires fdapde::value<E> && fdapde::status<E>
    class Worker_queue{
        using value_type = T;

        typedef std::vector<E> container;
        protected:
            container queue_;
            int head_ = 0; //indx of 1 over "first" element
            int tail_ = 0; //indx of "last" element
            int size_ = 0;
            bool empty_queue_ = true; // per distinguere head==tail-> vuota / head==tail-> piena
            mutable std::mutex m_;
            std::condition_variable cv_can_pop_; //notif when element add to queue_, can pop
            std::condition_variable cv_can_push_; //notif when element removed from queue_, can push 
            bool active_ = true;
        public:
            // default constructor credo poi da associare a metodo resize()
            Worker_queue() = default;
            // construct whit size of queue_=n;
            Worker_queue(int n): queue_(n){
                size_ = n;
            }

            //template <std::contiguous_iterator Iterator> per vector e array no list  
            template <typename Iterator>
            requires fdapde::vector_array_list<Iterator,T>
            Worker_queue(Iterator begin, Iterator end){
                int n = std::distance(begin, end); //itertor di list non supportano end-begin
                std::vector<E> temp_queue(n);
                for(int i =0; i<n;i++){
                    temp_queue[i].state_.store(Full);
                    temp_queue[i].v_ = *(begin);
                    std::advance(begin,1);         // list non supporta begin + 1
                }
                std::swap(queue_, temp_queue);
                size_ = queue_.size();
                empty_queue_ = false;
            }
            virtual ~Worker_queue(){ //virtual cosi che distruttore di figli chiamati quado si distruggono tramite puntatore di tipo base
                active_ = false;
                cv_can_pop_.notify_all();
                cv_can_push_.notify_all();
            }

            Worker_queue(const Worker_queue&) = delete;
            void operator=(const Worker_queue&) = delete;

            void resize(int n){
                std::lock_guard<std::mutex> loc(m_);
                std::vector<E> temp_queue(n);
                std::swap(queue_, temp_queue);
                size_ = n;
                head_ = 0;
                tail_ = 0;
                empty_queue_ = true;                
            }

            // wrap of function size() of vector thrade-safe
            int size() const {
                std::lock_guard<std::mutex> loc(m_);
                return queue_.size();
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
                    if(queue_[i].state_.load(std::memory_order_acquire) == Empty){
                        std::cout<<0<<" ";
                    }
                    else{
                        std::cout<<queue_[i].v_.value()<<"  ";
                    }
                }
                std::cout<<std::endl;
            }
    };

    //memory_order relax: se piu thread intervengono su stesssa cella push/pop durate stato= busy ritorna false/nullopt senza aspettare
    template <typename T>
    class Worker_queue_relax : public Worker_queue<T,Memory_order::relax,elem_relax<T>>{
        using value_type = T;
        using Worker_queue<T, Memory_order::relax, elem_relax<T>>::queue_;
        using Worker_queue<T, Memory_order::relax, elem_relax<T>>::head_;
        using Worker_queue<T, Memory_order::relax, elem_relax<T>>::tail_;
        using Worker_queue<T, Memory_order::relax, elem_relax<T>>::size_;
        using Worker_queue<T, Memory_order::relax, elem_relax<T>>::empty_queue_;
        using Worker_queue<T, Memory_order::relax, elem_relax<T>>::m_;
        using Worker_queue<T, Memory_order::relax, elem_relax<T>>::cv_can_pop_;
        using Worker_queue<T, Memory_order::relax, elem_relax<T>>::cv_can_push_;
        using Worker_queue<T, Memory_order::relax, elem_relax<T>>::active_;
        public:
            Worker_queue_relax(int n): Worker_queue<T,Memory_order::relax,elem_relax<T>>(n){};

            template <typename Iterator>
            requires fdapde::vector_array_list<Iterator,T>
            Worker_queue_relax(Iterator begin, Iterator end):Worker_queue<T,Memory_order::relax,elem_relax<T>>(begin, end){};

            bool push_front(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                //TODO: se coda piena forse non serve questo primo check perche tranto fatto poi in singolo elemento ( check stato == full)
                if (head_ == tail_ && !empty_queue_ ){// coda piena
                    std::cerr<<"queue full"<<std::endl; // per debug poi da togliere
                    return false;
                }
                int h = head_; //index dove inserira elemento
                if(queue_[h].state_.load(std::memory_order_acquire) != Empty)
                    return false;
                queue_[h].state_.store(Busy, std::memory_order_release); //TODO: capire se forse dato che dentro mutex memory order superfluo. forse ottimale  memory_order_relax
                head_ = (head_ == size_-1)? (0) : (head_ + 1);

                empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente 
                loc.unlock();

                //push di elemento
                queue_[h].v_ = std::move(t);
                queue_[h].state_.store(Full, std::memory_order_release); //aggiorna stato di elem con release.  SE un pop vede lo stato = FUll grazie a release allora sicuro vede anche l'elemeto pushato

                cv_can_pop_.notify_one(); // for pop_or_wait
                return true; 
            }
            
            //TODO: tutti metodi or wait da capire semantica
            bool push_front_or_wait(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_push_.wait(loc,[this](){return !this->active_ ||  this->head_ != this->tail_ || this->empty_queue_;});
                if(!active_){return false;}
                int h = head_; //index dove inserira elemento
                if(queue_[h].state_.load(std::memory_order_acquire) != Empty)
                    return false;
                queue_[h].state_.store(Busy, std::memory_order_release); //TODO: capire se forse dato che dentro mutex memory order superfluo
                
                head_ = (head_ == size_-1)? (0) : (head_ + 1);
                empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente 
                loc.unlock();
                
                //push di elemento
                queue_[h].v_ = std::move(t);
                queue_[h].state_.store(Full, std::memory_order_release); //aggiorna stato di elem con release

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
                // new_head = index di elemento da rimuovere
                int new_head = (head_== 0)? (size_-1) : (head_-1);

                if(queue_[new_head].state_.load(std::memory_order_acquire) != Full)
                    return std::nullopt;
                queue_[new_head].state_.store(Busy, std::memory_order_release);
                head_ = new_head;
                
                if(head_==tail_) {empty_queue_ = true;}  //head_ ==tail_ after pop() means empty, in general means full
                loc.unlock();

                // pop 
                value_type ret = std::move(queue_[new_head].v_.value());
                queue_[new_head].v_ = std::nullopt;
                queue_[new_head].state_.store(Empty, std::memory_order_release);

                cv_can_push_.notify_one();

                return ret;
                
            }

            std::optional<value_type> pop_front_or_wait(){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_pop_.wait(loc,[this](){return !this->active_ || !this->empty_queue_;});
                if(!active_) return std::nullopt;

                int new_head = (head_== 0)? (size_-1) : (head_-1);
                
                if(queue_[new_head].state_.load(std::memory_order_acquire) != Full)
                    return std::nullopt;
                queue_[new_head].state_.store(Busy, std::memory_order_release);
                head_ = new_head;
                if(head_==tail_) {empty_queue_ = true;}  //head_ ==tail_ after pop() means empty, in general means full
                loc.unlock();

                // pop
                value_type ret = std::move(queue_[new_head].v_.value());
                queue_[new_head].v_ = std::nullopt;
                queue_[new_head].state_.store(Empty, std::memory_order_release);
                
                cv_can_push_.notify_one();

                return ret;
            }
            
            //push_back() thread-safe 
            bool push_back(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                if (head_ == tail_ && !empty_queue_ ){// coda piena
                    std::cerr<<"queue full"<<std::endl; // per debug poi da togliere
                    return false;
                }
                int new_tail = (tail_ == 0)? (size_-1) : (tail_ -1);
            
                if(queue_[new_tail].state_.load(std::memory_order_acquire) != Empty)
                    return false;
                queue_[new_tail].state_.store(Busy, std::memory_order_release);
                tail_ = new_tail;                    
                
                empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente 
                loc.unlock();

                queue_[new_tail].v_ = std::move(t);
                queue_[new_tail].state_.store(Full, std::memory_order_release);
                
                cv_can_pop_.notify_one();
                return true;
            }

            bool push_back_or_wait(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_push_.wait(loc,[this](){return !this->active_ || this->head_ != this->tail_ || this->empty_queue_;});
                if(!active_){return false;}

                int new_tail = (tail_ == 0)? (size_-1) : (tail_ -1);
                
                if(queue_[new_tail].state_.load(std::memory_order_acquire) != Empty)
                    return false;
                queue_[new_tail].state_.store(Busy, std::memory_order_release);                    
                tail_ = new_tail;  
                empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente               
                loc.unlock();

                queue_[new_tail].v_ = std::move(t);
                queue_[new_tail].state_.store(Full, std::memory_order_release);
                
                cv_can_pop_.notify_one();
                return true;
            }

            //pop_back() thrade-safe
            std::optional<value_type> pop_back(){
                std::unique_lock<std::mutex> loc(m_);
                if(empty_queue_ ){
                    std::cerr << "Queue is empty" << std::endl;
                    return std::nullopt;
                }

                int t = tail_; // tmp idice di elmeto da pop
                
                if(queue_[t].state_.load(std::memory_order_acquire) != Full)
                    return std::nullopt;
                queue_[t].state_.store(Busy, std::memory_order_release);
                tail_ = (tail_ == size_-1)? (0):(tail_+1);
                
                if(head_==tail_) {empty_queue_ = true;}
                loc.unlock();

                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = std::move(queue_[t].v_.value());
                queue_[t].v_ = std::nullopt;
                queue_[t].state_.store(Empty, std::memory_order_release);

                cv_can_push_.notify_one();
                return ret;
            }

            std::optional<value_type> pop_back_or_wait(){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_pop_.wait(loc,[this](){return !this->active_ || !this->empty_queue_;}); // loc mutex, controllo condizione in lamda, se falsa unlock mutex e wait se vera va avanti
                //copia codice di pop_back() tranne check se coda vuota, alternativa a chiamata diretta di pop_back che però porta a dover usare recursive mutex (definito dal libro come il male assoluto)
                if(!active_) return std::nullopt; //se chiamato distruttore distruttore notifica a tutti di verificare condizione wait 
                int t = tail_; // tmp idice di elmeto da pop

                if(queue_[t].state_.load(std::memory_order_acquire) != Full)
                    return std::nullopt;
                queue_[t].state_.store(Busy, std::memory_order_release);

                int new_tail = (tail_ == size_-1)? (0):(tail_+1);
                tail_ = new_tail;
                if(head_==tail_) {empty_queue_ = true;}
                loc.unlock();

                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = std::move(queue_[t].v_.value());
                queue_[t].v_ = std::nullopt;
                queue_[t].state_.store(Empty, std::memory_order_release);

                cv_can_push_.notify_one();
                return ret;
            }

            // TODO: togliere while, per il momento lasciato cosi ci pensiamo dopo
            bool empty() const {
                std::lock_guard<std::mutex> loc(m_);
                if(empty_queue_){
                    //TODO //aspetta che ultimo pop tolga effettuvamente l'ultimo elemento
                    return true;
                }
                else
                    return false; 
            }

    };

    //memory_order hold: se piu thread intervengono su stesssa cella push/pop durate stato= busy aspettao
    template <typename T>
    class Worker_queue_hold : public Worker_queue<T,Memory_order::hold,elem_hold<T>>{
        using value_type = T;
        using Worker_queue<T, Memory_order::hold, elem_hold<T>>::queue_;
        using Worker_queue<T, Memory_order::hold, elem_hold<T>>::head_;
        using Worker_queue<T, Memory_order::hold, elem_hold<T>>::tail_;
        using Worker_queue<T, Memory_order::hold, elem_hold<T>>::size_;
        using Worker_queue<T, Memory_order::hold, elem_hold<T>>::empty_queue_;
        using Worker_queue<T, Memory_order::hold, elem_hold<T>>::m_;
        using Worker_queue<T, Memory_order::hold, elem_hold<T>>::cv_can_pop_;
        using Worker_queue<T, Memory_order::hold, elem_hold<T>>::cv_can_push_;
        using Worker_queue<T, Memory_order::hold, elem_hold<T>>::active_;
        public:
            Worker_queue_hold(int n): Worker_queue<T,Memory_order::hold,elem_hold<T>>(n){};

            template <typename Iterator>
            requires fdapde::vector_array_list<Iterator,T>
            Worker_queue_hold(Iterator begin, Iterator end):Worker_queue<T,Memory_order::hold,elem_hold<T>>(begin, end){};

            bool push_front(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                //TODO: se coda piena forse non serve questo primo check perche tranto fatto poi in singolo elemento ( check stato == full)
                if (head_ == tail_ && !empty_queue_ ){// coda piena
                    std::cerr<<"queue full"<<std::endl; // per debug poi da togliere
                    return false;
                }
                int h = head_; //index dove inserira elemento
                head_ = (head_ == size_-1)? (0) : (head_ + 1);
                empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente 
                cv_can_pop_.notify_one(); // for pop_or_wait
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[h].m_el_);
                queue_[h].cv_ready_to_push_.wait(loc_el,[this,h](){return queue_[h].state_.load(std::memory_order_acquire)==Empty;});
                //push di elemento
                queue_[h].v_ = std::move(t);
                queue_[h].state_.store(Full, std::memory_order_release); //aggiorna stato di elem con release
                queue_[h].cv_ready_to_pop_.notify_one();
                loc_el.unlock();
            
                return true; 
            }

            bool push_front_or_wait(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_push_.wait(loc,[this](){return !this->active_ ||  this->head_ != this->tail_ || this->empty_queue_;});
                if(!active_){return false;}
                int h = head_; //index dove inserira elemento
                head_ = (head_ == size_-1)? (0) : (head_ + 1);
                empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente 
                cv_can_pop_.notify_one(); // for pop_or_wait
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[h].m_el_);
                queue_[h].cv_ready_to_push_.wait(loc_el,[this,h](){return queue_[h].state_.load(std::memory_order_acquire)==Empty;});
                //push di elemento
                queue_[h].v_ = std::move(t);
                queue_[h].state_.store(Full, std::memory_order_release); //aggiorna stato di elem con release
                queue_[h].cv_ready_to_pop_.notify_one();
                loc_el.unlock();
            
                return true; 

            }

            std::optional<value_type> pop_front(){
                std::unique_lock<std::mutex> loc(m_);
                if (empty_queue_){
                    std::cerr<<"queue empty"<<std::endl;
                    return std::nullopt;
                }
                // coda non vuota ma magari un push_back ha modificato indici e non ancora inserito elemento (caso critico coda vuota poi push_back poi pop_front)
                // new_head = index di elemento da rimuovere
                int new_head = (head_== 0)? (size_-1) : (head_-1);
                head_ = new_head;
                if(head_==tail_) {empty_queue_ = true;}  //head_ ==tail_ after pop() means empty, in general means full
                cv_can_push_.notify_one();
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[new_head].m_el_);
                queue_[new_head].cv_ready_to_pop_.wait(loc_el,[this,new_head](){return queue_[new_head].state_.load(std::memory_order_acquire)==Full;});
                
                // pop 
                value_type ret = std::move(queue_[new_head].v_.value());
                queue_[new_head].v_ = std::nullopt;
                queue_[new_head].state_.store(Empty, std::memory_order_release);

                queue_[new_head].cv_ready_to_push_.notify_one();
                loc_el.unlock();                

                return ret;
                   
            }

            std::optional<value_type> pop_front_or_wait(){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_pop_.wait(loc,[this](){return !this->active_ || !this->empty_queue_;});
                if(!active_) return std::nullopt;
                // new_head = index di elemento da rimuovere
                int new_head = (head_== 0)? (size_-1) : (head_-1);
                if(head_==tail_) {empty_queue_ = true;}  //head_ ==tail_ after pop() means empty, in general means full
                cv_can_push_.notify_one();
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[new_head].m_el_);
                queue_[new_head].cv_ready_to_pop_.wait(loc_el,[this,new_head](){return queue_[new_head].state_.load(std::memory_order_acquire)==Full;});
                
                // pop 
                value_type ret = std::move(queue_[new_head].v_.value());
                queue_[new_head].v_ = std::nullopt;
                queue_[new_head].state_.store(Empty, std::memory_order_release);

                queue_[new_head].cv_ready_to_push_.notify_one();
                loc_el.unlock();                

                return ret;
                        
            }
            
            //push_back() thread-safe 
            bool push_back(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                if (head_ == tail_ && !empty_queue_ ){// coda piena
                    std::cerr<<"queue full"<<std::endl; // per debug poi da togliere
                    return false;
                }
                int new_tail = (tail_ == 0)? (size_-1) : (tail_ -1);
                empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente 
                tail_ = new_tail;
                cv_can_pop_.notify_one();
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[new_tail].m_el_);
                queue_[new_tail].cv_ready_to_push_.wait(loc_el,[this,new_tail](){return queue_[new_tail].state_.load(std::memory_order_acquire)==Empty;});
                queue_[new_tail].v_ = std::move(t);
                queue_[new_tail].state_.store(Full, std::memory_order_release);
                queue_[new_tail].cv_ready_to_pop_.notify_one();
                loc_el.unlock();

                return true;
            }

            bool push_back_or_wait(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_push_.wait(loc,[this](){return !this->active_ || this->head_ != this->tail_ || this->empty_queue_;});
                if(!active_){return false;}

                int new_tail = (tail_ == 0)? (size_-1) : (tail_ -1);
                empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente 
                tail_ = new_tail;
                cv_can_pop_.notify_one();
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[new_tail].m_el_);
                queue_[new_tail].cv_ready_to_push_.wait(loc_el,[this,new_tail](){return queue_[new_tail].state_.load(std::memory_order_acquire)==Empty;});
                queue_[new_tail].v_ = std::move(t);
                queue_[new_tail].state_.store(Full, std::memory_order_release);
                queue_[new_tail].cv_ready_to_pop_.notify_one();
                loc_el.unlock();
                
                return true;
            }

            //pop_back() thrade-safe
            std::optional<value_type> pop_back(){
                std::unique_lock<std::mutex> loc(m_);
                if(empty_queue_ ){
                    std::cerr << "Queue is empty" << std::endl;
                    return std::nullopt;
                }

                int t = tail_; // tmp idice di elmeto da pop
                tail_ = (tail_ == size_-1)? (0):(tail_+1);
                if(head_==tail_) {empty_queue_ = true;}
                cv_can_push_.notify_one();
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[t].m_el_);
                queue_[t].cv_ready_to_pop_.wait(loc_el,[this,t](){return queue_[t].state_.load(std::memory_order_acquire)==Full;});
                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = std::move(queue_[t].v_.value());
                queue_[t].v_ = std::nullopt;
                queue_[t].state_.store(Empty, std::memory_order_release);
                queue_[t].cv_ready_to_push_.notify_one();
                loc_el.unlock();
                

                return ret;
            }

            std::optional<value_type> pop_back_or_wait(){
                std::unique_lock<std::mutex> loc(m_);
                cv_can_pop_.wait(loc,[this](){return !this->active_ || !this->empty_queue_;}); // loc mutex, controllo condizione in lamda, se falsa unlock mutex e wait se vera va avanti
                //copia codice di pop_back() tranne check se coda vuota, alternativa a chiamata diretta di pop_back che però porta a dover usare recursive mutex (definito dal libro come il male assoluto)
                if(!active_) return std::nullopt; //se chiamato distruttore distruttore notifica a tutti di verificare condizione wait 

                int t = tail_; // tmp idice di elmeto da pop
                tail_ = (tail_ == size_-1)? (0):(tail_+1);
                if(head_==tail_) {empty_queue_ = true;}
                cv_can_push_.notify_one();
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[t].m_el_);
                queue_[t].cv_ready_to_pop_.wait(loc_el,[this,t](){return queue_[t].state_.load(std::memory_order_acquire)==Full;});
                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = std::move(queue_[t].v_.value());
                queue_[t].v_ = std::nullopt;
                queue_[t].state_.store(Empty, std::memory_order_release);
                queue_[t].cv_ready_to_push_.notify_one();
                loc_el.unlock();

                return ret;
            }

            
            bool empty() const {
                std::lock_guard<std::mutex> loc(m_);
                if(empty_queue_){
                    // TODO //aspetta che ultimo pop tolga effettuvamente l'ultimo elemento
                    return true;
                }
                else
                    return false; 
            }
    };

};

#endif