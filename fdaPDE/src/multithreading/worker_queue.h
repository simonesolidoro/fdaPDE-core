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
//
//

#ifndef __FDAPDE_WORKER_QUEUE_H__
#define __FDAPDE_WORKER_QUEUE_H__

#include "header_check.h"

namespace fdapde {
    template <typename T> 
    class Worker_queue{
    using value_type= T;
        private:
            std::vector<value_type> coda_;
            int head;
            int tail;
            std::mutex m;
        public:
            // default constructor
            Worker_queue(){
                head = 0;
                tail = 0;
            };
            // construct whit size of coda_=n;
            Worker_queue(int n){
                coda_.resize(n);
                head = 0;
                tail = 0;
            }

            bool push_front(value_type t);
            T pop_front();
            
            //member to be thread_safe (only )
            bool push_back(value_type t);
            T pop_back();

            // wrap of function size() empty() of vector
            int size(){
                return coda_.size();
            }
            bool empty(){
                return coda_.empy();
            }

            int get_tail()const {return tail;}
            int get_head()const {return head;}



    };

};

#endif