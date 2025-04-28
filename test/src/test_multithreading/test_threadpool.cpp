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
using job = std::function<void()>;
void fun(){
    std::cout<<"fun da thread_id: "<<std::this_thread::get_id()<<std::endl;
}
int main(){
    fdapde::Threadpool tp(16,2);
    job j1 = fun;
    job j2 = fun;
    job j3 = fun;
    job j4 = fun;
    
    tp.send_task(j1);
    tp.send_task(j2);
    tp.send_task(j3);
    tp.send_task(j4);

    std::this_thread::sleep_for(std::chrono::microseconds(100000));
    return 0;
}