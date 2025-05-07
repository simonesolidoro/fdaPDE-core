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
void printnum(){
    //std::cout<<std::this_thread::get_id()<<std::endl;
    std::this_thread::sleep_for(std::chrono::microseconds(1000000));
}
bool fun(){
    std::cout<<"fun da thread_id: "<<std::this_thread::get_id()<<std::endl;
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
    fdapde::Threadpool tp(16,2);
    job j1 = fun;
    job j2 = fun;
    job j3 = fun;
    job j4 = fun;
    
    tp.send_task(j1);
    tp.send_task(j2);
    tp.send_task(j3);
    tp.send_task(j4);
    tp.send_task([](){std::cout<<"lambda da thread_id: "<<std::this_thread::get_id()<<std::endl;
    return true;});

    auto ptr_task = std::make_shared<std::packaged_task<bool()>> (fun2); 
    std::future<bool> fut = ptr_task->get_future();
    tp.send_task([ptr_task]() mutable ->bool { (*ptr_task)();
    return true;});

    fut.get(); //OSS: unico job che siamo sicuri verra eseguito è task a cui è associato il future gli altri potrebbero non essere eseguiti perche il distruttore potrebbe essere chiamato prima


    //std::this_thread::sleep_for(std::chrono::seconds(3));
   
}

{/*se funzione che richiede parametri wrap in una lambda che cattura parametri cosi che lambda sia return_type() senza parametri*/
    //OSS: cosi facendo inutile template argoment Args... in worker e in threadpool tanto tutti i job sarebbero senza parametri

    fdapde::Threadpool tp(5,4);
    int n = 10000;

    auto ptr_task1 = std::make_shared<std::packaged_task<bool(int)>> (count);
    std::future<bool> fut1 = ptr_task1->get_future();
    tp.send_task_round([ptr_task1,n]() mutable ->bool { (*ptr_task1)(n);
        return true;});

    auto ptr_task2 = std::make_shared<std::packaged_task<bool(int)>> (count);
    std::future<bool> fut2 = ptr_task2->get_future();
    tp.send_task_round([ptr_task2,n]() mutable ->bool { (*ptr_task2)(n);
        return true;});

    auto ptr_task3 = std::make_shared<std::packaged_task<bool(int)>> (count);
    std::future<bool> fut3 = ptr_task3->get_future();
    tp.send_task_round([ptr_task3,n]() mutable ->bool { (*ptr_task3)(n);
        return true;});

    auto ptr_task4 = std::make_shared<std::packaged_task<bool(int)>> (count);
    std::future<bool> fut4 = ptr_task4->get_future();
    tp.send_task_round([ptr_task4,n]() mutable ->bool { (*ptr_task4)(n);
        return true;});

    fut1.get();
    fut2.get();
    fut3.get();
    fut4.get();
    

}
{
    using job = std::function<void()>;
    fdapde::Threadpool tp(16,4);
    std::vector<job> jobs;
    int n_jobs = 64;
    for(int i = 0; i< n_jobs; i++){
        jobs.push_back(printnum);
    }
    for (int i = 0; i<n_jobs; i++){
        tp.send_task(jobs[i]);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000)); //per aspettare esecuzione di tutti 
}
    return 0;
}