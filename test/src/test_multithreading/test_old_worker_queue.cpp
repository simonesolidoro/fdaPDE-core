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

#include<fdaPDE/multithreading.h>


    //old worker_queue full mutex
    namespace fdapde {
        template <typename T> 
        class  old_Worker_queue{
        using value_type= T;
        typedef std::vector<value_type> container;
            private:
                container queue_;
                int head_; //indx of 1 over "first" element
                int tail_; //indx of "last" element
                int size_;
                bool empty_queue_; 
                std::mutex m_;
            public:
                // default constructor credo poi da associare a metodo resize()
                old_Worker_queue(){
                    head_ = 0;
                    tail_ = 0;
                    empty_queue_ = true;
                };
                // construct whit size of queue_=n;
                old_Worker_queue(int n){
                    queue_.resize(n);
                    head_ = 0;
                    tail_ = 0;
                    size_ = n;
                    empty_queue_ = true;
                }
                // TODO: implement a constructor that takes as input a vector of value_type?
    
                old_Worker_queue(const old_Worker_queue&) = delete;
                void operator=(const old_Worker_queue&) = delete;
    
    
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
    
                value_type pop_front(){
                    std::lock_guard<std::mutex> loc(m_);
                    if (empty_queue_){
                        std::cerr<<"queue empty"<<std::endl;
                        return value_type();
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
                value_type pop_back(){
                    std::lock_guard<std::mutex> loc(m_);
    
                    if(empty_queue_ == true){
                        std::cerr << "Queue is empty" << std::endl;
                        return value_type();
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

using value = std::string;


// push_front di n elementi per old_worker_queue
void push_front_di_n_elem_o(fdapde::old_Worker_queue<value> & q,int n, value el){
    for (int j=0; j<n; j++){
        q.push_front(el);
    }
};


// push_back di n elementi per old_worker_queue 
void push_back_di_n_elem_o(fdapde::old_Worker_queue<value> & q,int n, value el){
    for (int j=0; j<n; j++){
        q.push_back(el);
    }
};


// pop_back di n elementi per old_worker_queue 
void pop_back_di_n_elem_o(fdapde::old_Worker_queue<value> & q,int n){
    for (int j=0; j<n; j++){
        q.pop_back();
    }
};


// pop_front di n elementi per old_worker_queue 
void pop_front_di_n_elem_o(fdapde::old_Worker_queue<value> & q,int n){
    for (int j=0; j<n; j++){
        q.pop_front();
    }
};

int main(){
    int size_coda= 1600;
    int n_thread = 8;
    int n_singolo= size_coda / n_thread;


//push_back da piu thread e push_front da singolo
    fdapde::old_Worker_queue<value> w(size_coda);
    
    auto start8 = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> thread_pool_o;
    for (int j=0; j<n_thread-1; j++){
        thread_pool_o.emplace_back(push_back_di_n_elem_o,std::ref(w),n_singolo, "ciao");
    }
    thread_pool_o.emplace_back(push_front_di_n_elem_o,std::ref(w),n_singolo, "ciao");

    for (int k= 0; k<n_thread; k++){
        thread_pool_o[k].join();
    }   
    auto end8 = std::chrono::high_resolution_clock::now();
    auto duration8 = std::chrono::duration_cast<std::chrono::microseconds>(end8 - start8);  
    //std::cout<<"push in old_worker di n_elementi: "<<size_coda<<" con n_thread_back: "<<n_thread-1<<" e un thread_front impiegato:"<<duration8.count()<< " microsecondi\n"; 
    std::cout<<duration8.count()<<",";

//pop_back da piu thread e pop_front da singolo
    fdapde::old_Worker_queue<value> w2(size_coda);

    //popolo
    for (int i=0; i<size_coda; i++){
        w2.push_front("ciao");
    }


    auto start9 = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> thread_pool_o2;
    for (int j=0; j<n_thread-1; j++){
        thread_pool_o2.emplace_back(pop_back_di_n_elem_o,std::ref(w2),n_singolo);
    }
    thread_pool_o2.emplace_back(pop_front_di_n_elem_o,std::ref(w2),n_singolo);

    for (int k= 0; k<n_thread; k++){
        thread_pool_o2[k].join();
    }   
    auto end9 = std::chrono::high_resolution_clock::now();
    auto duration9 = std::chrono::duration_cast<std::chrono::microseconds>(end9 - start9);  
    //std::cout<<"pop in old_worker di n_elementi: "<<size_coda<<" con n_thread_back: "<<n_thread-1<<" e un thread_front impiegato:"<<duration9.count()<< " microsecondi\n"; 
    std::cout<<duration9.count()<<",";

    return 0;
}

