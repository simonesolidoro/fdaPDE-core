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


namespace internals {

//define concept (iterator of vector,array,list)
template <typename Iterator, typename T>
concept vector_array_list = std::contiguous_iterator<Iterator> || std::same_as<Iterator, typename std::list<T>::iterator> ;

//forward declaration
template<typename T>
struct elem_relax;

template<typename T>
struct elem_hold;

// classe base 
//TODO: inutile 3 template argomenti, basta tipo elemento (E) che sottointende memory_order. togliere Memory_order U e modificare codice di conseguenza
template <typename T, typename E>  //TODO: costrain concept per garantire che E abbia membro state_ e v_, usati per costruttore da iteratori di conteiner
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
    //OSS: CORREZIONE: basta ache qui 1 cv, perchè simile a cv_ready_el vedi appunti per spiegazione
    std::condition_variable cv_can_now_;
    bool active_ = true; //TODO: valutare se ora che or_wait meotdi hanno wait_for serve ancora active e distruttore, penso di si.
public:
    //enumerator  state of elem
    static constexpr char Empty = 'e';
    static constexpr char Busy = 'b';
    static constexpr char Full = 'f';
    // default constructor credo poi da associare a metodo resize()
    Worker_queue() = default;
    // construct whit size of queue_=n;
    Worker_queue(int n): queue_(n){
        size_ = n;
    }

    //template <std::contiguous_iterator Iterator> per vector e array no list
    template <typename Iterator>
    requires internals::vector_array_list<Iterator,T>
    Worker_queue(Iterator begin, Iterator end){
        int n = std::distance(begin, end); //itertor di list non supportano end-begin
        std::vector<E> temp_queue(n);
        for(int i =0; i<n;i++){
            if constexpr(std::is_same_v<E,fdapde::internals::elem_relax<T>>)
                temp_queue[i].state_.store(Full);
            if constexpr(std::is_same_v<E,fdapde::internals::elem_hold<T>>)    
                temp_queue[i].state_ = false;
            temp_queue[i].v_ = *(begin);
            std::advance(begin,1);         // list non supporta begin + 1
        }
        std::swap(queue_, temp_queue);
        size_ = queue_.size();
        empty_queue_ = false;
    }
    virtual ~Worker_queue(){ //virtual cosi che distruttore di figli chiamati quado si distruggono tramite puntatore di tipo base
        active_ = false;
        cv_can_now_.notify_all();
        //TODO: valutare se serve notitficare a tutti le cv_ready_el_, e quindi aggiungere if(!active){return ...;} nei push/pop. credo di no perche in hold cv_ready_el si mette in attesa solo se ha gia la certezza che si potra svegliare (perche promessa di intervento su elemento fatta in movimento indici)
        //forse creare altra classe queue_guard che wrap worker_queue e fa raii (come mutex e lock_guard), al posto di fare raii in distruttore di stessa classe (come thread)  
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
            if constexpr(std::is_same_v<E,fdapde::internals::elem_relax<T>>){    
                if(queue_[i].state_.load(std::memory_order_acquire) == Empty){
                    std::cout<<0<<" ";
                }
                else{
                    std::cout<<queue_[i].v_.value()<<"  ";
                }
            }
            if constexpr(std::is_same_v<E,fdapde::internals::elem_hold<T>>){
                if(queue_[i].state_){
                    std::cout<<0<<" ";
                }
                else{
                    std::cout<<queue_[i].v_.value()<<"  ";
                }
            }
        }
        std::cout<<std::endl;
    }
};
template<typename T>
struct elem_relax{
    std::atomic<char> state_ = fdapde::internals::Worker_queue<T,elem_relax>::Empty; //
    std::optional<T> v_;
};

template<typename T>
struct elem_hold{
    bool state_ = true; //stato binario true = empty / false = full
    std::optional<T> v_;
    mutable std::mutex m_el_;
    // OSS: due cv necessarie perché garantisce pop e push si alternino (se chiamo su stessa cella push pop push primo push quando finisce notifica se solo una cv puo essere arrivi notifica a push,  e non a pop,che si sveglia vede non piu busy e sovrascrive, se in cv.wait oltre a check se !=busy aggiungo check !=full  cosi si sveglia vede full e torna a dormire -> deadlock di pop che aspetta per sempre)
    //OSS: NO!! CORREZIONE: basta una cv, perchè non puo verificarsi che un pop e un push sulla stessa cella dormano contemporanemante( i realta puo ma non è un problema, vedi fine osservazione), unico problema se secondo push va a dormire perche sblocca mutex di el tra la notify_one() di primo push e il blocco del mutex del pop, vede full e va a dormire, ed allora push e pop dormono insieme ma ormai notify_one è stato lanciato e non puo andare a svegliare thread che si è messo a dormire dopo il lancio,
    // fonte di ultima affermzzioe cppreference: "This makes it impossible for notify_one() to, for example, be delayed and unblock a thread that started waiting just after the call to notify_one() was made." 
    std::condition_variable cv_ready_el_; 

    std::condition_variable cv_empty_;
    int count_push_ = 0;
    int count_pop_ = 0;
};

}

    //memory_order relax: se piu thread intervengono su stesssa cella push/pop durate stato= busy ritorna false/nullopt senza aspettare
    template <typename T>
    class Worker_queue_relax : public internals::Worker_queue<T, internals::elem_relax<T>>{
        using value_type = T;
        using internals::Worker_queue<T, internals::elem_relax<T>>::queue_;
        using internals::Worker_queue<T, internals::elem_relax<T>>::head_;
        using internals::Worker_queue<T, internals::elem_relax<T>>::tail_;
        using internals::Worker_queue<T, internals::elem_relax<T>>::size_;
        using internals::Worker_queue<T, internals::elem_relax<T>>::empty_queue_;
        using internals::Worker_queue<T, internals::elem_relax<T>>::m_;
        using internals::Worker_queue<T, internals::elem_relax<T>>::cv_can_now_;
        using internals::Worker_queue<T, internals::elem_relax<T>>::active_;
        //enum per state_
        using internals::Worker_queue<T, internals::elem_relax<T>>::Empty;
        using internals::Worker_queue<T, internals::elem_relax<T>>::Full;
        using internals::Worker_queue<T, internals::elem_relax<T>>::Busy;
        public:
            Worker_queue_relax(int n): internals::Worker_queue<T,internals::elem_relax<T>>(n){};

            template <typename Iterator>
            requires fdapde::internals::vector_array_list<Iterator,T>
            Worker_queue_relax(Iterator begin, Iterator end):internals::Worker_queue<T,internals::elem_relax<T>>(begin, end){};

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

                cv_can_now_.notify_one(); // OSS: in "relax" (diversamente da "hold", dove viene fatta in primo mutex di modifica indici) notifica per metodi or_wait fatta alla fine cosi meno probabile che chi si sveglia poi veda ancora stato elemeto == busy 
                return true; 
            }
            
            //TODO: tutti metodi or wait da capire semantica
            bool push_front_or_wait(value_type t, int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag = cv_can_now_.wait_for(loc,std::chrono::seconds(s),[this](){return !this->active_ ||  this->head_ != this->tail_ || this->empty_queue_;});
                if(!active_){return false;}
                if(!flag){return false;}
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

                cv_can_now_.notify_one();
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

                cv_can_now_.notify_one();

                return ret;
                
            }

            std::optional<value_type> pop_front_or_wait(int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag = cv_can_now_.wait_for(loc,std::chrono::seconds(s),[this](){return !this->active_ || !this->empty_queue_;});
                if(!active_) return std::nullopt;
                if(!flag){return std::nullopt;}
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
                
                cv_can_now_.notify_one();

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
                
                cv_can_now_.notify_one();
                return true;
            }

            bool push_back_or_wait(value_type t, int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag = cv_can_now_.wait_for(loc,std::chrono::seconds(s),[this](){return !this->active_ || this->head_ != this->tail_ || this->empty_queue_;});
                if(!active_){return false;}
                if(!flag){return false;}
                int new_tail = (tail_ == 0)? (size_-1) : (tail_ -1);
                
                if(queue_[new_tail].state_.load(std::memory_order_acquire) != Empty)
                    return false;
                queue_[new_tail].state_.store(Busy, std::memory_order_release);                    
                tail_ = new_tail;  
                empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente               
                loc.unlock();

                queue_[new_tail].v_ = std::move(t);
                queue_[new_tail].state_.store(Full, std::memory_order_release);
                
                cv_can_now_.notify_one();
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

                cv_can_now_.notify_one();
                return ret;
            }

            std::optional<value_type> pop_back_or_wait(int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag = cv_can_now_.wait_for(loc,std::chrono::seconds(s),[this](){return !this->active_ || !this->empty_queue_;}); // loc mutex, controllo condizione in lamda, se falsa unlock mutex e wait se vera va avanti
                //copia codice di pop_back() tranne check se coda vuota, alternativa a chiamata diretta di pop_back che però porta a dover usare recursive mutex (definito dal libro come il male assoluto)
                if(!active_) return std::nullopt; //se chiamato distruttore distruttore notifica a tutti di verificare condizione wait 
                if(!flag){return std::nullopt;}
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

                cv_can_now_.notify_one();
                return ret;
            }

            // TODO: togliere while, per il momento lasciato cosi ci pensiamo dopo
            bool empty() const {
                std::lock_guard<std::mutex> loc(m_);
                if(empty_queue_){ //OSS: massimo un pop che deve acora eseguire perchè altri vedono busy e non si mettoo in coda
                    while(queue_[tail_].state_.load(std::memory_order_acquire) != Empty){} //momentaneo while finche non troviamo altro modo
                    return true;
                }
                else
                    return false; 
            }

    };

    //memory_order hold: se piu thread intervengono su stesssa cella push/pop aspettano che precedente finisca
    template <typename T>
    class Worker_queue_hold : public internals::Worker_queue<T,internals::elem_hold<T>>{
        using value_type = T;
        using internals::Worker_queue<T, internals::elem_hold<T>>::queue_;
        using internals::Worker_queue<T, internals::elem_hold<T>>::head_;
        using internals::Worker_queue<T, internals::elem_hold<T>>::tail_;
        using internals::Worker_queue<T, internals::elem_hold<T>>::size_;
        using internals::Worker_queue<T, internals::elem_hold<T>>::empty_queue_; //TODO: (OSS:empty_queue_ serve perchè alternativa giardare stato di elemento queue_[tail_/head_] se tail_ == head_, ma per lettura attendibile servirebbe bloccare mutex su elemento) TODO:valutare se piu conveniente blocco mutex ma elimino if(),  =false e = true fatti su empty_queue_ (metodi piu leggeri)
        using internals::Worker_queue<T, internals::elem_hold<T>>::m_;
        using internals::Worker_queue<T, internals::elem_hold<T>>::cv_can_now_;
        using internals::Worker_queue<T, internals::elem_hold<T>>::active_;
        public:
            Worker_queue_hold(int n): internals::Worker_queue<T,internals::elem_hold<T>>(n){};

            template <typename Iterator>
            requires internals::vector_array_list<Iterator,T>
            Worker_queue_hold(Iterator begin, Iterator end): internals::Worker_queue<T,internals::elem_hold<T>>(begin, end){};
            
            ~Worker_queue_hold(){
                active_ = false;
                for(internals::elem_hold<T> & e : queue_){
                    e.cv_ready_el_.notify_all();
                    e.cv_empty_.notify_all();
                } 
            }
            bool push_front(value_type t){
                std::unique_lock<std::mutex> loc(m_);
                if (head_ == tail_ && !empty_queue_ ){// coda piena
                    std::cerr<<"queue full"<<std::endl; // per debug poi da togliere
                    return false;
                }
                int h = head_; //index dove inserira elemento
                head_ = (head_ == size_-1)? (0) : (head_ + 1); //head_++
                empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente 
                cv_can_now_.notify_one(); // for pop_or_wait
                queue_[h].count_push_ ++;
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[h].m_el_);
                queue_[h].cv_ready_el_.wait(loc_el,[this,h](){return queue_[h].state_;}); // to be sure state_ = true (empty)
                if(!active_){return false;}
                queue_[h].v_ = std::move(t); //push di elemento
                queue_[h].state_ = false; //aggiorna stato di elem a full
                queue_[h].cv_ready_el_.notify_one(); // notifica pop dormiente su stesso elemento
                queue_[h].count_push_ --;
                loc_el.unlock();
            
                return true; 
            }

            bool push_front_or_wait(value_type t, int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag  = cv_can_now_.wait_for(loc,std::chrono::seconds(s),[this](){return !this->active_ ||  this->head_ != this->tail_ || this->empty_queue_;});
                if(!active_){return false;}
                if(!flag){return false;}
                int h = head_; //index dove inserira elemento
                head_ = (head_ == size_-1)? (0) : (head_ + 1);
                empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente 
                cv_can_now_.notify_one(); // for pop_or_wait
                queue_[h].count_push_ ++;
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[h].m_el_);
                queue_[h].cv_ready_el_.wait(loc_el,[this,h](){return queue_[h].state_;});
                if(!active_){return false;}
                //push di elemento
                queue_[h].v_ = std::move(t);
                queue_[h].state_ = false; //aggiorna stato di elem con release
                queue_[h].cv_ready_el_.notify_one();
                queue_[h].count_push_ --;
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
                cv_can_now_.notify_one();
                queue_[new_head].count_pop_ ++;
                loc.unlock();

                //OSS: importate lasciare new_head perche poi head_ potrebbe essere modificata da altri thread
                std::unique_lock<std::mutex> loc_el(queue_[new_head].m_el_);
                queue_[new_head].cv_ready_el_.wait(loc_el,[this,new_head](){return !queue_[new_head].state_;});
                if(!active_) return std::nullopt;
                // pop 
                value_type ret = std::move(queue_[new_head].v_.value());
                queue_[new_head].v_ = std::nullopt;
                queue_[new_head].state_ = true;

                queue_[new_head].cv_ready_el_.notify_one();
                queue_[new_head].count_pop_ --;
                if(queue_[new_head].count_push_ == 0 )
                    queue_[new_head].cv_empty_.notify_one();
                loc_el.unlock();                

                return ret;
                   
            }

            std::optional<value_type> pop_front_or_wait(int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag = cv_can_now_.wait_for(loc,std::chrono::seconds(s),[this](){return !this->active_ || !this->empty_queue_;});
                if(!active_) return std::nullopt;
                if(!flag){return std::nullopt;}
                // new_head = index di elemento da rimuovere
                int new_head = (head_== 0)? (size_-1) : (head_-1);
                head_ = new_head;
                if(head_==tail_) {empty_queue_ = true;}  //head_ ==tail_ after pop() means empty, in general means full
                cv_can_now_.notify_one();
                queue_[new_head].count_pop_ ++;
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[new_head].m_el_);
                queue_[new_head].cv_ready_el_.wait(loc_el,[this,new_head](){return !queue_[new_head].state_;});
                if(!active_) return std::nullopt;
                // pop 
                value_type ret = std::move(queue_[new_head].v_.value());
                queue_[new_head].v_ = std::nullopt;
                queue_[new_head].state_ = true;

                queue_[new_head].cv_ready_el_.notify_one();
                queue_[new_head].count_pop_ --;
                if(queue_[new_head].count_push_ == 0 )
                    queue_[new_head].cv_empty_.notify_one();
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
                cv_can_now_.notify_one();
                queue_[new_tail].count_push_ ++;
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[new_tail].m_el_);
                queue_[new_tail].cv_ready_el_.wait(loc_el,[this,new_tail](){return queue_[new_tail].state_;});
                if(!active_){return false;}
                queue_[new_tail].v_ = std::move(t);
                queue_[new_tail].state_ = false;
                queue_[new_tail].cv_ready_el_.notify_one();
                queue_[new_tail].count_push_ --;
                loc_el.unlock();

                return true;
            }

            bool push_back_or_wait(value_type t, int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag = cv_can_now_.wait_for(loc,std::chrono::seconds(s),[this](){return !this->active_ || this->head_ != this->tail_ || this->empty_queue_;});
                if(!active_){return false;}
                if(!flag){return false;}
                int new_tail = (tail_ == 0)? (size_-1) : (tail_ -1);
                empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente 
                tail_ = new_tail;
                cv_can_now_.notify_one();
                queue_[new_tail].count_push_ ++;
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[new_tail].m_el_);
                queue_[new_tail].cv_ready_el_.wait(loc_el,[this,new_tail](){return queue_[new_tail].state_;});
                if(!active_){return false;}
                queue_[new_tail].v_ = std::move(t);
                queue_[new_tail].state_ = false;
                queue_[new_tail].cv_ready_el_.notify_one();
                queue_[new_tail].count_push_ --;
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
                cv_can_now_.notify_one();
                queue_[t].count_pop_ ++;
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[t].m_el_);
                queue_[t].cv_ready_el_.wait(loc_el,[this,t](){return !queue_[t].state_;});
                if(!active_) return std::nullopt;
                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = std::move(queue_[t].v_.value());
                queue_[t].v_ = std::nullopt;
                queue_[t].state_ = true;
                queue_[t].cv_ready_el_.notify_one();
                queue_[t].count_pop_ --;
                if(queue_[t].count_push_ == 0 )
                    queue_[t].cv_empty_.notify_one();
                loc_el.unlock();
                

                return ret;
            }

            std::optional<value_type> pop_back_or_wait(int s){
                std::unique_lock<std::mutex> loc(m_);
                bool flag = cv_can_now_.wait_for(loc,std::chrono::seconds(s),[this](){return !this->active_ || !this->empty_queue_;}); // loc mutex, controllo condizione in lamda, se falsa unlock mutex e wait se vera va avanti
                //copia codice di pop_back() tranne check se coda vuota, alternativa a chiamata diretta di pop_back che però porta a dover usare recursive mutex (definito dal libro come il male assoluto)
                if(!active_) return std::nullopt; //se chiamato distruttore distruttore notifica a tutti di verificare condizione wait 
                if(!flag){return std::nullopt;}
                int t = tail_; // tmp idice di elmeto da pop
                tail_ = (tail_ == size_-1)? (0):(tail_+1);
                if(head_==tail_) {empty_queue_ = true;}
                cv_can_now_.notify_one();
                queue_[t].count_pop ++;
                loc.unlock();

                std::unique_lock<std::mutex> loc_el(queue_[t].m_el_);
                queue_[t].cv_ready_el_.wait(loc_el,[this,t](){return !queue_[t].state_;});
                if(!active_) return std::nullopt;
                // sostituisce in posto che viene liberato il valore di defaul di value_type
                value_type ret = std::move(queue_[t].v_.value());
                queue_[t].v_ = std::nullopt;
                queue_[t].state_ = true;
                queue_[t].cv_ready_el_.notify_one();
                queue_[t].count_pop --;
                if(queue_[t].count_push == 0 )
                    queue_[t].cv_empty_.notify_one();
                loc_el.unlock();

                return ret;
            }

            
            bool empty()  {
                std::lock_guard<std::mutex> loc(m_);
                if(empty_queue_){
                    std::unique_lock<std::mutex> loc_el(queue_[tail_].m_el_);
                    queue_[tail_].cv_empty_.wait(loc_el,[this](){return queue_[tail_].count_pop_ == 0;});
                    return true;
                }
                else
                    return false; 
            }
    };

};

#endif