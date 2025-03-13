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


void popola1(fdapde::Worker_queue<int> & coda){
    for(int i=1 ; i<9; i++)
        coda.push_back(i*10);
}
void popola2(fdapde::Worker_queue<int> & coda){
    for(int i=1 ; i<9; i++)
        coda.push_front(i);
}

int main(){
    fdapde::Worker_queue<int> q1(10);


    std::thread t1(popola1, std::ref(q1));
    std::thread t2(popola2, std::ref(q1));

    t1.join();
    t2.join();
    
    q1.print();
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