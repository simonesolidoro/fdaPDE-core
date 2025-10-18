#include <fdaPDE/finite_elements.h>
#include<fdaPDE/multithreading.h>
using namespace fdapde;


int main(int argc, char** argv){
    int nodi = std::stoi(argv[1]);
    int workers = std::stoi(argv[2]);
    int kk = std::stoi(argv[3]);
    fdapde::Threadpool<fdapde::steal::random> Tp(1000,workers);
    std::cout<<"tempo (microsec) calcolo triple parallelo, thread: "<<workers<<std::endl; 
for(int i=11; i>1; i--){
    Triangulation<2, 2> unit_square = Triangulation<2, 2>::UnitSquare(nodi);

    FeSpace Vh(unit_square, P1<1>);
    TrialFunction u(Vh);
    TestFunction  v(Vh);

    auto b = integral(unit_square)(dot(grad(u), grad(v))); // laplacian weak form
    
    Eigen::SparseMatrix<double> A2 = b.assemble(execution::par,Tp,kk); // use parallel version 
    

}
std::cout<<std::endl;
    return 0;
}
