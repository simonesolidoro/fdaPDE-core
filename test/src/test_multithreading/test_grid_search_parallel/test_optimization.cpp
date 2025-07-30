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
#include<fdaPDE/optimization.h>
#include<fdaPDE/fields.h>
#include<fdaPDE/utility.h>
int main(){

// funzione x^2 + y^2
fdapde::ScalarField<2, decltype([](const Eigen::Matrix<double, 2, 1>& p) { return std::pow(p[0], 2) + std::pow(p[1], 2); })> objective;

// definizione di griglia di possibili valor
Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> grid;
int n_points = 10000;
grid.resize(n_points, 2);
// grid da popolare con la griglia dei valori da esplorare
for (int i = 0; i< n_points; i++){
    grid(i,0) = i+1.0;
    grid(i,1) = i+2.0;
}


// definizione dell'ottimizzatore 
fdapde::GridSearch<2> opt;
opt.optimize(objective, grid,execution::par); // <- da modificare questo step
std::cout<<opt.value()<<std::endl;

return 0;
}
