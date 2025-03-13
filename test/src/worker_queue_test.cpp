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




int main(){
    fdapde::Worker_queue<int> q(10);
    fdapde::Worker_queue<int> p;
    std::cout<<p.size()<<" "<<q.size()<<std::endl;
    std::cout<<p.get_tail()<<" "<<q.get_head()<<std::endl;
    for (int i =1; i<21; i++){
        q.push_back(i);
    }
    
    q.print();
    q.flush();
    q.print();
    
    /* pop_front
    for(int j=0; j<20; j++)
        q.pop_front();
    
    q.print();
    */

    /* pop_back()
    for(int j=0; j<20; j++)
        q.pop_back();
    q.print();*/

    
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