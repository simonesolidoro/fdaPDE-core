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

int main(int argc,char** argv){
    int granularity = std::stoi(argv[1]);
    fdapde::threadpool tp(1024,8);

{
std::cout<<"================ reduce sum  ================"<<std::endl; 
    std::vector<double> v (16000,2);
    int sum = 0;
    sum = tp.reduce(v.begin(),v.end(),[](double a, double b){
        return a+b;
    }, granularity); 
    std::cout<< sum<<std::endl;
}
 



    return 0; //
}
