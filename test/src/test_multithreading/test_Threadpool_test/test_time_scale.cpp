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
void increment(std::atomic<int>& a, int n){
    for (int j = 0; j<n; j++){
        a++;
        //std::cout<<a<<std::endl;
        //std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
};
void contafino(int n){
    int a= 0;
    for (int j = 0; j<n; j++){
        a++;
        a--;
        a++;
        a--;
    }
    //std::this_thread::sleep_for(std::chrono::milliseconds(1));

}

int main(){
{//contare senza incremento variabile comune
    int n_thread= 8;
    int n_job = 100;
    fdapde::Threadpool tp(n_job,n_thread);
    int n= 100000;
    std::atomic<int> a = 0;

    

    std::vector<std::function<void(int)>> jobs;
    std::vector<std::optional<std::future<bool>>> futs;
    for(int i= 0; i<n_job; i++){
        jobs.push_back(contafino);
    }
    
    auto start2 = std::chrono::high_resolution_clock::now();

    for(int i= 0; i<n_job; i++){
        futs.push_back(std::move(tp.send_task(contafino,n)));
    }
    for(int i= 0; i<n_job; i++){
        if(futs[i]){
            futs[i].value().get();
        }
    }

    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);  
    std::cout<<"operazioni fatte: "<<n*n_job<<"  con n_job: "<<n_job<<"mandati su n_thread:"<<n_thread<<" impiegato:"<<duration2.count()<< " microsecondi\n";
    
    auto start3 = std::chrono::high_resolution_clock::now();
    std::thread t(contafino,n*n_job);
    t.join();
    auto end3 = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start3);  
    std::cout<<"operazioni fatte: "<<n*n_job<<" singolo thread impiegato:"<<duration3.count()<< " microsecondi\n";
    
    //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
}
/*
{//void function, trasformate in bool fittizie coai che con future si aspetta esecuzione
    int n_thread= 4;
    fdapde::Threadpool tp(16,n_thread);
    int n= 10000000;
    std::atomic<int> a = 0;
    
    auto start2 = std::chrono::high_resolution_clock::now();

    auto fut1 = tp.send_task(increment,std::ref(a),n);
    auto fut2 = tp.send_task(increment,std::ref(a),n);
    auto fut3 = tp.send_task(increment,std::ref(a),n);
    auto fut4 = tp.send_task(increment,std::ref(a),n);
    auto fut5 = tp.send_task(increment,std::ref(a),n);


    if(fut5){fut5.value().get();} 
    if(fut4){fut4.value().get();} 
    if(fut3){fut3.value().get();} 
    if(fut2){fut2.value().get();} 
    if(fut1){fut1.value().get();}

    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);  
    std::cout<<"incremento a++: "<<n<<" volte, con n_thread:"<<n_thread<<" impiegato:"<<duration2.count()<< " microsecondi\n";
    std::cout<<"a: "<<a<<std::endl;
    
    auto start3 = std::chrono::high_resolution_clock::now();
    std::thread t(increment,std::ref(a),n*5);
    t.join();
    auto end3 = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start3);  
    std::cout<<"incremento a++: "<<n<<" volte, singolo thread impiegato:"<<duration3.count()<< " microsecondi\n";
    
    //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    std::cout<<"a: "<<a<<std::endl;
}
*/
    int T = std::thread::hardware_concurrency();
    std::cout<<"hardware thread supportati: "<<T<<std::endl;
    return 0;
}

/*   
    CRONOMETRO
    auto start2 = std::chrono::high_resolution_clock::now();
 
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);  
    //std::cout<<"pop_back() in deque di n_elementi: "<<size_coda<<" con n_thread:"<<n_thread<<" impiegato:"<<duration2.count()<< " microsecondi\n";
    std::cout<<duration2.count()<<",";*/

/*
    RISULTATI:  operazioni fatte: 400000000  con n_thread:4 impiegato:287563 microsecondi
                operazioni fatte: 400000000 singolo thread impiegato:701472 microsecondi

                incremento a++: 10000000 volte, con n_thread:4 impiegato:791509 microsecondi
                a: 50000000
                incremento a++: 10000000 volte, singolo thread impiegato:371085 microsecondi
                a: 100000000

                oss: operzioni scollegate multithread da vantaggi, operazioni su stessa ref ovviamente multitrhead non solo non da vantaggi ma peggiora perche concorrenza su stessa ref (ref a valore atomico perche altrimenti multithread era anche sbagliato)
*/