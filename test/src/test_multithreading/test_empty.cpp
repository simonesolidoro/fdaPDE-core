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
 
 void vuoto(fdapde::Worker_queue<int> & q){
   std::cout<<std::this_thread::get_id()<<"dice che è: "<<q.empty()<<std::endl;
 }

 int main(){

    fdapde::Worker_queue<int> q(10);

/*    //funzionamento empty() 
    std::cout<<q.empty()<<std::endl;

    q.push_back(2);

    std::cout<<q.empty()<<std::endl;

    q.pop_back();
    std::cout<<q.empty()<<std::endl;

    q.push_front(3);
    std::cout<<q.empty()<<std::endl;

    q.pop_front();
    std::cout<<q.empty()<<std::endl;
*/
    //funzionamento di occupied_----------> !!!!! qualcosa non funziona, run piu volte a volte q.empty() ridà falso. 
    std::vector<std::thread> pool;
    int k=0;
    for(int i=0; i<20; i++){
      if((k % 2) == 0)
         pool.emplace_back(&fdapde::Worker_queue<int>::push_front,std::ref(q),10);
      else
         pool.emplace_back(&fdapde::Worker_queue<int>::pop_front,std::ref(q));
      k++;
    }
    
    std::cout<<"vede: "<<q.empty()<<std::endl;
    q.print();

    for(int i =0; i<pool.size(); i++){
      pool[i].join();
    }
     return 0;
    }