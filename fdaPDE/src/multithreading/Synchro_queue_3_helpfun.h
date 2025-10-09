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

#ifndef __Synchro_queue_3_helpfun_H__
#define __Synchro_queue_3_helpfun_H__   
#include"Synchro_queue_3.h"

namespace fdapde{
    //definizioni di funzioni friend per indx
    //TODO: possiile mettere lock di mutex dentro *_indx function.
    //TODO: spostare notify() dopo unlock cosi si evita di svegliare e far fare tentativo di lock a thread dormiente quando il thread che notifica ancora ha il mutex lock. (vedi esempio qui: https://en.cppreference.com/w/cpp/thread/condition_variable.html)
           // oss: forse il compilatore ottimizza trasferendo direttamente la proprietà del mutex e quindi aspetta unlock() dopo notify() e poi sveglia CV, ma non è sicuro meglio spostare notify() dopo unlock direttamente per certezza
    template<typename T,typename M> 
    int push_f_indx(Synchro_queue<T,M> & S){
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait>){ //OSS: tolto hold_wait perche wait non puo fallire se inizia è perche CV ha gia checcato che coda non sia piena
            if (S.head_ == S.tail_ && !S.empty_queue_ ){// TODO: capire se possibile togliere questo check in relax_nowait perche tanto se coda piena elemento in cui si vuole fare push sara full
                //std::cerr<<"queue full"<<std::endl; // per debug poi da togliere
                return -1;
            }
            S.empty_queue_ = false; //magari gia false quindi ridondante,ma evita if(empty_queue_) {empty_queue_ = false;} non so quale piu efficiente. 
        }
        int h = S.head_; //index dove inserira elemento
        if constexpr(std::is_same_v<M,relax>){//OSS: non atomico lettura e modifica di stato con compare_exchange perchè tanto è gia dentro al mutex. oss fuori da mutex unico cambiamento è da busy->empty/full e qui la scrittura viene fatta solo se la lettura da !=busy quindi ce garanzia che tra lettura e scrittura non cambi niente nel mentre
            if(S.queue_[h].state_.load(std::memory_order_acquire) != Synchro_queue<T,relax>::Empty)
                return -1;
            S.queue_[h].state_.store(Synchro_queue<T,relax>::Busy, std::memory_order_relaxed); //TODO: capire se forse dato che dentro mutex memory order superfluo. forse ottimale  memory_order_relax. SI
        }
        S.head_ = (S.head_ == S.size_-1)? (0) : (S.head_ + 1); //head_++
        return h;
    };

    template<typename T,typename M> 
    int pop_f_indx(Synchro_queue<T,M> & S){
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait> ){
            if (S.empty_queue_){
                //std::cerr<<"queue empty"<<std::endl;
                return -1;
            }
        }
        int new_head = (S.head_== 0)? (S.size_-1) : (S.head_-1); // new_head = index di elemento da rimuovere
        if constexpr(std::is_same_v<M,relax>){
            if(S.queue_[new_head].state_.load(std::memory_order_acquire) != Synchro_queue<T,relax>::Full)
                return -1;
            S.queue_[new_head].state_.store(Synchro_queue<T,relax>::Busy, std::memory_order_relaxed);
        }
        S.head_ = new_head; 
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait>){
            if(S.head_==S.tail_) {S.empty_queue_ = true;} 
            S.queue_[new_head].count_pop_ ++;
        }
        return new_head;
    };

    template<typename T, typename M>
    int push_b_indx(Synchro_queue<T,M> & S){
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait>){
            if (S.head_ == S.tail_ && !S.empty_queue_ ){// coda piena
                //std::cerr<<"queue full"<<std::endl; // per debug poi da togliere
                return -1;
            }
            S.empty_queue_ = false;
        }
        int new_tail = (S.tail_ == 0)? (S.size_-1) : (S.tail_ -1);
        if constexpr(std::is_same_v<M,relax>){
            if(S.queue_[new_tail].state_.load(std::memory_order_acquire) != Synchro_queue<T,relax>::Empty)
                return -1;
            S.queue_[new_tail].state_.store(Synchro_queue<T,relax>::Busy, std::memory_order_relaxed);
        }
        S.tail_ = new_tail;                         
        return new_tail;
    };

    template<typename T,typename M>
    int pop_b_indx(Synchro_queue<T,M> & S){
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait>){
            if(S.empty_queue_ ){
                //std::cerr << "Queue is empty" << std::endl;
                return -1;
            }
        }
        int t = S.tail_; // tmp idice di elmeto da pop
        if constexpr(std::is_same_v<M,relax>){
            if(S.queue_[t].state_.load(std::memory_order_acquire) != Synchro_queue<T,relax>::Full)
                return -1;
            S.queue_[t].state_.store(Synchro_queue<T,relax>::Busy, std::memory_order_relaxed);
        }
        S.tail_ = (S.tail_ == S.size_-1)? (0):(S.tail_+1);
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait>){
            if(S.head_==S.tail_) {S.empty_queue_ = true;}
            S.queue_[t].count_pop_ ++;
        }   
        return t;
    };

    // helper func per push/pop
    template<typename T,typename M> 
    void push_fb_push(elem<T,M>& E, T& new_value){ 
        E.v_ = std::move(new_value);
        if constexpr(std::is_same_v<M,relax>){
            E.state_.store(Synchro_queue<T,relax>::Full, std::memory_order_release); //aggiorna stato di elem con release.  SE un pop vede lo stato = FUll grazie a release allora sicuro vede anche l'elemeto pushato
        }
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait>){
            E.state_ = false; //aggiorna stato di elem a full
        }
    };

    template<typename T,typename M> 
    T pop_fb_pop(elem<T,M>& E){
        T ret = std::move(E.v_.value());
        E.v_ = std::nullopt;
        if constexpr(std::is_same_v<M,relax>){
            E.state_.store(Synchro_queue<T,relax>::Empty, std::memory_order_release);
        }
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait>){
            E.state_ = true;
            E.count_pop_ --;
        }
        return ret; 
    };
}

#endif