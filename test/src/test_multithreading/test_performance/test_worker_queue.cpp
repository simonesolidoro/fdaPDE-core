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

int fun(){
    std::string s="ciaoo";
    std::vector<std::string> v = {s,s,s,s};
    return 0;
};

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
    int size_coda= 16000;
    int n_thread = 8;
    int n_singolo= size_coda / n_thread;

    fdapde::Worker_queue<value> q1(size_coda);
    value el = "ciao";

//push_back() sinolo thread
/*    auto start5 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        q1.push_back(el);
    }

    auto end5 = std::chrono::high_resolution_clock::now();
    auto duration5 = std::chrono::duration_cast<std::chrono::microseconds>(end5 - start5);  
    //std::cout<<"push_back() worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration5.count()<< " microsecondi\n";
    std::cout<<duration5.count()<<",";
*/
//pop_back() singolo thread

        //popolo
        for (int i=0; i<size_coda; i++){
            q1.push_front(el);
        }
    auto start7 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        q1.pop_back();
    }

    auto end7 = std::chrono::high_resolution_clock::now();
    auto duration7 = std::chrono::duration_cast<std::chrono::microseconds>(end7 - start7);  
    //std::cout<<"pop_back() worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration7.count()<< " microsecondi\n";
    std::cout<<duration7.count()<<",";

//push_front() singolo thread
/*
    auto start = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        q1.push_front(el);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);  
    //std::cout<<"push_frot worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration.count()<< " microsecondi\n";
    std::cout<<duration.count()<<",";
*/
//pop_front() singolo thread
/*   //popolo
    for (int i=0; i<size_coda; i++){
        q1.push_front(el);
    }

    auto start2 = std::chrono::high_resolution_clock::now();
    for(int j=0; j<size_coda-1; j++){
        q1.pop_front();
    }

    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);  
    //std::cout<<"pop_frot worker_queue di n_elementi: "<<size_coda<<" impiegato:"<<duration2.count()<< " microsecondi\n";
    std::cout<<duration2.count()<<",";
*/
//push_back() multithreading
/*
    fdapde::Worker_queue<value> q(size_coda);

    auto start = std::chrono::high_resolution_clock::now();  
    //worker_queue push_back parallelo
    std::vector<std::thread> thread_pool;
    for (int j=0; j<n_thread; j++){
        thread_pool.emplace_back(push_back_di_n_elem,std::ref(q),n_singolo, el);
    }

    for (int k= 0; k<n_thread; k++){
        thread_pool[k].join();
    }
    //q1.print(); //per debug
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);  
    //std::cout<<"push_back in worker_queue di n_elementi: "<<size_coda<<" con n_thread:"<<n_thread<<" impiegato:"<<duration.count()<< " microsecondi\n";
    std::cout<<duration.count()<<",";
    
*/
//pop_back() multithreading
/*
    fdapde::Worker_queue<value> q2(size_coda);

    //popolo
    for (int i=0; i<size_coda; i++){
        q2.push_front(el);
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
    //std::cout<<"pop_back() in worker_queue di n_elementi: "<<size_coda<<" con n_thread:"<<n_thread<<" impiegato:"<<duration2.count()<< " microsecondi\n";
    std::cout<<duration2.count()<<",";
*/


//push_back da piu thread e push_front da singolo
/*    fdapde::Worker_queue<value> q3(size_coda);
 
    auto start4 = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> thread_pool3;
    for (int j=0; j<n_thread-1; j++){
        thread_pool3.emplace_back(push_back_di_n_elem,std::ref(q3),n_singolo, el);
    }
    thread_pool3.emplace_back(push_front_di_n_elem,std::ref(q3),n_singolo, el);

    for (int k= 0; k<n_thread; k++){
        thread_pool3[k].join();
    }   
    auto end4 = std::chrono::high_resolution_clock::now();
    auto duration4 = std::chrono::duration_cast<std::chrono::microseconds>(end4 - start4);  
    //std::cout<<"push in worker_queue di n_elementi: "<<size_coda<<" con n_thread_back: "<<n_thread-1<<" e un thread_front impiegato:"<<duration4.count()<< " microsecondi\n"; 
    std::cout<<duration4.count()<<",";

*/

//pop_back da piu thread e pop_front da singolo
/*    fdapde::Worker_queue<value> q4(size_coda);
    
    //popolo
    for (int i=0; i<size_coda; i++){
        q4.push_front(el);
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
*/
    return 0;
}
