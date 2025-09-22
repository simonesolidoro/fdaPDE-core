#include <fdaPDE/finite_elements.h>
#include<fdaPDE/multithreading.h>
using namespace fdapde;

int main(int argc, char** argv){
    int nodi = std::stoi(argv[1]);
    int workers = std::stoi(argv[2]);
    int kk = std::stoi(argv[3]);
    fdapde::Threadpool<fdapde::steal::random> Tp(1000,workers);
    Triangulation<2, 2> unit_square = Triangulation<2, 2>::UnitSquare(nodi);

    FeSpace Vh(unit_square, P1<1>);
    TrialFunction u(Vh);
    TestFunction  v(Vh);

    auto b = integral(unit_square)(dot(grad(u), grad(v))); // laplacian weak form
    
//cronometro assemblaggio parallelo
    auto start2 = std::chrono::high_resolution_clock::now();

    //Eigen::SparseMatrix<double> A2 = b.assemble(execution::par,Tp,kk); // use parallel version 
    Eigen::SparseMatrix<double> A2 = b.assemble_unicovettore(execution::par,Tp,kk);

    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);  
    std::cout<<"tempo (microsec) assemblaggio parallelo, thread: "<<workers<<" ,"<<duration2.count()<<std::endl; 

    return 0;
}
