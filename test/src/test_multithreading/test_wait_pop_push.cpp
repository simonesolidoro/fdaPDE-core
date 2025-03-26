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

   
int main(){
    fdapde::Worker_queue<int> q(5);
    //coda vuota
    int a;
    if( q.pop_back() == std::nullopt){
        a= 0;
    } 
    std::cout<<a<<" ridato false (0) perche coda vuota"<<std::endl;
    std::thread t(&fdapde::Worker_queue<int>::pop_back_or_wait,std::ref(q)); //prova a fare pop ma coda vuota quindi aspetta finche non viene fatto push
    q.push_back(3);
    q.print(); 
    q.print();
    t.join();

    

    return 0;
}

