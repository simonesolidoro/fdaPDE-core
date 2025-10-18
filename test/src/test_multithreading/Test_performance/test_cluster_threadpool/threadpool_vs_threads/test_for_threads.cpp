
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

void printmatrix(std::vector<std::vector<int>> M, int size){
    for (int i = 0; i<size; i++){
        for (int j = 0; j<size; j++){
            std::cout<<M[i][j]<<" ";    
        }
        std::cout<<std::endl;
    }
}

int main(int argc, char** argv){
    int size = std::stoi(argv[1]); 
    int n_gran = std::stoi(argv[2]); //granularity
    int n_threads = std::stoi(argv[3]);

    std::vector<std::vector<int>> A;
    std::vector<std::vector<int>> B;
    std::vector<std::vector<int>> C;
    for (int i = 0; i<size; i++){
        A.emplace_back(size,1);
        B.emplace_back(size,1);
        C.emplace_back(size,0);
    }

    int total_job = size/n_gran;
    if( size%n_gran != 0 ){
        std::cout<<"granularity non divisore di size"<<std::endl;
        return 0;
    }
    int cicli_while_completi = total_job/n_threads;
    int resto =  total_job%n_threads; //resto_dei_job
    std::vector<std::thread> threads;
//parallel_for_sure_n
    auto start2 = std::chrono::high_resolution_clock::now();
    int l =0; // cicli, ogni ciclo contiene n_thread job
    while(l<cicli_while_completi){
        for(int k = 0; k< n_threads; k++){ //k-esimo job in ciclo 
            threads.emplace_back([&](int y, int L){
                int start = L*n_threads*n_gran+y*n_gran;
                for(int i = 0; i< n_gran; i++){
                    for(int j=0; j<size; j++){
                    C[i+start][j] = A[i+start][j] + B[i+start][j];
                    }
                }
        },k,l);
        }
        for(int i = 0; i< n_threads; i++){
            threads[i+n_threads*l].join();
        }
        l++;
    }

    for(int k = 0; k< resto; k++){ //k-esimo job in ciclo 
        threads.emplace_back([&](int y, int L){
                int start = L*n_threads*n_gran+y*n_gran;
                for(int i = 0; i< n_gran; i++){
                    for(int j=0; j<size; j++){
                    C[i+start][j] = A[i+start][j] + B[i+start][j];
                    }
                }
        },k,l);
    }
    for(int i = 0; i< resto; i++){
        threads[i+n_threads*l].join();
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2); 
        
    std::cout<<duration2.count()<<",";
    //printmatrix(C,size);
    return 0;
}
