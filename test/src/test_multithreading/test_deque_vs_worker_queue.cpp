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

/*test per verifica vector usato come array piu efficente di deque */

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


int main(){
    int size_coda= 10000;
    fdapde::Worker_queue_hold<value> q1(size_coda);
    Worker_queue_deque<value> d1;
    fdapde::old_Worker_queue<value> w1(size_coda);
    value el= "ciao";

    //push_front()
    auto start = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        q1.push_front(el);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);  
    std::cout<<"push_frot worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration.count()<< " microsecondi\n";

    auto start1 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        d1.push_front(el);
    }

    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);  
    std::cout<<"push_frot deque di n_elementi: "<<size_coda<<" impiegato:"<<duration1.count()<< " microsecondi\n";

    auto start9 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        w1.push_front(el);
    }

    auto end9 = std::chrono::high_resolution_clock::now();
    auto duration9 = std::chrono::duration_cast<std::chrono::microseconds>(end9 - start9);  
    std::cout<<"push_frot old_worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration9.count()<< " microsecondi\n";

    //q1.print();
    //d1.print();

    //pop_front()
    auto start2 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        q1.pop_front();
    }

    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);  
    std::cout<<"pop_frot worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration2.count()<< " microsecondi\n";

    auto start3 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        d1.pop_front();
    }

    auto end3 = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start3);  
    std::cout<<"pop_frot deque di n_elementi: "<<size_coda<<" impiegato:"<<duration3.count()<< " microsecondi\n";

    auto start10 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        w1.pop_front();
    }

    auto end10 = std::chrono::high_resolution_clock::now();
    auto duration10 = std::chrono::duration_cast<std::chrono::microseconds>(end10 - start10);  
    std::cout<<"pop_frot old_worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration10.count()<< " microsecondi\n";

    //q1.print();
    //d1.print();

    //push_back()
    auto start5 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        q1.push_back(el);
    }

    auto end5 = std::chrono::high_resolution_clock::now();
    auto duration5 = std::chrono::duration_cast<std::chrono::microseconds>(end5 - start5);  
    std::cout<<"push_back() worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration5.count()<< " microsecondi\n";

    auto start6 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        d1.push_back(el);
    }

    auto end6 = std::chrono::high_resolution_clock::now();
    auto duration6 = std::chrono::duration_cast<std::chrono::microseconds>(end6 - start6);  
    std::cout<<"push_back() deque di n_elementi: "<<size_coda<<" impiegato:"<<duration6.count()<< " microsecondi\n";

    auto start11 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        w1.push_back(el);
    }

    auto end11 = std::chrono::high_resolution_clock::now();
    auto duration11 = std::chrono::duration_cast<std::chrono::microseconds>(end11 - start11);  
    std::cout<<"push_back old_worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration11.count()<< " microsecondi\n";

    //q1.print();
    //d1.print();

    //pop_back()
    auto start7 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        q1.pop_back();
    }

    auto end7 = std::chrono::high_resolution_clock::now();
    auto duration7 = std::chrono::duration_cast<std::chrono::microseconds>(end7 - start7);  
    std::cout<<"pop_back() worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration7.count()<< " microsecondi\n";

    auto start8 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        d1.pop_back();
    }

    auto end8 = std::chrono::high_resolution_clock::now();
    auto duration8 = std::chrono::duration_cast<std::chrono::microseconds>(end8 - start8);  
    std::cout<<"pop_back() deque di n_elementi: "<<size_coda<<" impiegato:"<<duration8.count()<< " microsecondi\n";

    auto start12 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        w1.pop_back();
    }

    auto end12 = std::chrono::high_resolution_clock::now();
    auto duration12 = std::chrono::duration_cast<std::chrono::microseconds>(end12 - start12);  
    std::cout<<"pop_back() old_worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration12.count()<< " microsecondi\n";
    //q1.print();
    //d1.print();

    return 0;
}




/* RISULTATI */
/*
push_frot worker_queue di n_elementi: 100 impiegato:22 microsecondi
push_frot deque di n_elementi: 100 impiegato:4 microsecondi
pop_frot worker_queue di n_elementi: 100 impiegato:21 microsecondi
pop_frot deque di n_elementi: 100 impiegato:7 microsecondi
push_back() worker_queue di n_elementi: 100 impiegato:16 microsecondi
push_back() deque di n_elementi: 100 impiegato:10 microsecondi
pop_back() worker_queue di n_elementi: 100 impiegato:20 microsecondi
pop_back() deque di n_elementi: 100 impiegato:17 microsecondi
*/


