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

int size_coda=100000000;
void padronecodatutta(fdapde::Worker_queue<int> & coda){
    int ii = size_coda;
    while(ii!=0){
        coda.pop_front();
        ii--;
    }
}

void padronecodamulti(fdapde::Worker_queue<int> & coda){
    int ii = size_coda/2;
    while(ii!=0){
        coda.pop_front();
        ii--;
    }
}

void rubalavoro(fdapde::Worker_queue<int> & coda){
    int ii = size_coda/4;
    while(ii!=0){
        coda.pop_back();
        ii--;
    }
}

int main(){
    fdapde::Worker_queue<int> q1(size_coda);
    //popolo coda
    for(int j=0; j<size_coda-1; j++){
        q1.push_front(j);
    }
    //q1.print();

    auto start = std::chrono::high_resolution_clock::now();
    std::thread t1(padronecodamulti, std::ref(q1));
    std::thread t2(rubalavoro, std::ref(q1));
    std::thread t3(rubalavoro, std::ref(q1));


    t1.join();
    t2.join();
    t3.join();
    //q1.print();
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);  
    std::cout<<"svuotare coda di n_elementi: "<<size_coda<<" con multithrading impiegato:"<<duration.count()<< " microsecondi\n";

    fdapde::Worker_queue<int> q2(size_coda);
    //popolo coda
    for(int j=0; j<size_coda-1; j++){
        q2.push_front(j);
    }

    auto start2 = std::chrono::high_resolution_clock::now();

    std::thread t4(padronecodatutta, std::ref(q2));
    t4.join();
    
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);  
    std::cout<<"svuotare coda di n_elementi: "<<size_coda<<" con 1 thrad impiegato:"<<duration2.count()<< " microsecondi\n";

    return 0;
}
