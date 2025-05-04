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

bool fun(){
    std::cout<<"fun da thread_id: "<<std::this_thread::get_id()<<std::endl;
    //std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return true;
};
bool fun2(){
    std::cout<<"fun22222222da thread_id: "<<std::this_thread::get_id()<<std::endl;
    return true;
};

bool count (int n){
    int i= 0;
    for(int j=0; j<n; j++){
        i++;
    }
    std::cout<<"thread:"<<std::this_thread::get_id()<<" ha contato fino a "<<i<<std::endl;
    return true;
};
int main(){
{/*void function, trasformate in bool fittizie coai che con future si aspetta esecuzione*/
    
    using job = std::function<bool()>;
    fdapde::Threadpool tp(16,4);
    job j1 = fun;
    job j2 = fun;
    job j3 = fun;
    job j4 = fun;
    job j5 = fun;
    
    auto fut1 = tp.send_task(j1);
    auto fut2 = tp.send_task(j2);
    auto fut3 = tp.send_task(j3);
    auto fut4 = tp.send_task(j4);
    auto fut5 = tp.send_task(j5);


    fut5.get(); 
    fut1.get();
    fut2.get();
    fut3.get();
    fut4.get();
    

}
    return 0;
}
