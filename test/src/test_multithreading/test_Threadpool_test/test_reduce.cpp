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
    fdapde::Threadpool<fdapde::steal::random> tp(16,4);

    int out = tp.parallel_reduce<fdapde::op::sum, std::function<int(int)>>(0,10000,[&](int i){return i;});

    std::cout << out << std::endl;

    out = tp.parallel_reduce<fdapde::op::sum, std::function<int()>>(10000,[&](){return 1;});

    std::cout << out << std::endl;
 
    return 0;
}
