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

/*test per verifica vector usato come array(worker_queue) piu efficente di deque (Worker_queue_deque) */
/*contenuto 
    classe worker_queue con deque (full mutex)
    classe old_Worker_queue (full mutex)
    funzioni per pop/push in for da passare a thread
    main(){
        test push_back multithread (worke vs deque)
        test pop_back miltithread (worke vs deque)
        test push multithtread da back e singolo da front (worke vs deque vs old_worker)
        test pop multithtread da back e singolo da front (worke vs deque vs old_worker)
         }
*/


// worker_queue con deque
template <typename T>
class Worker_queue_deque{
    using value_type= T;
        private:
            std::deque<value_type> queue_;
            int size_; 
            std::mutex m;
        public:

            // void per pigrizia tanto solo per test
            void push_front(value_type t){
                std::lock_guard<std::mutex> loc(m);
                queue_.push_back(t); 
            }

            T pop_front(){
                std::lock_guard<std::mutex> loc(m);
                value_type ret=queue_.back();
                queue_.pop_back();
                return ret;   
            }
            
            //push_back() thread-safe 
            void push_back(value_type t){
                std::lock_guard<std::mutex> loc(m);
                queue_.push_front(t);  
            }

            //pop_back() thrade-safe
            T pop_back(){
                std::lock_guard<std::mutex> loc(m);
                value_type ret=queue_.front();
                queue_.pop_front();
                return ret;
            }

            // wrap of function size() empty() of vector thrade-safe
            int size(){
                std::lock_guard<std::mutex> loc(m);
                return queue_.size();
            }
            bool empty(){
                std::lock_guard<std::mutex> loc(m);
                return queue_.empty();
            }
            //per debug momentanei
            void print(){
                for (T i : queue_)
                    std::cout<<i<<"  ";
                std::cout<<std::endl;
            }



    };

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

// push_front di n elementi per worker queue 
void push_front_di_n_elem(fdapde::Worker_queue<value> & q,int n, value el){
    for (int j=0; j<n; j++){
        q.push_front(el);
    }
};
// push_front di n elementi per deque 
void push_front_di_n_elem_d(Worker_queue_deque<value> & q,int n, value el){
    for (int j=0; j<n; j++){
        q.push_front(el);
    }
};
// push_front di n elementi per old_worker_queue
void push_front_di_n_elem_o(fdapde::old_Worker_queue<value> & q,int n, value el){
    for (int j=0; j<n; j++){
        q.push_front(el);
    }
};

// push_back di n elementi per worker queue 
void push_back_di_n_elem(fdapde::Worker_queue<value> & q,int n, value el){
    for (int j=0; j<n; j++){
        q.push_back(el);
    }
};
// push_back di n elementi per deque 
void push_back_di_n_elem_d(Worker_queue_deque<value> & q,int n, value el){
    for (int j=0; j<n; j++){
        q.push_back(el);
    }
};
// push_back di n elementi per old_worker_queue 
void push_back_di_n_elem_o(fdapde::old_Worker_queue<value> & q,int n, value el){
    for (int j=0; j<n; j++){
        q.push_back(el);
    }
};

// pop_back di n elementi per worker queue 
void pop_back_di_n_elem(fdapde::Worker_queue<value> & q,int n){
    for (int j=0; j<n; j++){
        q.pop_back();
    }
};
// pop_back di n elementi per deque 
void pop_back_di_n_elem_d(Worker_queue_deque<value> & q,int n){
    for (int j=0; j<n; j++){
        q.pop_back();
    }
};
// pop_back di n elementi per old_worker_queue 
void pop_back_di_n_elem_o(fdapde::old_Worker_queue<value> & q,int n){
    for (int j=0; j<n; j++){
        q.pop_back();
    }
};

// pop_front di n elementi per worker queue 
void pop_front_di_n_elem(fdapde::Worker_queue<value> & q,int n){
    for (int j=0; j<n; j++){
        q.pop_front();
    }
};
// pop_front di n elementi per deque 
void pop_front_di_n_elem_d(Worker_queue_deque<value> & q,int n){
    for (int j=0; j<n; j++){
        q.pop_front();
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

/*    
//push_back()

    fdapde::Worker_queue<value> q1(size_coda);
    Worker_queue_deque<value> d1;
    auto start = std::chrono::high_resolution_clock::now();  
    //worker_queue push_back parallelo
    std::vector<std::thread> thread_pool;
    for (int j=0; j<n_thread; j++){
        thread_pool.emplace_back(push_back_di_n_elem,std::ref(q1),n_singolo, "ciao");
    }

    for (int k= 0; k<n_thread; k++){
        thread_pool[k].join();
    }
    //q1.print(); //per debug
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);  
    std::cout<<"push_back in worker_queue di n_elementi: "<<size_coda<<" con n_thread:"<<n_thread<<" impiegato:"<<duration.count()<< " microsecondi\n";

    auto start1 = std::chrono::high_resolution_clock::now();
    //deque push_back parallelo
    std::vector<std::thread> thread_pool_d;
    for (int j=0; j<n_thread; j++){
        thread_pool_d.emplace_back(push_back_di_n_elem_d,std::ref(d1),n_singolo, "ciao");
    }

    for (int k= 0; k<n_thread; k++){
        thread_pool_d[k].join();
    }
    //d1.print(); //per debug
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);  
    std::cout<<"push_frot in deque di n_elementi: "<<size_coda<<" con n_thread:"<<n_thread<<" impiegato:"<<duration1.count()<< " microsecondi\n";


//pop_back()
    fdapde::Worker_queue<value> q2(size_coda);
    Worker_queue_deque<value> d2;

    //popolo
    for (int i=0; i<size_coda; i++){
        q2.push_front("ciao");
        d2.push_front("ciao");
    }

    auto start2 = std::chrono::high_resolution_clock::now();
    //worker_queue pop_back parallelo
    std::vector<std::thread> thread_pool2;
    for (int j=0; j<n_thread; j++){
        thread_pool2.emplace_back(pop_back_di_n_elem,std::ref(q2),n_singolo);
    }

    for (int k= 0; k<n_thread; k++){
        thread_pool2[k].join();
    }    
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);  
    std::cout<<"pop_back() in worker_queue di n_elementi: "<<size_coda<<" con n_thread:"<<n_thread<<" impiegato:"<<duration2.count()<< " microsecondi\n";


    auto start3 = std::chrono::high_resolution_clock::now();
    //deque push_back parallelo
    std::vector<std::thread> thread_pool_d2;
    for (int j=0; j<n_thread; j++){
        thread_pool_d2.emplace_back(pop_back_di_n_elem_d,std::ref(d2),n_singolo);
    }

    for (int k= 0; k<n_thread; k++){
        thread_pool_d2[k].join();
    }   

    auto end3 = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start3);  
    std::cout<<"pop_back in deque di n_elementi: "<<size_coda<<" con n_thread:"<<n_thread<<" impiegato:"<<duration3.count()<< " microsecondi\n";

*/

//push_back da piu thread e push_front da singolo
    fdapde::Worker_queue<value> q3(size_coda);
    Worker_queue_deque<value> d3;
    fdapde::old_Worker_queue<value> w(size_coda);
/*
    auto start4 = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> thread_pool3;
    for (int j=0; j<n_thread-1; j++){
        thread_pool3.emplace_back(push_back_di_n_elem,std::ref(q3),n_singolo, "ciao");
    }
    thread_pool3.emplace_back(push_front_di_n_elem,std::ref(q3),n_singolo, "ciao");

    for (int k= 0; k<n_thread; k++){
        thread_pool3[k].join();
    }   
    auto end4 = std::chrono::high_resolution_clock::now();
    auto duration4 = std::chrono::duration_cast<std::chrono::microseconds>(end4 - start4);  
    //std::cout<<"push in worker_queue di n_elementi: "<<size_coda<<" con n_thread_back: "<<n_thread-1<<" e un thread_front impiegato:"<<duration4.count()<< " microsecondi\n"; 
    std::cout<<duration4.count()<<",";
*/
/*
    auto start5 = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> thread_pool4;
    for (int j=0; j<n_thread-1; j++){
        thread_pool4.emplace_back(push_back_di_n_elem_d,std::ref(d3),n_singolo, "ciao");
    }
    thread_pool4.emplace_back(push_front_di_n_elem_d,std::ref(d3),n_singolo, "ciao");

    for (int k= 0; k<n_thread; k++){
        thread_pool4[k].join();
    }   
    auto end5 = std::chrono::high_resolution_clock::now();
    auto duration5 = std::chrono::duration_cast<std::chrono::microseconds>(end5 - start5);  
    //std::cout<<"push in deque di n_elementi: "<<size_coda<<" con n_thread_back: "<<n_thread-1<<" e un thread_front impiegato:"<<duration5.count()<< " microsecondi\n"; 
    std::cout<<duration5.count()<<",";
    
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
*/
//pop_back da piu thread e pop_front da singolo
    fdapde::Worker_queue<value> q4(size_coda);
    Worker_queue_deque<value> d4;
    fdapde::old_Worker_queue<value> w2(size_coda);

    //popolo
    for (int i=0; i<size_coda; i++){
        q4.push_front("ciao");
        d4.push_front("ciao");
        w2.push_front("ciao");
    }
/*
    auto start6 = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> thread_pool5;
    for (int j=0; j<n_thread-1; j++){
        thread_pool5.emplace_back(pop_back_di_n_elem,std::ref(q4),n_singolo);
    }
    thread_pool5.emplace_back(pop_front_di_n_elem,std::ref(q4),n_singolo);

    for (int k= 0; k<n_thread; k++){
        thread_pool5[k].join();
    }   
    auto end6= std::chrono::high_resolution_clock::now();
    auto duration6 = std::chrono::duration_cast<std::chrono::microseconds>(end6 - start6);  
    //std::cout<<"pop in worker_queue di n_elementi: "<<size_coda<<" con n_thread_back: "<<n_thread-1<<" e un thread_front impiegato:"<<duration6.count()<< " microsecondi\n"; 
    std::cout<<duration6.count()<<",";

    auto start7 = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> thread_pool6;
    for (int j=0; j<n_thread-1; j++){
        thread_pool6.emplace_back(pop_back_di_n_elem_d,std::ref(d4),n_singolo);
    }
    thread_pool6.emplace_back(pop_front_di_n_elem_d,std::ref(d4),n_singolo);

    for (int k= 0; k<n_thread; k++){
        thread_pool6[k].join();
    }   
    auto end7 = std::chrono::high_resolution_clock::now();
    auto duration7 = std::chrono::duration_cast<std::chrono::microseconds>(end7 - start7);  
    //std::cout<<"pop in deque di n_elementi: "<<size_coda<<" con n_thread_back: "<<n_thread-1<<" e un thread_front impiegato:"<<duration7.count()<< " microsecondi\n"; 
    std::cout<<duration7.count()<<",";
*/
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



//risultati
/* push_back e pop_back con multithreading, :| :) 

push_back in worker_queue di n_elementi: 1600 con n_thread:8 impiegato:1542 microsecondi
push_frot in deque di n_elementi: 1600 con n_thread:8 impiegato:1602 microsecondi
pop_back() in worker_queue di n_elementi: 1600 impiegato:757 microsecondi
pop_back in deque di n_elementi: 1600 impiegato:1231 microsecondi

push_back in worker_queue di n_elementi: 1600 con n_thread:8 impiegato:5234 microsecondi
push_frot in deque di n_elementi: 1600 con n_thread:8 impiegato:4176 microsecondi
pop_back() in worker_queue di n_elementi: 1600 impiegato:974 microsecondi
pop_back in deque di n_elementi: 1600 impiegato:1902 microsecondi

push_back in worker_queue di n_elementi: 1600 con n_thread:8 impiegato:921 microsecondi
push_frot in deque di n_elementi: 1600 con n_thread:8 impiegato:2140 microsecondi
pop_back() in worker_queue di n_elementi: 1600 impiegato:844 microsecondi
pop_back in deque di n_elementi: 1600 impiegato:1527 microsecondi
*/


//OSS: inutile pop_front() e push_front() fatti da piu thread perche, per nostra implementazione threadpool, solo un thread fara push_front e pop_front()
// invece da testare pop e push di n elelemnti con:  multithread che fanno back e singolo che fa front
// risultati
/*
push in worker_queue di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:1772 microsecondi
push in deque di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:4848 microsecondi
pop in worker_queue di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:2973 microsecondi
pop in deque di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:2615 microsecondi

push in worker_queue di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:3829 microsecondi
push in deque di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:4046 microsecondi
pop in worker_queue di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:1376 microsecondi
pop in deque di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:3132 microsecondi

push in worker_queue di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:3875 microsecondi
push in deque di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:4692 microsecondi
pop in worker_queue di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:2508 microsecondi
pop in deque di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:2416 microsecondi

*/


//OSS: valutare cambiando 2 variabili singolarmente:
//     -n_thread:   diminuendo risultati peggiorano (sensato peerche worker piu pesante per essere piu efficente in multithread)
//     -size_coda: sia numero sia value (es con int deque molto meglio, con string risultati migliorano) (ideale test con value=function, come sara in threadpool)
