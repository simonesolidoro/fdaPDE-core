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
                queue_.push_back(t); 
            }

            T pop_front(){
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
// push_back di n elementi per worker queue 
void push_back_di_n_elem(fdapde::Worker_queue<std::string> & q,int n, std::string el){
    for (int j=0; j<n; j++){
        q.push_back(el);
    }
};
// push_back di n elementi per deque 
void push_back_di_n_elem_d(Worker_queue_deque<std::string> & q,int n, std::string el){
    for (int j=0; j<n; j++){
        q.push_back(el);
    }
};

int main(){
    int size_coda= 1600;
    fdapde::Worker_queue<std::string> q1(size_coda);
    Worker_queue_deque<std::string> d1;

    int n_thread = 8;
    int n_singolo= size_coda / n_thread;
    
//push_back()
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
/*

//pop_front()
    auto start2 = std::chrono::high_resolution_clock::now();
    

    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);  
    std::cout<<"pop_frot worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration2.count()<< " microsecondi\n";

    auto start3 = std::chrono::high_resolution_clock::now();
   

    auto end3 = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start3);  
    std::cout<<"pop_frot deque di n_elementi: "<<size_coda<<" impiegato:"<<duration3.count()<< " microsecondi\n";

//push_back()
    auto start5 = std::chrono::high_resolution_clock::now();
   
    auto end5 = std::chrono::high_resolution_clock::now();
    auto duration5 = std::chrono::duration_cast<std::chrono::microseconds>(end5 - start5);  
    std::cout<<"push_back() worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration5.count()<< " microsecondi\n";

    auto start6 = std::chrono::high_resolution_clock::now();
   

    auto end6 = std::chrono::high_resolution_clock::now();
    auto duration6 = std::chrono::duration_cast<std::chrono::microseconds>(end6 - start6);  
    std::cout<<"push_back() deque di n_elementi: "<<size_coda<<" impiegato:"<<duration6.count()<< " microsecondi\n";

//pop_back()
    auto start7 = std::chrono::high_resolution_clock::now();
   

    auto end7 = std::chrono::high_resolution_clock::now();
    auto duration7 = std::chrono::duration_cast<std::chrono::microseconds>(end7 - start7);  
    std::cout<<"pop_back() worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration7.count()<< " microsecondi\n";

    auto start8 = std::chrono::high_resolution_clock::now();


    auto end8 = std::chrono::high_resolution_clock::now();
    auto duration8 = std::chrono::duration_cast<std::chrono::microseconds>(end8 - start8);  
    std::cout<<"pop_back() deque di n_elementi: "<<size_coda<<" impiegato:"<<duration8.count()<< " microsecondi\n";

*/
    return 0;
}

// risultati
/*
con int deque piu veloce in push, :(


con string worker se la gioca in push, :|
*/
