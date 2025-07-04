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
#include<Eigen/Dense>

int main(int argc, char** argv){
    int size = std::stoi(argv[1]); //numero cicli in contafino // lunghezza singolo job

    Eigen::MatrixXd  A = Eigen::MatrixXd::Ones(size, size);
    Eigen::MatrixXd  B = Eigen::MatrixXd::Ones(size, size);
    Eigen::MatrixXd  C = Eigen::MatrixXd::Zero(size, size);

    auto start2 = std::chrono::high_resolution_clock::now();
    
    C = A+B;

    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2); 
    
    std::cout<<duration2.count()<<",";
    //std::cout << C << std::endl;
    return 0;
}
