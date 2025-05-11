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

void contafino(int n){
    int a= 0;
    for (int j = 0; j<n; j++){
        a++;
        a--;
        a++;
        a--;
    }
    //std::this_thread::sleep_for(std::chrono::microseconds(1000));

}
template<int N>
void runTest_send_bilaciati(int n, int n_job){
    fdapde::Threadpool<N> tp(n_job);
    std::vector<std::function<void(int)>> jobs;
    std::vector<std::optional<std::future<bool>>> futs;
    for(int i= 0; i<n_job; i++){
        jobs.push_back(contafino);
    }
    
    auto start2 = std::chrono::high_resolution_clock::now();

    for(int i= 0; i<n_job; i++){
        futs.push_back(std::move(tp.send_task_only_to_some(contafino,n)));
    }
    for(int i= 0; i<n_job; i++){
        if(futs[i]){
            futs[i].value().get();
        }
    }

    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);  
    //std::cout<<"operazioni fatte: "<<n*n_job<<"  con n_job: "<<n_job<<"mandati su n_thread:"<<n_thread<<" impiegato:"<<duration2.count()<< " microsecondi\n";
    std::cout<<duration2.count()<<",";
}
template<int N>
void runTest_send_sbilaciati(int n, int n_job){
    fdapde::Threadpool<N> tp(n_job);

    std::vector<std::function<void(int)>> jobs;
    std::vector<std::optional<std::future<bool>>> futs;
    for(int i= 0; i<n_job; i++){
        jobs.push_back(contafino);
    }
    
    auto start2 = std::chrono::high_resolution_clock::now();

    for(int i= 0; i<n_job/2; i++){
        futs.push_back(std::move(tp.send_task_only_to_zero(contafino,n)));
    }
    for(int i= 0; i<n_job/2; i++){
        futs.push_back(std::move(tp.send_task_only_to_some(contafino,n)));
    }
    for(int i= 0; i<n_job; i++){
        if(futs[i]){
            futs[i].value().get();
        }
    }

    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);  
    //std::cout<<"operazioni fatte: "<<n*n_job<<"  con n_job: "<<n_job<<"mandati su n_thread:"<<n_thread<<" impiegato:"<<duration2.count()<< " microsecondi\n";
    std::cout<<duration2.count()<<",";
}

int main(int argc, char** argv){
/*
{//manda solo a meta dei thread. dato che job uniformi tra meta thread il most busy ca,biera spesso e quindi no starvatio e quindi simile random. infatti in plot stessi risultati
    int n = std::stoi(argv[1]); //numero cicli in contafino // lunghezza singolo job

    int n_thread = std::stoi(argv[2]);
    int n_job = 100;
    //per definire threadpool template va fatta at compile time, ma cosi possiile passare a file umero di thread
    switch (n_thread)
    {
    case 1:
        runTest_send_bilaciati<1>(n, n_job);
        break;
    case 2:
        runTest_send_bilaciati<2>(n, n_job);
        break;
    case 3:
        runTest_send_bilaciati<3>(n, n_job);
        break;
    case 4:
        runTest_send_bilaciati<4>(n, n_job);
        break;
    case 5:
        runTest_send_bilaciati<5>(n, n_job);
        break;
    case 6:
        runTest_send_bilaciati<6>(n, n_job);
        break;
    case 7:
        runTest_send_bilaciati<7>(n, n_job);
        break;
    case 8:
        runTest_send_bilaciati<8>(n, n_job);
        break;
    case 10:
        runTest_send_bilaciati<10>(n, n_job);
        break;
    case 12:
        runTest_send_bilaciati<12>(n, n_job);
        break;
    case 14:
        runTest_send_bilaciati<14>(n, n_job);
        break;
    case 16:
        runTest_send_bilaciati<16>(n, n_job);
        break;
    case 18:
        runTest_send_bilaciati<18>(n, n_job);
        break;

    default:
        break;
    }
}*/

{//manda meta job totali solo a uno e poi atra meta solo a meta worker, cosi si crea asimmetria e starvetion in steal_from_most_busy dovrebbe essere piu evidente e peggiorare prestazioni
    int n = std::stoi(argv[1]); //numero cicli in contafino // lunghezza singolo job

    int n_thread = std::stoi(argv[2]);
    int n_job = 100;
    //per definire threadpool template va fatta at compile time, ma cosi possiile passare a file umero di thread
    switch (n_thread)
    {
    case 1:
        runTest_send_sbilaciati<1>(n, n_job);
        break;
    case 2:
        runTest_send_sbilaciati<2>(n, n_job);
        break;
    case 3:
        runTest_send_sbilaciati<3>(n, n_job);
        break;
    case 4:
        runTest_send_sbilaciati<4>(n, n_job);
        break;
    case 5:
        runTest_send_sbilaciati<5>(n, n_job);
        break;
    case 6:
        runTest_send_sbilaciati<6>(n, n_job);
        break;
    case 7:
        runTest_send_sbilaciati<7>(n, n_job);
        break;
    case 8:
        runTest_send_sbilaciati<8>(n, n_job);
        break;
    case 10:
        runTest_send_sbilaciati<10>(n, n_job);
        break;
    case 12:
        runTest_send_sbilaciati<12>(n, n_job);
        break;
    case 14:
        runTest_send_sbilaciati<14>(n, n_job);
        break;
    case 16:
        runTest_send_sbilaciati<16>(n, n_job);
        break;
    case 18:
        runTest_send_sbilaciati<18>(n, n_job);
        break;

    default:
        break;
    }
    
}

    return 0;
}



