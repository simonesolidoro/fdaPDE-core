#include<fdaPDE/multithreading.h>

int main(int argc, char** argv){
    int n_thread= std::stoi(argv[1]);
    int runs = 20;

{//============= TEST PARALLE FOR SUM MATRIX===============================================================
    int granularity = -1; //25
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    std::vector<std::vector<std::chrono::microseconds>> tempi_for(3); // 0 for, 1 parallel_for, 2 openMP
    std::vector<std::string> tag_tipi_for = {"For","Parallel_for","OpenMp"};
    int size = 8000;
    std::vector<std::vector<int>> A;
    std::vector<std::vector<int>> B;
    std::vector<std::vector<int>> C;
    for (int i = 0; i<size; i++){
        A.emplace_back(size,1);
        B.emplace_back(size,1);
        C.emplace_back(size,0);
    }
    fdapde::threadpool<fdapde::round_robin_scheduling ,fdapde::max_load_stealing> tp(1024,n_thread);
    if(n_thread == 2){
        //for
        for(int run = 0; run <runs; run ++){
            start = std::chrono::high_resolution_clock::now();
            for(int i = 0; i<size; i++){
                for(int j=0; j<size; j++){C[i][j] = A[i][j] + B[i][j];}}
            end = std::chrono::high_resolution_clock::now();
            tempi_for[0].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start)); 
        }
    }
    //parallel_for
    for(int run = 0; run <runs; run ++){
        start = std::chrono::high_resolution_clock::now();
        tp.parallel_for(0,size,[&](int i,int index_worker){
            for(int j=0; j<size; j++){
                C[i][j] = A[i][j] + B[i][j];
            }
        },granularity);
        end = std::chrono::high_resolution_clock::now();
        tempi_for[1].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start)); 
    }
    // 2 openMp
    for(int run = 0; run <runs; run ++){
        start = std::chrono::high_resolution_clock::now();
        #pragma omp parallel for num_threads(n_thread) shared(A,B,C)
        for(int i = 0; i<size; i++){
            for(int j=0; j<size; j++){
                C[i][j] = A[i][j] + B[i][j];}}
        end = std::chrono::high_resolution_clock::now();
        tempi_for[2].push_back(std::chrono::duration_cast<std::chrono::microseconds>(end - start)); 
    }

    int inizio = (n_thread == 2)? 0:1; //stampa for solo se n_thread ==2 
    for (int i = inizio; i<3; i++){
        if(i==0){
            std::cout<<tag_tipi_for[i]<<" thread_"<< 1 << " ";
            for(auto& t: tempi_for[i]){std::cout<<t.count()<<" ";}
            std::cout<<std::endl;            
        }else{
            std::cout<<tag_tipi_for[i]<<" thread_"<< n_thread << " ";
            for(auto& t: tempi_for[i]){std::cout<<t.count()<<" ";}
            std::cout<<std::endl;
        }
    }
}


return 0;
}
