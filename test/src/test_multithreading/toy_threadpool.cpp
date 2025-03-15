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

using namespace std::chrono_literals;

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
            cv.notify_all();
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

int main(){
    threadpool_toy<int> pool(3);
    std::vector<int> V;
    for(int i =0; i<100; i++){
        V.push_back(i);
    }
    for (auto x: V){
        pool.post(x);
    }
    return 0;
}