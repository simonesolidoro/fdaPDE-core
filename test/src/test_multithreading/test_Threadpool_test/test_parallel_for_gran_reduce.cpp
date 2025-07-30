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

int main(int argc,char** argv)
{   
    int n = 10000;
    fdapde::Threadpool<fdapde::steal::random> tp(64,6);
    std::atomic<int> a=0;

// parallel_for_sure_granularity
    a.store(10);
    int n_it = std::stoi(argv[1]); //numero di blocchi in cui dividere range di for
    auto start3 = std::chrono::high_resolution_clock::now();
    int min;
    min =tp.parallel_for_sure_granularity_reduce_min(0,1200,n_it,[&](int i){a++;
        return a.load();
    });
    
    auto end3 = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start3);  
    std::cout<<"par_for_sure_n - incrementata a da 0 ad: "<<a.load()<<"  impiegato:"<<duration3.count()<< " microsecondi con n_it: "<<n_it<<std::endl;
    std::cout<<"minimo trovatp: "<<min<<std::endl;

}
