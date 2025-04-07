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


    fdapde::Worker_queue<int, fdapde::Memory_order::hold> q(10);

    //push_front()
    for (int i =1; i<11; i++){
        q.push_front(i);
    }
    std::cout<<"push_front():  "<<std::endl;
    q.print();

    //pop_front
    for(int j=1; j<11; j++)
        q.pop_front();
    std::cout<<"pop_front():  "<<std::endl;
    q.print();

    //push_back()
    for (int i =1; i<11; i++){
        q.push_back(i);
    }
    std::cout<<"push_back():  "<<std::endl;
    q.print();

    //pop_back()
    for(int j=1; j<11; j++)
        q.pop_back();
    std::cout<<"pop_back():  "<<std::endl;
    q.print();
     return 0;
    }
