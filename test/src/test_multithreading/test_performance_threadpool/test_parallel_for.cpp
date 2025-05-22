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

void printnum(int i){
    thread_local int a;
    a++;
    std::cout<<std::this_thread::get_id()<<"A : "<<a<<std::endl;
    std::this_thread::sleep_for(std::chrono::microseconds(10000));
}
void printnumb(int i){
    thread_local int b;
    b++;
    std::cout<<std::this_thread::get_id()<<"B : "<<b<<std::endl;
    std::this_thread::sleep_for(std::chrono::microseconds(10000));
}
void fun_con_i_e_void(int a){
    std::cout<<std::this_thread::get_id()<<"fun_con_i_e_void"<<std::endl;
}
int fun_con_i_e_return(int a){
    std::cout<<std::this_thread::get_id()<<"fun_con_i_e_return"<<std::endl;
    return a;
}
void fun_con_args_e_void(int a,int b){
    std::cout<<std::this_thread::get_id()<<"fun_con_args_e_void"<<std::endl;
}
int fun_con_args_e_return(int a,int b){
    std::cout<<std::this_thread::get_id()<<"fun_con_args_e_return"<<std::endl;
    return a+b;
}
int main(){
/*
{   
    fdapde::Threadpool<fdapde::steal::random> tp(64,4);
    tp.parallel_for(0,16,[&](int i){printnum(i);});
    tp.parallel_for(9,printnumb,7);
}
{
    fdapde::Threadpool<fdapde::steal::random> tp(64,8);
    std::vector<std::optional<std::future<int>>> futs1;
    std::vector<std::optional<std::future<int>>> futs2;
    std::vector<std::optional<std::future<void>>> futs3;
    std::vector<std::optional<std::future<void>>> futs4;
    futs3 = tp.parallel_for(0,8,fun_con_i_e_void);
    futs1 = tp.parallel_for(0,8,fun_con_i_e_return);
    

    futs4 = tp.parallel_for(9,fun_con_args_e_void,4,5);
    futs2 = tp.parallel_for(9,fun_con_args_e_return,4,5);

    std::this_thread::sleep_for(std::chrono::microseconds(10000));
    std::cout<<"return di fun_con_i_e_return"<<std::endl;
    for(size_t k = 0; k<futs1.size(); k++){
        if(futs1[k]){
            std::cout<<futs1[k].value().get()<<std::endl;
        }
    }
    std::this_thread::sleep_for(std::chrono::microseconds(10000));
    std::cout<<"return di fun_con_i_e_void"<<std::endl;
    for(size_t k = 0; k<futs1.size(); k++){
        if(futs3[k]){
            futs3[k].value().get();
            std::cout<<"futs3[k].value().get()"<<std::endl;
        }
    }

    std::cout<<"return di fun_con_args_e_void"<<std::endl;
    for(size_t k = 0; k<futs2.size(); k++){
        if(futs4[k]){
            futs4[k].value().get();
            std::cout<<"futs4[k].value().get()"<<std::endl;
        }
    }
    std::cout<<"return di fun_con_args_e_return"<<std::endl;
    for(size_t k = 0; k<futs2.size(); k++){
        if(futs2[k]){
            std::cout<<futs2[k].value().get()<<std::endl;
        }
    }

}
{
    fdapde::Threadpool<fdapde::steal::random> tp(64,8);
        //std::atomic<int> a = 0;
    tp.parallel_for(100,[](){
        thread_local int a;
        std::cout<<std::this_thread::get_id()<<"incrementa a:"<<++a<<std::endl;
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    });
}
*/
{//parallel_for_iteartor;  parallel_for_iteartor_ref  
    std::cout<<"test scrorrere conteiner con iterator"<<std::endl;
    fdapde::Threadpool<fdapde::steal::random_half_most_busy> tp(64,3);
    std::vector<int> V;
    //std::list<int> V;
    for (int i=0; i<10; i++){
        V.push_back(i);
    }
    tp.parallel_for_iterator(V.begin(),V.end(),[](int a){
        std::cout<<std::this_thread::get_id()<<"printa: "<<a<<std::endl;
    });
    std::vector<std::optional<std::future<int>>> ret = tp.parallel_for_iterator(V.begin(),V.end(),[](int a){return a+100;});
    for (size_t i=0; i<ret.size(); i++){
        if(ret[i]){
            std::cout<<ret[i].value().get()<<" : "<<std::endl;
        }
    }
    std::vector<std::optional<std::future<void>>> ret_void = tp.parallel_for_iterator_ref(V.begin(),V.end(),[](int& a){a+=100;});
    for (size_t i=0; i<ret_void.size(); i++){
        if(ret_void[i]){
            ret_void[i].value().get();
            std::cout<<"ret_void[i].value().get() : "<<std::endl;
        }
    }
    for (int i=0; i<10; i++){
        std::cout<<"V[i]:"<<V[i]<<std::endl;
    }


}
    
    return 0;
}
