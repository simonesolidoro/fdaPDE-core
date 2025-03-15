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

/*test per confronto threadpool con singola deque e threadpool con piu worker_queue*/

using namespace std::chrono_literals;

//threadpool con singola deque
template<typename T>
class threadpool_toy{
    private:
        std::deque<T> coda_;
        std::vector<std::thread> threads;
        std::mutex m_; //protegge accesso coda
        std::condition_variable cv;
        int n_thread;
    public:
        threadpool_toy(int n):n_thread(n){
            for (int i=0; i<n; i++){
                threads.emplace_back(&threadpool_toy::run,this);
            }
        }
        
        ~threadpool_toy(){
            for (int i =0; i<n_thread; i++){
                threads[i].join();
            }
        }
        void post(T t){
            std::lock_guard<std::mutex> loc(m_);
            coda_.push_back(t);
            cv.notify_one();
        }

        void run (){
            std::unique_lock<std::mutex> loc(m_); //con cv serve unique_lock non lock_guard
            cv.wait(loc,[this](){return ! coda_.empty();});
            std::this_thread::sleep_for(10ms);
        }
};


//threadpool con piu worker_queue 
template<typename T>
class Workerpool_toy{
    private:
        std::vector<std::shared_ptr<fdapde::Worker_queue<T>>> coda_; //shared ptr perche classe Worker_queue ha mutex quindi non movable
        std::vector<std::thread> threads;
        int n_thread;
        int n_el_wq; //size worker queue
    public:
        Workerpool_toy(int n, int m):n_thread(n),n_el_wq(m){
            // inizializzo singole worker_queue
            for (int i=0; i<n; i++){
                auto ptr= std::make_shared<fdapde::Worker_queue<T>> (n_el_wq);
                coda_.push_back(ptr);
            }
            //inizializzo i thread
            for(int i=0; i<n; i++){
                threads.emplace_back(&Workerpool_toy::run,this,std::ref(coda_),i);
            }
        }
        
        ~Workerpool_toy(){

            for (int i =0; i<n_thread; i++){
                threads[i].join();
            }

            print_all();//per debug
        }

        //per debug
        void print_all(){
            int k=0;
            for (auto x: coda_){
                std::cout<<"coda :"<<k<<" =";
                x->print();
                k++;
            }

        }
        // add same t to all worker_queue
        void post_all(T t){
            for (int i=0; i<n_thread; i++)
                for(int j=0; j<n_el_wq; j++)
                    coda_[i]->push_back(t); 
        }
/*        bool all_empty(){
            bool flag;
            for (auto x: coda_){
                if(! x->empty())
                    return false;
            }
            return true;
        }
*/
        void run (std::vector<std::shared_ptr<fdapde::Worker_queue<T>>> & q, int index){
                    int j=0; //per debug
                    std::cout << "Thread " << index <<"con id:"<<std::this_thread::get_id()<<" sta iniziando"<<std::endl;
                    while(!q[index]->empty()){
                        q[index]->pop_front();
                        std::cout<<"da thread:"<<std::this_thread::get_id()<<"in index"<<index<<"j vale:"<<j<<std::endl;
                        std::this_thread::sleep_for(10ms);
                        j++;
                    }
                    // finito con proprio thread passa a successivo
                    int i=0;
                    int new_index= index;
                    while(i<n_thread){
                        new_index = (new_index != n_thread-1)? (new_index + 1):(0);
                        while(!q[new_index]->empty()){
                            q[new_index]->pop_back(); // back perchè non è il suo
                            //std::cout<<"thread:"<<std::this_thread::get_id()<<"in index"<<new_index<<std::endl;
                            std::this_thread::sleep_for(1s);
                        }
                        i++;
                    }                    
        }
};

int main(){

    // unica deque threapool
    threadpool_toy<int> pool(3);
    std::vector<int> V;
    for(int i =0; i<100; i++){
        V.push_back(i);
    }
    for (auto x: V){
        pool.post(x);
    }

    //threadpool con piu worker queue (workerpool)
    Workerpool_toy<int> wpool(2,3);
    int x=2;
    wpool.post_all(x);
    
    return 0;
}
