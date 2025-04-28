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
using job = std::function<bool()>;
bool fun(){
    std::cout<<"fun da thread_id: "<<std::this_thread::get_id()<<std::endl;
    return true;
};
bool fun2(){
    std::cout<<"fun22222222da thread_id: "<<std::this_thread::get_id()<<std::endl;
    return true;
};
int main(){
    fdapde::Threadpool<bool> tp(16,4);
    job j1 = fun;
    job j2 = fun;
    job j3 = fun;
    job j4 = fun;
    
    tp.send_task_round(j1);
    tp.send_task_round(j2);
    tp.send_task_round(j3);
    tp.send_task_round(j4);
    tp.send_task_round([](){std::cout<<"lambda da thread_id: "<<std::this_thread::get_id()<<std::endl;
    return true;});

    auto ptr_task = std::make_shared<std::packaged_task<bool()>> (fun2); 
    std::future<bool> fut = ptr_task->get_future();
    tp.send_task_round([ptr_task]() mutable ->bool { (*ptr_task)();
    return true;});

    fut.get(); //OSS: unico job che siamo sicuri verra eseguito è task a cui è associato il future gli altri potrebbero non essere eseguiti perche il distruttore potrebbe essere chiamato prima


    //std::this_thread::sleep_for(std::chrono::seconds(3));
    return 0;
}