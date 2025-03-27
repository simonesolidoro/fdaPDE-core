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

using value = std::function<int()>;

int fun(){
    std::string s="ciaoo";
    std::vector<std::string> v = {s,s,s,s};
    return 0;
};


// push_front di n elementi per deque 
void push_front_di_n_elem_d(Worker_queue_deque<value> & q,int n, value el){
    for (int j=0; j<n; j++){
        q.push_front(el);
    }
};

// push_back di n elementi per deque 
void push_back_di_n_elem_d(Worker_queue_deque<value> & q,int n, value el){
    for (int j=0; j<n; j++){
        q.push_back(el);
    }
};
// pop_back di n elementi per deque 
void pop_back_di_n_elem_d(Worker_queue_deque<value> & q,int n){
    for (int j=0; j<n; j++){
        q.pop_back();
    }
};
// pop_front di n elementi per deque 
void pop_front_di_n_elem_d(Worker_queue_deque<value> & q,int n){
    for (int j=0; j<n; j++){
        q.pop_front();
    }
};

int main(){
    int size_coda= 16000;
    int n_thread = 4;
    int n_singolo= size_coda / n_thread;


//push_back da piu thread e push_front da singolo
    Worker_queue_deque<value> d3;
    
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
*/
//pop_back da piu thread e pop_front da singolo
    
    Worker_queue_deque<value> d4;
    

    //popolo
    for (int i=0; i<size_coda; i++){    
        d4.push_front(fun);
    }

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

    return 0;
}

