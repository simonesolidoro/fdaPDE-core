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

#ifndef __synchro_queue_helpfun_H__
#define __synchro_queue_helpfun_H__   
#include"synchro_queue.h"

namespace fdapde{
    //definition of friend function to move index
    template<typename T,typename M> 
    int push_b_indx(synchro_queue<T,M> & S){
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait>){ //
            if (S.head_ == S.tail_ && !S.empty_queue_ ){return -1;}
            S.empty_queue_ = false; //maybe already false, so redundant, but avoids if(empty_queue_) {empty_queue_ = false;} 
        }
        int t = S.tail_; //index to return
        if constexpr(std::is_same_v<M,relax>){
            if(S.queue_[t].state_.load(std::memory_order_acquire) != synchro_queue<T,relax>::Empty)
                return -1;
            S.queue_[t].state_.store(synchro_queue<T,relax>::Busy, std::memory_order_relaxed); 
        }
        S.tail_ = (S.tail_ == S.size_-1)? (0) : (S.tail_ + 1); //tail_++
        return t;
    };

    template<typename T,typename M> 
    int pop_b_indx(synchro_queue<T,M> & S){
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait> ){
            if (S.empty_queue_){
                return -1;
            }
        }
        int new_tail = (S.tail_== 0)? (S.size_-1) : (S.tail_-1);
        if constexpr(std::is_same_v<M,relax>){
            if(S.queue_[new_tail].state_.load(std::memory_order_acquire) != synchro_queue<T,relax>::Full)
                return -1;
            S.queue_[new_tail].state_.store(synchro_queue<T,relax>::Busy, std::memory_order_relaxed);
        }
        S.tail_ = new_tail; 
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait>){
            if(S.head_==S.tail_) {S.empty_queue_ = true;} 
            S.queue_[new_tail].count_pop_ ++;
        }
        return new_tail;
    };

    template<typename T, typename M>
    int push_f_indx(synchro_queue<T,M> & S){
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait>){
            if (S.head_ == S.tail_ && !S.empty_queue_ ){
                return -1;
            }
            S.empty_queue_ = false;
        }
        int new_head = (S.head_ == 0)? (S.size_-1) : (S.head_ -1);
        if constexpr(std::is_same_v<M,relax>){
            if(S.queue_[new_head].state_.load(std::memory_order_acquire) != synchro_queue<T,relax>::Empty)
                return -1;
            S.queue_[new_head].state_.store(synchro_queue<T,relax>::Busy, std::memory_order_relaxed);
        }
        S.head_ = new_head;                         
        return new_head;
    };

    template<typename T,typename M>
    int pop_f_indx(synchro_queue<T,M> & S){
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait>){
            if(S.empty_queue_ ){
                return -1;
            }
        }
        int h = S.head_; 
        if constexpr(std::is_same_v<M,relax>){
            if(S.queue_[h].state_.load(std::memory_order_acquire) != synchro_queue<T,relax>::Full)
                return -1;
            S.queue_[h].state_.store(synchro_queue<T,relax>::Busy, std::memory_order_relaxed);
        }
        S.head_ = (S.head_ == S.size_-1)? (0):(S.head_+1);
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait>){
            if(S.head_==S.tail_) {S.empty_queue_ = true;}
            S.queue_[h].count_pop_ ++;
        }   
        return h;
    };

    // helper function to push/pop
    template<typename T,typename M> 
    void push_fb_push(typename synchro_queue<T,M>::elem & E, T& new_value){ 
        E.v_ = std::move(new_value);
        if constexpr(std::is_same_v<M,relax>){
            E.state_.store(synchro_queue<T,relax>::Full, std::memory_order_release); 
        }
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait>){
            E.state_ = false; //(false == Full)
        }
    };

    template<typename T,typename M> 
    T pop_fb_pop(typename synchro_queue<T,M>::elem & E){
        T ret = std::move(E.v_.value());
        E.v_ = std::nullopt;
        if constexpr(std::is_same_v<M,relax>){
            E.state_.store(synchro_queue<T,relax>::Empty, std::memory_order_release);
        }
        if constexpr(std::is_same_v<M,hold_nowait> || std::is_same_v<M,hold_wait>){
            E.state_ = true;
            E.count_pop_ --;
        }
        return ret; 
    };
}

#endif