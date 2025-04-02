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
#include<list>

int main(){
    using value = int;
    value el = 1;
    std::list<value> v ={el,el,el};

    fdapde::Worker_queue<int> q(v.begin(),v.end());
    q.print();
    std::vector<value> v1 ={el,el,el};

    fdapde::Worker_queue<int> q1(v1.begin(),v1.end());
    q1.print();
    std::array<value,3> v2 ={el,el,el};

    fdapde::Worker_queue<int> q2(v2.begin(),v2.end());
    q2.print();


    return 1;

}
