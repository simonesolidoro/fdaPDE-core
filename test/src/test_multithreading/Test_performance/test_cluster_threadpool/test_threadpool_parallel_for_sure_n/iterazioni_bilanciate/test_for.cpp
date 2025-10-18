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

int main(int argc, char** argv){
    int n_range_for = std::stoi(argv[1]); //numero cicli in contafino // lunghezza singolo job
    int operazioni_in_singola_body_function = std::stoi(argv[2]); // rappresenta "pesantezza" body_function
    //int n_thread = std::stoi(argv[3]);
    //int size_coda = std::stoi(argv[4]);
    //int n_blocchi = std::stoi(argv[5]); //numero blocchi in cui dividere range di for (utilizzato solo in parallel_for_sure_n)
    //fdapde::Threadpool<fdapde::steal::random> tp(size_coda,n_thread);
    std::atomic<int> a=0; //usata per verifica tutti jo vengano eseguiti (a deve arrivare ad n)

    auto fun_body = [=](int i){
        // for inutile simula lavoro di body function
        
        int b = 0;
        for (int j = 0; j< operazioni_in_singola_body_function; j++){
            b ++;
            b --;
            b ++;
            b --;
        }
    
        //std::this_thread::sleep_for(std::chrono::milliseconds(1)); //usato al posto di operazioi pr dimostrare che speedup o ottimale dovuto a cosumo di cpu
        //a++;
    };
//for non parallel
    auto start2 = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < n_range_for; i++){
       fun_body(i);
    }
    
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2); 
    //stampa risultato solo se tutti i job eseguiti. (poi check lunghezza dati in python per verificare che tutti run eseguiti correttamente. se cosi non è ce bug in threadpool da risolvere)
    //if (a.load() == n_range_for) 
        //std::cout<<"for - incrementata a da 0 ad: "<<a.load()<<"  impiegato:"<<duration2.count()<< " microsecondi\n";
        std::cout<<duration2.count()<<",";

    return 0; ///
}
