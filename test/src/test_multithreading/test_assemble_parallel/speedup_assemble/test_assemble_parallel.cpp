#include <fdaPDE/finite_elements.h>
#include<fdaPDE/multithreading.h>
using namespace fdapde;

int main(int argc, char** argv){
    int nodi = std::stoi(argv[1]);
    int workers = std::stoi(argv[2]);
    fdapde::Threadpool<fdapde::steal::random> Tp(1000,workers);
    Triangulation<2, 2> unit_square = Triangulation<2, 2>::UnitSquare(nodi);

    FeSpace Vh(unit_square, P1<1>);
    TrialFunction u(Vh);
    TestFunction  v(Vh);
    auto a = integral(unit_square)(dot(grad(u), grad(v))); // laplacian weak form

//cronometro assemblaggio non parallello
    auto start = std::chrono::high_resolution_clock::now();

    Eigen::SparseMatrix<double> A = a.assemble_parallel23(Tp);//(execution::par); // use parallel version

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);  
    std::cout<<duration.count()<<",";

    return 0;
}