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

#ifndef __FDAPDE_GRID_SEARCH_H__
#define __FDAPDE_GRID_SEARCH_H__

#include "header_check.h"

namespace fdapde {

template <int N> class GridSearch {
   private:
    //using vector_t = std::conditional_t<N == Dynamic, Eigen::Matrix<double, Dynamic, 1>, Eigen::Matrix<double, N, 1>>;
    using vector_t = Eigen::Matrix<double, 1, 2>;
    using grid_t = MdMap<const double, MdExtents<Dynamic, Dynamic>>;

    vector_t optimum_;
    double value_;                 // objective value at optimum
    std::vector<double> values_;   // explored objective values during optimization
    int size_;
   public:
    static constexpr bool gradient_free = true;
    static constexpr int static_input_size = N;
    vector_t x_curr;
    double obj_curr;
    // constructor
    GridSearch() : size_(N) { fdapde_static_assert(N != Dynamic, THIS_METHOD_IS_FOR_STATIC_SIZED_GRID_SEARCH_ONLY); }
    GridSearch(int size) : size_(N == Dynamic ? size : N) { fdapde_assert(N == Dynamic || size == N); }
    GridSearch(const GridSearch& other) : size_(other.size_) { }
    GridSearch& operator=(const GridSearch& other) {
        size_ = other.size_;
        return *this;
    }
    template <typename ObjectiveT, typename GridT, typename... Callbacks>
        requires((internals::is_vector_like_v<GridT> || internals::is_matrix_like_v<GridT>))
    vector_t optimize(ObjectiveT&& objective, const GridT& grid, Callbacks&&... callbacks) {
        fdapde_static_assert(
          std::is_same<decltype(std::declval<ObjectiveT>().operator()(vector_t())) FDAPDE_COMMA double>::value,
          INVALID_CALL_TO_OPTIMIZE__OBJECTIVE_FUNCTOR_NOT_CALLABLE_AT_VECTOR_TYPE);
        constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
        std::tuple<Callbacks...> callbacks_ {callbacks...};
        grid_t grid_;
        value_ = std::numeric_limits<double>::max();
        if constexpr (internals::is_vector_like_v<GridT>) {
            fdapde_assert(grid.size() % size_ == 0);
            grid_ = grid_t(grid.data(), grid.size() / size_, size_);
        } else {
            fdapde_assert(grid.cols() == size_);
            grid_ = grid_t(grid.data(), grid.rows(), size_);
        }
        bool stop = false;   // asserted true in case of forced stop
        grid_.row(0).assign_to(x_curr);
        obj_curr = objective(x_curr);
        stop |= internals::exec_eval_hooks(*this, objective, callbacks_);
        values_.clear();
        values_.push_back(obj_curr);
        if (obj_curr < value_) {
            value_ = obj_curr;
            optimum_ = x_curr;
        }
        // optimize field over supplied grid
        for (std::size_t i = 1; i < grid_.rows() && !stop; ++i) {
            grid_.row(i).assign_to(x_curr);
            obj_curr = objective(x_curr);
            stop |= internals::exec_eval_hooks(*this, objective, callbacks_);
            values_.push_back(obj_curr);
            // update minimum if better optimum found
            if (obj_curr < value_) {
                value_ = obj_curr;
                optimum_ = x_curr;
            }
            stop |= internals::exec_stop_if(*this, objective);
        }
        return optimum_;
    }


    template <typename ObjectiveT, typename GridT, typename... Callbacks>
        requires((internals::is_vector_like_v<GridT> || internals::is_matrix_like_v<GridT>))
    vector_t optimize(ObjectiveT&& objective, const GridT& grid, execution::execution_parallel, Callbacks&&... callbacks) { //execution prima di callack perche senno overload amiguo (perchè callback variadic)
        fdapde_static_assert(
          std::is_same<decltype(std::declval<ObjectiveT>().operator()(vector_t())) FDAPDE_COMMA double>::value,
          INVALID_CALL_TO_OPTIMIZE__OBJECTIVE_FUNCTOR_NOT_CALLABLE_AT_VECTOR_TYPE);

        //creazione threadpool
        fdapde::Threadpool<fdapde::steal::random> Tp(grid.size() / size_); //n_worker = hardwer_thread di defaul, size queue di worker = numero poit da valutare (male che va 1 worker e u jo per ogni iterazioe stao i queue)

        constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
        std::tuple<Callbacks...> callbacks_ {callbacks...};
        grid_t grid_;
        value_ = std::numeric_limits<double>::max();
        if constexpr (internals::is_vector_like_v<GridT>) {
            fdapde_assert(grid.size() % size_ == 0);
            grid_ = grid_t(grid.data(), grid.size() / size_, size_);
        } else {
            fdapde_assert(grid.cols() == size_);
            grid_ = grid_t(grid.data(), grid.rows(), size_);
        }
        bool stop = false;   // asserted true in case of forced stop
        
        values_.clear();       
        // optimize field over supplied grid
        int granularity = 1; //per ora hardcode, poi versioe con gran "optimal" di defaul ( tipo grid_.rows()/Tp.get_n_worker()/10)
        
        // variabile locale per ogni thread (evita dover creare una x_curr_local per ogni iterazione)
        thread_local vector_t x_curr_local_thread;
        
        //fix size di vector values_ cosi da modifica threadsafe con accesso tramite indice [i]
        values_.resize(grid_.rows());

        //TODO: logica di stop anticipato da capire, se possibile aggiungere in metodo tp.paralle_for_reduce il passaggio di una ref a bool stop cosi da stoppare il job e non fare iterazioni. 
        //      per ora no stop anticipato, si finisce quando scorre tutta griglia
        // problema: non si puo usare i metodi in callbacks.h perchè *this (e quidi x_curr_, ...) non sono modificati nel mentre, lo fossero bisognerebbe renderli threadsafe ma poi modifica sequenziale
        std::pair<double,int> min_argmin = Tp.parallel_for_reduce_min(0,grid_.rows(), [&, this](int i) -> double {
            grid_.row(i).assign_to(x_curr_local_thread);
            double obj_of_iteration = objective(x_curr_local_thread);
            std::cout<<std::this_thread::get_id()<<" da x_curr: "<<x_curr_local_thread<<" da value: "<<obj_of_iteration<<std::endl;
            //stop |= internals::exec_eval_hooks(*this, objective, callbacks_); 
            values_[i]= obj_of_iteration;
            return obj_of_iteration;
            
            //stop |= internals::exec_stop_if(*this, objective); 

        },granularity);
        grid_.row(min_argmin.second).assign_to(optimum_);
        value_ = min_argmin.first;
        return optimum_;
    }



    // observers
    const vector_t& optimum() const { return optimum_; }
    double value() const { return value_; }
    const std::vector<double>& values() const { return values_; }
};

}   // namespace fdapde

#endif   // __FDAPDE_GRID_SEARCH_H__
