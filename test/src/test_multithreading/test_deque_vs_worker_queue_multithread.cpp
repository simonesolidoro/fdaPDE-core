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

using value = std::string;

// push_back di n elementi per worker queue 
void push_front_di_n_elem(fdapde::Worker_queue<value> & q,int n, value el){
    for (int j=0; j<n; j++){
        q.push_front(el);
    }
};
// push_back di n elementi per deque 
void push_front_di_n_elem_d(Worker_queue_deque<value> & q,int n, value el){
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

int main(){
    int size_coda= 1600;
    fdapde::Worker_queue<value> q1(size_coda);
    Worker_queue_deque<value> d1;

    int n_thread = 8;
    int n_singolo= size_coda / n_thread;
/*    
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
    std::cout<<"push in worker_queue di n_elementi: "<<size_coda<<" con n_thread_back: "<<n_thread-1<<" e un thread_front impiegato:"<<duration4.count()<< " microsecondi\n"; 

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
    std::cout<<"push in deque di n_elementi: "<<size_coda<<" con n_thread_back: "<<n_thread-1<<" e un thread_front impiegato:"<<duration5.count()<< " microsecondi\n"; 



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

push in worker_queue di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:2028 microsecondi
push in deque di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:1344 microsecondi

push in worker_queue di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:1548 microsecondi
push in deque di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:1816 microsecondi

push in worker_queue di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:703 microsecondi
push in deque di n_elementi: 1600 con n_thread_back: 7 e un thread_front impiegato:1715 microsecondi
*/

//OSS: a volte deque da errore corrupted size vs. prev_size

//OSS: valutare cambiando 2 variabili singolarmente:
//     -n_thread:   diminuendo risultati peggiorano (sensato peerche worker piu pesante per essere piu efficente in multithread)
//     -size_coda: sia numero sia value (es con int deque molto meglio, con string risultati migliorano) (ideale test con value=function, come sara in threadpool)
