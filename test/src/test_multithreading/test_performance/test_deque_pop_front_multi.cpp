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
            std::deque<std::optional<value_type>> queue_;
            int size_; 
            std::mutex m;
        public:

            // void per pigrizia tanto solo per test
            void push_front(value_type t){
                std::lock_guard<std::mutex> loc(m);
                queue_.push_back(t); 
            }

            std::optional<value_type> pop_front(){
                std::lock_guard<std::mutex> loc(m);
                if(queue_.empty())
                    return std::nullopt;
                value_type ret=std::move(queue_.back().value());
                queue_.pop_back();
                return ret;   
            }
            
            //push_back() thread-safe 
            void push_back(value_type t){
                std::lock_guard<std::mutex> loc(m);
                queue_.push_front(t);  
            }

            //pop_back() thrade-safe
            std::optional<value_type> pop_back(){
                std::lock_guard<std::mutex> loc(m);
                if (queue_.empty())
                    return std::nullopt;
                value_type ret=std::move(queue_.front().value());
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


// pop_front di n elementi per deque 
void pop_front_di_n_elem_d(Worker_queue_deque<value> & q,int n){
    for (int j=0; j<n; j++){
        q.pop_front();
    }
};

int main(){
    int size_coda= 16000;
    int n_thread = 8;
    int n_singolo= size_coda / n_thread;

    value el = "ciao";

    Worker_queue_deque<value> q1;

//pop_front() multithreading

    Worker_queue_deque<value> q2;

    //popolo
    for (int i=0; i<size_coda; i++){
        q2.push_front(el);
    }

    auto start2 = std::chrono::high_resolution_clock::now();
    //worker_queue pop_back parallelo
    std::vector<std::thread> thread_pool2;
    for (int j=0; j<n_thread; j++){
        thread_pool2.emplace_back(pop_front_di_n_elem_d,std::ref(q2),n_singolo);
    }

    for (int k= 0; k<n_thread; k++){
        thread_pool2[k].join();
    }    
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);  
    //std::cout<<"pop_back() in deque di n_elementi: "<<size_coda<<" con n_thread:"<<n_thread<<" impiegato:"<<duration2.count()<< " microsecondi\n";
    std::cout<<duration2.count()<<",";
    return 0;
}
