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
#include<iostream>  //per debug momentaneo

// worker_queue con deque
template <typename T>
class Worker_queue_deque{
    using value_type= T;
        private:
            std::deque<value_type> queue_;
            int size_; 
            std::mutex m;
        public:
            // default constructor credo poi da associare a metodo resize()
            Worker_queue_deque();
            // construct whit size of queue_=n;
            Worker_queue_deque(int n){
                queue_.resize(n);
                size_ = n;
            }

            // resize() di queue_ super easy for the moment
            void resize(int n){
                queue_ = std::deque<value_type> (n);
            }

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
                std::lock_guard loc(m);
                queue_.push_front(t);  
            }

            //pop_back() thrade-safe
            T pop_back(){
                std::lock_guard loc(m);
                value_type ret=queue_.front();
                queue_.pop_front();
                return ret;
            }

            // wrap of function size() empty() of vector thrade-safe
            int size(){
                std::lock_guard loc(m);
                return queue_.size();
            }
            bool empty(){
                std::lock_guard loc(m);
                return queue_.empty();
            }
            //per debug momentanei
            void print(){
                for (T i : queue_)
                    std::cout<<i<<"  ";
                std::cout<<std::endl;
            }



    };

int main(){
    int size_coda= 100;
    fdapde::Worker_queue<int> q1(size_coda);
    Worker_queue_deque<int> d1(size_coda);

    
    auto start = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        q1.push_front(j);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);  
    std::cout<<"push_frot worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration.count()<< " microsecondi\n";

    auto start1 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        d1.push_front(j);
    }

    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);  
    std::cout<<"push_frot deque di n_elementi: "<<size_coda<<" impiegato:"<<duration1.count()<< " microsecondi\n";
    return 0;
}




/* RISULTATI */
/*push_frot worker_queue di n_elementi: 100 impiegato:1 microsecondi
  push_frot deque di n_elementi: 100 impiegato:6 microsecondi
*/


