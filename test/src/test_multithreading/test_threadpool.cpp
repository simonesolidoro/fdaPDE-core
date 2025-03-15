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


/*TEST singola coda thread-safe  vs  piu worker_queue */

//OSS: in realta servirebbe threadpool che implementa logica per cui un thread finito la sua worker_queue va a pescare da altre, quindi non so quanto utile questo test

int size_coda=1000;
using namespace std::chrono_literals;

void padronecoda(fdapde::Worker_queue<int> & coda){
    while(! coda.empty()){
        coda.pop_front();
        //std::this_thread::sleep_for(1ms); //simula lavoro
    }

}


void rubalavoro(fdapde::Worker_queue<int> & coda1,fdapde::Worker_queue<int> & coda2){
    while(!coda1.empty()){
        coda1.pop_back();
        //std::this_thread::sleep_for(1ms);
    }
    while(!coda2.empty()){
        coda2.pop_back();
        //std::this_thread::sleep_for(1ms);
    }
}

template <typename T>
class Unica_coda_threadsafe{
    private:
        std::deque<T> queue_; 
        std::mutex m_;
    public:

    // tutti membri pop push con lock (thtread-safe)
        void push_front(T t){
            std::lock_guard<std::mutex> loc(m_);
            queue_.push_back(t); 
        }

        T pop_front(){
            std::lock_guard<std::mutex> loc(m_);
            T ret=queue_.back();
            queue_.pop_back();
            return ret;   
        }
                 
        void push_back(T t){
            std::lock_guard<std::mutex> loc(m_);
            queue_.push_front(t);  
        }

        T pop_back(){
            std::lock_guard<std::mutex> loc(m_);
            T ret=queue_.front();
            queue_.pop_front();
            return ret;
        }

        // wrap of function size() empty() of vector thrade-safe
        int size(){
            std::lock_guard<std::mutex> loc(m_);
            return queue_.size();
        }
        bool empty(){
            std::lock_guard<std::mutex> loc(m_);
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

    fdapde::Worker_queue<int> q1(size_coda);
    fdapde::Worker_queue<int> q2(size_coda);

    //popolo code
    for(int j=1; j<=size_coda; j++){
        q1.push_front(j);
    }
    for(int j=1; j<=size_coda; j++){
        q2.push_front(j);
    }
    //q1.print();
    //q2.print();

    // svuoto con tre thread due che hanno loro coda e uno che ruba dal fondo 
    auto start1 = std::chrono::high_resolution_clock::now();
    std::thread t1(padronecoda, std::ref(q1));
    std::thread t2(padronecoda, std::ref(q2));
    std::thread t3(rubalavoro, std::ref(q1),std::ref(q2));


    t1.join();
    t2.join();
    t3.join();
    //q1.print();
    //q2.print();
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);  
    std::cout<<"svuotare 2 singole coda di n_elementi: "<<size_coda<<" con multithrading impiegato:"<<duration1.count()<< " microsecondi\n";


    /* unica coda per tutti i thread */
    //popolo
    Unica_coda_threadsafe<int> D;
    for (int i =1; i<= size_coda*2; i++){
        D.push_back(i);
    }

    // svuoto coda unica con tre thread
    auto start2 = std::chrono::high_resolution_clock::now();
    std::thread tt([&D](){while (!D.empty()){
                                D.pop_back();
                                //std::this_thread::sleep_for(1ms);
                                 }
                        }
                    );
    std::thread tt1([&D](){while (!D.empty()){
                                D.pop_back();
                                //std::this_thread::sleep_for(1ms);
                                 }
                        }
                    );   
    std::thread tt2([&D](){while (!D.empty()){
                                D.pop_back();
                                //std::this_thread::sleep_for(1ms);
                                 }
                        }
                    );

    tt.join();
    tt1.join();
    tt2.join();

    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);  
    std::cout<<"svuotare singola coda di n_elementi: "<<size_coda*2<<" con multithrading impiegato:"<<duration2.count()<< " microsecondi\n";


    return 0;
}






















/*test multithreading giocattolo
void conta(){

}

class toy_worker {
    private:
        fdapde::Worker_queue<int> coda;
        std::thread t;
    public:
        toy_worker(int n):coda(n),t(conta,){

        };
};

class toy_threadpool {
    private:
        std::vector<toy_worker> W;
    pubblic:
        toy_threadpool(int n, int n_threads){
            for(int i=0; i<n_threads; i++){
                W.emplace_back(n)
            }
            
        }

};*/


/*
class toy_worker {
    private:
        fdapde::Worker_queue<int> coda;
    public:
        toy_worker(int n):coda(n){
            std::thread t(&toy_worker::run,this);
        };
        void run(){
            while(true){
                if(! coda.empty()){
                    int count= coda.pop_front();
                    std::cout<<"da thread: "<<std::this_thread::get_id()<<" int: "<<count<<std::endl;
                }
            }
        }
};

/*class toy_threadpool {
    private:
        std::vector<toy_worker> W;
    public:
        toy_threadpool(int n_coda, int n_thread){
            for (int i = 0; i< n_thread; i++){
                W.emplace_back(n_coda);
            }
        }

};*/
