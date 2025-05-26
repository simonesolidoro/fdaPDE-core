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
{
    fdapde::Threadpool<fdapde::steal::random> tp(16,4);
    std::atomic<int> a=0;
    tp.parallel_for_sure(0,10000,[&](int i){a++;});
    std::cout<<"sure a:"<<a.load()<<std::endl;

    
    auto V = tp.parallel_for(0,10000,[&](int i){a++;});
    for(auto& x: V){
        if(x){
            x.value().get();
        }
    }
    std::cout<<"sure a:"<<a.load()<<std::endl;
} 
    return 0;
}
