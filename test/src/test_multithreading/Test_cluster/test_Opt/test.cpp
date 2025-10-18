#include<fdaPDE/multithreading.h>
#include<fdaPDE/optimization.h>
#include <fdaPDE/finite_elements.h>


int main(int argc, char** argv){
    int n_thread= std::stoi(argv[1]);


{//==================== GRID SEARCH ==============================================
    // default granularity poi un altro per effetto granularity
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    std::vector<int> grid_sizes = {100000,1000000,10000000}; //10^8 per cluster 
    std::vector<int> runs_vett = {1,1,1}; //{50,50,20}; // 10 rus di 10alla8 che sono 4 sec l'una
    std::vector<std::vector<std::chrono::microseconds>> tempi_seq(grid_sizes.size()); // vect esterno un elemento per ogni grid_size, quelli interni sono tempi di runs
    std::vector<std::vector<std::chrono::microseconds>> tempi_par(grid_sizes.size());
    double lower = -5;
    double upper = 5;
    std::uniform_real_distribution<double> unif(lower,upper);
    std::default_random_engine re;    
    //rastrigin function
    fdapde::ScalarField<2, decltype([](const Eigen::Matrix<double, 2, 1>& p) { return 20 + p[0]*p[0] + p[1]*p[1] - 10*std::cos(2*M_PI*p[0]) - 10*std::cos(2*M_PI*p[1]); })> rastrigin;
    for(int gs = 0; gs< grid_sizes.size(); gs++){
        Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> grid;
        grid.resize(grid_sizes[gs],2);
        for(int i =0; i<grid.rows();++i){
            grid(i,0) = unif(re);
            grid(i,1) = unif(re);
        } 
    {//seq
        if(n_thread == 2){
            for (int run = 0; run<runs_vett[gs]; run ++){
                fdapde::GridSearch<2> opt;
                start = std::chrono::high_resolution_clock::now();
                opt.optimize(rastrigin, grid);
                end = std::chrono::high_resolution_clock::now();
                tempi_seq[gs].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start));
            }
        }
    }
    {//parallel
        for (int run = 0; run<runs_vett[gs]; run ++){
            fdapde::Threadpool<fdapde::steal::random> Tp(1024, n_thread);
            fdapde::GridSearch<2> opt;
            start = std::chrono::high_resolution_clock::now();
            opt.optimize(rastrigin, grid, execution::par,Tp,-1);
            end = std::chrono::high_resolution_clock::now();
            tempi_par[gs].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start));
        }
    }
    }
    /*==== effetto granularity =======================================*/
    int size_grid_eg = 10000000; //test effetto gran solo su 10^7
    std::vector<int> grans = {size_grid_eg/(n_thread*10), size_grid_eg/(n_thread*100), size_grid_eg/(n_thread*200), size_grid_eg/(n_thread*500)};
    std::vector<std::vector<std::chrono::microseconds>> tempi_par_gran(grans.size()); //tempi in diverse gran
    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> grid;
    grid.resize(size_grid_eg,2);
    for(int i =0; i<grid.rows();++i){
        grid(i,0) = unif(re);
        grid(i,1) = unif(re);
    } 
    for(int gran = 0; gran< grans.size(); gran++){
        for (int run = 0; run<runs_vett[2]; run ++){
            fdapde::Threadpool<fdapde::steal::random> Tp(1024, n_thread);
            fdapde::GridSearch<2> opt;
            start = std::chrono::high_resolution_clock::now();
            opt.optimize(rastrigin, grid, execution::par,Tp,grans[gran]);
            end = std::chrono::high_resolution_clock::now();
            tempi_par_gran[gran].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start));
        }
    }

    std::cout<<std::endl;
    std::cout<<"================================================================================================================================================================================================================="<<std::endl;
    std::cout<<"===========TEST GRID_SEARCH======================================================================================================================================================================================"<<std::endl;
    std::cout<<"================================================================================================================================================================================================================="<<std::endl;
    std::cout<<"== granularity di defaul (-1) e vari size grid==================================================================================================================================================================="<<std::endl;
    for(int gs = 0; gs< grid_sizes.size(); gs++){
        std::cout<<"SIZE_GRID: "<<grid_sizes[gs]<<std::endl;
        if(n_thread == 2){
            std::cout<<"tempi sequenziale: ";
            for(auto& t : tempi_seq[gs]){std::cout<<t.count()<<" ,";}
            std::cout<<std::endl;
        }
        std::cout<<"tempi Parallelo con "<<n_thread<<" thread: ";
        for(auto& t : tempi_par[gs]){std::cout<<t.count()<<" ,";}
        std::cout<<std::endl;
        std::cout<<std::endl;
    }
    std::cout<<"== size grid fissa 10^7, n_thread: "<<n_thread<< ", varia granularity==================================================================================================================================================================="<<std::endl;
    for(int gr = 0; gr< grans.size(); gr++){
        std::cout<<"Granularity: "<<grans[gr]<<std::endl;
        std::cout<<"tempi Parallelo con "<<n_thread<<" thread: ";
        for(auto& t : tempi_par_gran[gr]){std::cout<<t.count()<<" ,";}
        std::cout<<std::endl;
        std::cout<<std::endl;
    }
}
{// ============================= ASSEMBLE =============================================
    std::cout<<"================================================================================================================================================================================================================="<<std::endl;
    std::cout<<"==========TEST ASSEMBLE MATRICE FORMA BILINEARE======================================================================================================================================================================================================="<<std::endl;
    std::cout<<"================================================================================================================================================================================================================="<<std::endl;
    using namespace fdapde;
    std::cout<<"===============CALCOLO TRIPLE, granularity default (-1) ====================================================================================================================================================================================================================================================="<<std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    std::vector<int> nodes = {250,500,1000}; // cosi da avere anche scalabilità debole con 8 16 32 oppure 16 32 64
    std::vector<int> runs_vett = {1,1,1};// {50,50,20};
    int run_effetto_triple = 1; //10
    std::vector<std::vector<std::chrono::microseconds>> tempo_seq_assemble(nodes.size()); //per ogni nodi vettore di tempi di assemblaggio completo. (vediamo se overhead in setfromtriple rimane in cluster (speriamo di no perchè il vttore di triple è uguale in seq e in par))
    std::vector<std::vector<std::chrono::microseconds>> tempo_par_assemble(nodes.size());
    if(n_thread == 2){
        std::cout<<"=====Tempi SEQUENZIALE========================================================================================================================================================================================================================================================="<<std::endl; 
        for(int nodi = 0; nodi<nodes.size(); nodi++){
            Triangulation<2, 2> unit_square = Triangulation<2, 2>::UnitSquare(nodes[nodi]);
            FeSpace Vh(unit_square, P1<1>);
            TrialFunction u(Vh);
            TestFunction  v(Vh);
            auto a = integral(unit_square)(dot(grad(u), grad(v))); // laplacian weak form
            std::cout<<"Nodi per lato: "<<nodes[nodi]<<": ";
            for(int run = 0; run <runs_vett[nodi]; run ++){
                Eigen::SparseMatrix<double> A = a.assemble_tempotriple();// il cout dei tempi è dentro assemble() con start e end che racchiudono assemble(...)
            }
            for(int run = 0; run <runs_vett[nodi]; run ++){
                start = std::chrono::high_resolution_clock::now();
                Eigen::SparseMatrix<double> A2 = a.assemble();
                end = std::chrono::high_resolution_clock::now();
                tempo_seq_assemble[nodi].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start));
            }
            std::cout<<std::endl;
            std::cout<<std::endl;
        }            
    }
    std::cout<<std::endl;
    {
        std::cout<<"=====Tempi PARALLELO n_thread: "<<n_thread<<"========================================================================================================================================================================================================================================================="<<std::endl; 
        for(int nodi = 0; nodi<nodes.size(); nodi++){
            Triangulation<2, 2> unit_square = Triangulation<2, 2>::UnitSquare(nodes[nodi]);
            FeSpace Vh(unit_square, P1<1>);
            TrialFunction u(Vh);
            TestFunction  v(Vh);
            auto a = integral(unit_square)(dot(grad(u), grad(v))); // laplacian weak form
            std::cout<<"Nodi per lato: "<<nodes[nodi]<<": ";
            for(int run = 0; run <runs_vett[nodi]; run ++){
                fdapde::Threadpool<fdapde::steal::random> Tp(1024,n_thread);
                Eigen::SparseMatrix<double> A = a.assemble_tempotriple(execution::par,Tp,-1);// il cout dei tempi è dentro assemble() con start e end che racchiudono assemble(...)
            }
            for(int run = 0; run <runs_vett[nodi]; run ++){
                fdapde::Threadpool<fdapde::steal::random> Tp(1024,n_thread);
                start = std::chrono::high_resolution_clock::now();
                Eigen::SparseMatrix<double> A = a.assemble(execution::par,Tp,-1);
                end = std::chrono::high_resolution_clock::now();
                tempo_par_assemble[nodi].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start));
            }
            std::cout<<std::endl;
            std::cout<<std::endl;
        }
    }
    std::cout<<std::endl;
    std::cout<<"===============CALCOLO TRIPLE, granularity varia nodi:1000*1000 ====================================================================================================================================================================================================================================================="<<std::endl; 
    {
        int nodi = 1000;
        std::vector<int> grans = {(nodi*nodi)/(n_thread*10), (nodi*nodi)/(n_thread*100), (nodi*nodi)/(n_thread*200)};
        std::cout<<"=====Tempi PARALLELO n_thread: "<<n_thread<<"========================================================================================================================================================================================================================================================="<<std::endl; 
        Triangulation<2, 2> unit_square = Triangulation<2, 2>::UnitSquare(nodi);
        FeSpace Vh(unit_square, P1<1>);
        TrialFunction u(Vh);
        TestFunction  v(Vh);
        auto a = integral(unit_square)(dot(grad(u), grad(v))); // laplacian weak form
        for(int gran = 0; gran<grans.size(); gran++){
            std::cout<<"granularity: "<<grans[gran]<<" tempi: ";
            for(int run = 0; run <run_effetto_triple; run ++){
                fdapde::Threadpool<fdapde::steal::random> Tp(1024,n_thread);
                Eigen::SparseMatrix<double> A = a.assemble_tempotriple(execution::par,Tp,grans[gran]);// il cout dei tempi è dentro assemble() con start e end che racchiudono assemble(...)
            }
            std::cout<<std::endl;
            std::cout<<std::endl;
        }
        
    }
    {//cout di tempi assemble completi
        std::cout<<std::endl;
        std::cout<<"===============ASSEMBLE COMPLETO, granularity default -1 nodi:250,500,1000 ====================================================================================================================================================================================================================================================="<<std::endl; 
        for(int nodi = 0; nodi< nodes.size(); nodi++){
        std::cout<<"NODI PER LATO: "<<nodes[nodi]<<std::endl;
        if(n_thread == 2){
            std::cout<<"tempi sequenziale: ";
            for(auto& t : tempo_seq_assemble[nodi]){std::cout<<t.count()<<" ,";}
            std::cout<<std::endl;
        }
        std::cout<<"tempi Parallelo con "<<n_thread<<" thread: ";
        for(auto& t : tempo_par_assemble[nodi]){std::cout<<t.count()<<" ,";}
        std::cout<<std::endl;
        std::cout<<std::endl;
    }
    }
}   



return 0;
}
