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
/*
//pop_ _or_wait()
    fdapde::Worker_queue<int> q(5);
    //coda vuota
    std::thread t(&fdapde::Worker_queue<int>::pop_back_or_wait,std::ref(q)); //prova a fare pop ma coda vuota quindi aspetta finche non viene fatto push
    q.push_back(3);
    q.print(); 
    q.print();
    t.join();

    std::thread t1(&fdapde::Worker_queue<int>::pop_front_or_wait,std::ref(q)); //prova a fare pop ma coda vuota quindi aspetta finche non viene fatto push
    q.push_back(3);
    q.print(); 
    q.print();
    t1.join();
*/
//push_ _or_wait()
{
    fdapde::Worker_queue_hold<int> q1(5);
    //popolo
    for(int j=0; j<5; j++){
        q1.push_front(j);
    }
    std::thread d(&fdapde::Worker_queue_hold<int>::push_back_or_wait,std::ref(q1),9,5); //prova a fare push ma coda piena quindi aspetta finche non viene fatto pop
    q1.pop_back();
    q1.print(); 
    q1.print();
    d.join();

    std::thread d2(&fdapde::Worker_queue_hold<int>::push_front_or_wait,std::ref(q1),9,5); //prova a fare pop ma coda vuota quindi aspetta finche non viene fatto push
    q1.pop_front();
    q1.print(); 
    d2.join();
    q1.print();
}
    
{
    fdapde::Worker_queue_relax<int> q1(5);
    //popolo
    for(int j=0; j<5; j++){
        q1.push_front(j);
    }
    std::thread d(&fdapde::Worker_queue_relax<int>::push_back_or_wait,std::ref(q1),9,5); //prova a fare push ma coda piena quindi aspetta finche non viene fatto pop
    q1.pop_back();
    q1.print(); 
    q1.print();
    d.join();

    std::thread d2(&fdapde::Worker_queue_relax<int>::push_front_or_wait,std::ref(q1),9,5); //prova a fare pop ma coda vuota quindi aspetta finche non viene fatto push
    q1.pop_front();
    q1.print(); 
    q1.print();
    d2.join();
}



    

    return 0;
}

