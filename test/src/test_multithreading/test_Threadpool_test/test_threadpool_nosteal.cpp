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
     thread_local int a;
    a++;
    std::cout<<std::this_thread::get_id()<<"valore A:"<<a<<std::endl;
    std::this_thread::sleep_for(std::chrono::microseconds(10000));
}

int main(){
{
    using job = std::function<void()>;
    fdapde::Threadpool_nosteal tp(16,4);
    std::vector<std::optional<std::future<void>>> futs;
    std::vector<job> jobs;
    int n_jobs = 64;
    for(int i = 0; i< n_jobs; i++){
        jobs.push_back(printnum);
    }
    for (int i = 0; i<n_jobs; i++){
        futs.push_back(std::move(tp.send_task_round(jobs[i])));
    }
    for (int i = 0; i<n_jobs; i++){
        if(futs[i]){
            futs[i].value().get();}
        else 
            std::cout<<"noninviato"<<std::endl;
    }

}
    return 0;
}
