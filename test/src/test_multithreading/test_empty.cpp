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
 
 void vuoto(fdapde::Synchro_queue<int,fdapde::relax_nowait> & q){
   std::cout<<std::this_thread::get_id()<<"dice che è: "<<q.empty()<<std::endl;
 }

 int main(){
{ std::cout<<"--------------------------------RELAX_NOWAIT--------------------------------"<<std::endl;
    fdapde::Synchro_queue<int,fdapde::relax_nowait> q(10);
 
    std::vector<std::thread> pool;
    int k=0;
    for(int i=0; i<10; i++){
      if((k % 2) == 0)
         pool.emplace_back(&fdapde::Synchro_queue<int,fdapde::relax_nowait>::push_front,std::ref(q),k);
      else
         pool.emplace_back(&fdapde::Synchro_queue<int,fdapde::relax_nowait>::pop_front,std::ref(q));
      k++;
    }
    q.print();
    std::cout<<"vede: "<<q.empty()<<"   dovrebbe vedere 1 (coda vuota 1=true)"<<std::endl;
    q.print();
    
   

    for(size_t i =0; i<pool.size(); i++){
      pool[i].join();
    }
}

{/// Hold_nowait
  std::cout<<"--------------------------------HOLD_NOWAIT--------------------------------"<<std::endl;
  fdapde::Synchro_queue<int,fdapde::hold_nowait> q(10);
 
  std::vector<std::thread> pool;
  int k=0;
  for(int i=0; i<10; i++){
    if((k % 2) == 0)
       pool.emplace_back(&fdapde::Synchro_queue<int,fdapde::hold_nowait>::push_front,std::ref(q),k);
    else
       pool.emplace_back(&fdapde::Synchro_queue<int,fdapde::hold_nowait>::pop_front,std::ref(q));
    k++;
  }
  q.print();
  std::cout<<"vede: "<<q.empty()<<"   dovrebbe vedere 1 (coda vuota 1=true)"<<std::endl;
  q.print();
  
 

  for(size_t i =0; i<pool.size(); i++){
    pool[i].join();
  }
}
{/// Hold_wait
  std::cout<<"--------------------------------HOLD_WAIT--------------------------------"<<std::endl;
  fdapde::Synchro_queue<int,fdapde::hold_wait> q(10);
 
  std::vector<std::thread> pool;
  int k=0;
  for(int i=0; i<10; i++){
    if((k % 2) == 0)
       pool.emplace_back(&fdapde::Synchro_queue<int,fdapde::hold_wait>::push_front,std::ref(q),k);
    else
       pool.emplace_back(&fdapde::Synchro_queue<int,fdapde::hold_wait>::pop_front,std::ref(q));
    k++;
  }
  q.print();
  std::cout<<"vede: "<<q.empty()<<"   dovrebbe vedere 1 (coda vuota 1=true)"<<std::endl;
  q.print();

  for(size_t i =0; i<pool.size(); i++){
    pool[i].join();
  }
}


return 0;
    }
