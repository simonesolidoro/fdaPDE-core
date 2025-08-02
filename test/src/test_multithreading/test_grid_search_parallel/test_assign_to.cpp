
#include<fdaPDE/optimization.h>

int main(){
using vector_t = Eigen::Matrix<double, 2, 1>;
//using vector_t = Eigen::Matrix<double, 1, 2>;
using grid_t = fdapde::MdMap<const double, fdapde::MdExtents<Eigen::Dynamic, Eigen::Dynamic>>;

// funzione x^2 + y^2
fdapde::ScalarField<2, decltype([](const Eigen::Matrix<double, 2, 1>& p) { return std::pow(p[0], 2) + std::pow(p[1], 2); })> objective;

// definizione di griglia di possibili valor
Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> grid;
int n_points = 10;
grid.resize(n_points, 2);
// grid da popolare con la griglia dei valori da esplorare
for (int i = 0; i< n_points; i++){
    grid(i,0) = i;
    grid(i,1) = i+1;
}
std::cout<<grid<<std::endl;
//Eigen::Matrix<double, 2, 1> x;
Eigen::VectorXd x(2);
grid_t grid_;
grid_ = grid_t(grid.data(),n_points,2); 
grid_.row(0).assign_to(x);
std::cout<<x<<std::endl;

return 0;
}

