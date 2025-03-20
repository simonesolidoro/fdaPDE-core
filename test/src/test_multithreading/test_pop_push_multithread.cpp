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

using namespace std::chrono_literals;

void pushbackconc(std::vector<int> V, fdapde::Worker_queue<int> & q){
    for (auto x: V){
        q.push_back(x);
    }
    std::this_thread::sleep_for(10ms);
    //if (q.empty()){std::cout<<"vuoto";}
}
void pushfrontconc(std::vector<int> V, fdapde::Worker_queue<int> & q){
    for (auto x: V){
        q.push_front(x);
    }
    std::this_thread::sleep_for(10ms);
    //if (q.empty()){std::cout<<"vuoto";}
}

int main(){
    int size_coda=10;
    fdapde::Worker_queue<int> q1(size_coda);
    
/*
    // push_back concorrente semplice
    int a=3;
    int b=2;
    std::thread t1([&](){q1.push_back(a);});
    std::thread t2([&](){q1.push_back(b);});
    t1.join();
    t2.join();
    q1.print();

    // pop_back concorrente semplice
    std::thread t3([&q1](){q1.pop_back();});
    std::thread t4([&q1](){q1.pop_back();});
    t3.join();
    t4.join();
    q1.print();
*/
    std::vector<int> v={1,2,3,4,5};
    pushfrontconc(v,q1);
    std::thread t1(pushbackconc,v,std::ref(q1));
    //std::thread t2(pushbackconc,v,std::ref(q1));
    t1.join();
    //t2.join();
    q1.print();
    return 0;
}
