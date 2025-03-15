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

int main(){
    int size_coda= 100;
    fdapde::Worker_queue<int> q1(size_coda);
    Worker_queue_deque<int> d1;

    //push_front()
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

    //q1.print();
    //d1.print();

    //push_back()
    auto start5 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        q1.push_back(j);
    }

    auto end5 = std::chrono::high_resolution_clock::now();
    auto duration5 = std::chrono::duration_cast<std::chrono::microseconds>(end5 - start5);  
    std::cout<<"push_back() worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration5.count()<< " microsecondi\n";

    auto start6 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        d1.push_back(j);
    }

    auto end6 = std::chrono::high_resolution_clock::now();
    auto duration6 = std::chrono::duration_cast<std::chrono::microseconds>(end6 - start6);  
    std::cout<<"push_back() deque di n_elementi: "<<size_coda<<" impiegato:"<<duration6.count()<< " microsecondi\n";

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

    //q1.print();
    //d1.print();

    return 0;
}




/* RISULTATI */
/*
push_frot worker_queue di n_elementi: 100 impiegato:1 microsecondi
push_frot deque di n_elementi: 100 impiegato:2 microsecondi
pop_frot worker_queue di n_elementi: 100 impiegato:1 microsecondi
pop_frot deque di n_elementi: 100 impiegato:2 microsecondi
push_back() worker_queue di n_elementi: 100 impiegato:5 microsecondi
push_back() deque di n_elementi: 100 impiegato:4 microsecondi
pop_back() worker_queue di n_elementi: 100 impiegato:3 microsecondi
pop_back() deque di n_elementi: 100 impiegato:31 microsecondi
*/


