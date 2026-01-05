#include<fdaPDE/multithreading.h>
#include<fdaPDE/optimization.h>
#include <fdaPDE/finite_elements.h>


int main(int argc, char** argv){
    int n_thread= std::stoi(argv[1]);

    // seq vs gran1 con default granularity -1
{// ============================= ASSEMBLE =============================================
    using namespace fdapde;
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    std::vector<int> nodes = {500}; //MODIFICA NODI PER LATO DI MESH
    std::vector<int> runs_vett = {10};//RUNS
    std::vector<std::vector<std::chrono::microseconds>> tempo_seq_assemble(nodes.size()); //per ogni nodi vettore di tempi di assemblaggio completo. (vediamo se overhead in setfromtriple rimane in cluster (speriamo di no perchè il vttore di triple è uguale in seq e in par))
    std::vector<std::vector<std::chrono::microseconds>> tempo_par_assemble(nodes.size());
    if(n_thread == 2){
        //std::cout<<"=====Tempi SEQUENZIALE========================================================================================================================================================================================================================================================="<<std::endl; 
        for(int nodi = 0; nodi<nodes.size(); nodi++){
            Triangulation<2, 2> unit_square = Triangulation<2, 2>::UnitSquare(nodes[nodi]);
            FeSpace Vh(unit_square, P1<1>);
            TrialFunction u(Vh);
            TestFunction  v(Vh);
            auto a = integral(unit_square)(dot(grad(u), grad(v))); // laplacian weak form
            std::cout<<"calcolo_triple "<<nodes[nodi]<<" sequenziale ";
            for(int run = 0; run <runs_vett[nodi]; run ++){
                Eigen::SparseMatrix<double> A = a.assemble_tempotriple();// il cout dei tempi è dentro assemble() con start e end che racchiudono assemble(...)
            }
            for(int run = 0; run <runs_vett[nodi]; run ++){
                start = std::chrono::high_resolution_clock::now();
                Eigen::SparseMatrix<double> A = a.assemble();// il cout dei tempi è dentro assemble() con start e end che racchiudono assemble(...)
                end = std::chrono::high_resolution_clock::now();
                tempo_seq_assemble[0].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start));
            }            
            std::cout<<std::endl;
        }            
    }
    {
        //std::cout<<"=====Tempi PARALLELO n_thread: "<<n_thread<<"========================================================================================================================================================================================================================================================="<<std::endl; 
        for(int nodi = 0; nodi<nodes.size(); nodi++){
            Triangulation<2, 2> unit_square = Triangulation<2, 2>::UnitSquare(nodes[nodi]);
            FeSpace Vh(unit_square, P1<1>);
            TrialFunction u(Vh);
            TestFunction  v(Vh);
            auto a = integral(unit_square)(dot(grad(u), grad(v))); // laplacian weak form
            std::cout<<"calcolo_triple "<<nodes[nodi]<<" thread_"<<n_thread<<" ";
            for(int run = 0; run <runs_vett[nodi]; run ++){
                fdapde::threadpool<fdapde::round_robin_scheduling, fdapde::random_stealing> Tp(1024,n_thread);
                Eigen::SparseMatrix<double> A = a.assemble_tempotriple(execution::par,Tp,-1);// il cout dei tempi è dentro assemble() con start e end che racchiudono assemble(...)
            }
            for(int run = 0; run <runs_vett[nodi]; run ++){
                fdapde::threadpool<fdapde::round_robin_scheduling, fdapde::random_stealing> Tp(1024,n_thread);
                start = std::chrono::high_resolution_clock::now();
                Eigen::SparseMatrix<double> A = a.assemble(execution::par,Tp,-1);// il cout dei tempi è dentro assemble() con start e end che racchiudono assemble(...)
                end = std::chrono::high_resolution_clock::now();
                tempo_par_assemble[0].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start));
            }
            std::cout<<std::endl;
        }
    }

    {//cout di tempi assemble completi

        if(n_thread == 2){
            for(int nodi = 0; nodi< nodes.size(); nodi++){
                std::cout<<"assemble_completo "<<nodes[nodi]<<" sequenziale ";
                for(auto& t : tempo_seq_assemble[nodi]){std::cout<<t.count()<<" ";}
                std::cout<<std::endl;
            }
        }
        for(int nodi = 0; nodi< nodes.size(); nodi++){
            std::cout<<"assemble_completo "<<nodes[nodi]<<" thread_"<<n_thread<<" ";
            for(auto& t : tempo_par_assemble[nodi]){std::cout<<t.count()<<" ";}
            std::cout<<std::endl;
        }
        
    }

    }

return 0;
}

