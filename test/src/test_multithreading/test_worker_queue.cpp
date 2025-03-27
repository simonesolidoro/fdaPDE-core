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


using value = std::string;

// push_front di n elementi per worker queue 
void push_front_di_n_elem(fdapde::Worker_queue<value> & q,int n, value el){
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

// pop_back di n elementi per worker queue 
void pop_back_di_n_elem(fdapde::Worker_queue<value> & q,int n){
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

int main(){
    int size_coda= 1600;
    int n_thread = 2;
    int n_singolo= size_coda / n_thread;

/*
//push_back da piu thread e push_front da singolo
    fdapde::Worker_queue<value> q3(size_coda);
 
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
//pop_back da piu thread e pop_front da singolo
    fdapde::Worker_queue<value> q4(size_coda);
    
    //popolo
    for (int i=0; i<size_coda; i++){
        q4.push_front("ciao");
    }

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

    return 0;
}
