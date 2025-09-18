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
    using vector_t = std::conditional_t<N == Dynamic, Eigen::Matrix<double, Dynamic, 1>, Eigen::Matrix<double, N, 1>>;

    vector_t optimum_;
    double value_;                 // objective value at optimum
    std::vector<double> values_;   // explored objective values during optimization //? in parallel ha senso tenerlo?
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
        using layout_policy = decltype([]() {
            if constexpr (internals::is_eigen_dense_xpr_v<GridT>) {
                return std::conditional_t<GridT::IsRowMajor, internals::layout_right, internals::layout_left> {};
            } else {
                return internals::layout_right {};
            }
        }());
        using grid_t = MdMap<const double, MdExtents<Dynamic, Dynamic>, layout_policy>;
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
        grid_.row(0).assign_to(x_curr.transpose());
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
            grid_.row(i).assign_to(x_curr.transpose());
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

    // versione che usa parallel_for_reduce_min(). threadpool in input
    template <typename ObjectiveT, typename GridT>
        requires((internals::is_vector_like_v<GridT> || internals::is_matrix_like_v<GridT>))
    vector_t optimize(ObjectiveT&& objective, const GridT& grid, execution::execution_parallel,fdapde::Threadpool<fdapde::steal::random>& Tp, int job_per_worker=1) { //int job_per_worker per ora in input per semplicita in test
        fdapde_static_assert(
          std::is_same<decltype(std::declval<ObjectiveT>().operator()(vector_t())) FDAPDE_COMMA double>::value,
          INVALID_CALL_TO_OPTIMIZE__OBJECTIVE_FUNCTOR_NOT_CALLABLE_AT_VECTOR_TYPE);
        using layout_policy = decltype([]() {
            if constexpr (internals::is_eigen_dense_xpr_v<GridT>) {
                return std::conditional_t<GridT::IsRowMajor, internals::layout_right, internals::layout_left> {};
            } else {
                return internals::layout_right {};
            }
        }());
        using grid_t = MdMap<const double, MdExtents<Dynamic, Dynamic>, layout_policy>;
        constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
	
        grid_t grid_;
        value_ = std::numeric_limits<double>::max();
        if constexpr (internals::is_vector_like_v<GridT>) {
            fdapde_assert(grid.size() % size_ == 0);
            grid_ = grid_t(grid.data(), grid.size() / size_, size_);
        } else {
            fdapde_assert(grid.cols() == size_);
            grid_ = grid_t(grid.data(), grid.rows(), size_);
        }      
        
        int n_threads = Tp.get_n_worker();
        // variabile locale per ogni thread (evita dover creare una x_curr_local, obj_curr_local per ogni iterazione)
        thread_local vector_t x_curr_local_thread;
        
        int granularity = std::max(int(grid_.rows()/(n_threads*job_per_worker)),1); //per ora job_per_worker input per test piu semplici, poi valore scelto

        //TODO: logica di stop anticipato da capire, se possibile aggiungere in metodo tp.paralle_for_reduce il passaggio di una ref a bool stop cosi da stoppare il job e non fare iterazioni. 
        //      per ora no stop anticipato, si finisce quando scorre tutta griglia
        // problema: non si puo usare i metodi in callbacks.h perchè *this (e quidi x_curr_, ...) non sono modificati nel mentre, lo fossero bisognerebbe renderli threadsafe ma poi modifica sequenziale
        std::pair<double,int> min_argmin = Tp.parallel_for_reduce_min(0,grid_.rows(), [&, this](int i) -> double {
            grid_.row(i).assign_to(x_curr_local_thread.transpose());
            return objective(x_curr_local_thread);
            
        },granularity);

        grid_.row(min_argmin.second).assign_to(optimum_.transpose());
        value_ = min_argmin.first;

        return optimum_;
    }


    // overload con Threadpool costruità non passata in input.
    template <typename ObjectiveT, typename GridT>
        requires((internals::is_vector_like_v<GridT> || internals::is_matrix_like_v<GridT>))
    vector_t optimize(ObjectiveT&& objective, const GridT& grid, execution::execution_parallel, int n_threads = std::thread::hardware_concurrency(),int job_per_worker = 1) {        
        //creazione threadpool
        fdapde::Threadpool<fdapde::steal::random> Tp(1024, n_threads); 
        return optimize(std::forward<ObjectiveT>(objective),grid,execution::par,Tp,job_per_worker);
    }

    
    //versione con parallel_for e reduce "fatto da qui", Threadpool in input
    
    template <typename ObjectiveT, typename GridT>
        requires((internals::is_vector_like_v<GridT> || internals::is_matrix_like_v<GridT>))
    vector_t optimize2(ObjectiveT&& objective, const GridT& grid, execution::execution_parallel,fdapde::Threadpool<fdapde::steal::random>& Tp, int job_per_worker = 1) { // per ora int job_per_worker in input perche piu comodo fare i test poi sostituire valore scelto
        fdapde_static_assert(
          std::is_same<decltype(std::declval<ObjectiveT>().operator()(vector_t())) FDAPDE_COMMA double>::value,
          INVALID_CALL_TO_OPTIMIZE__OBJECTIVE_FUNCTOR_NOT_CALLABLE_AT_VECTOR_TYPE);
        using layout_policy = decltype([]() {
            if constexpr (internals::is_eigen_dense_xpr_v<GridT>) {
                return std::conditional_t<GridT::IsRowMajor, internals::layout_right, internals::layout_left> {};
            } else {
                return internals::layout_right {};
            }
        }());
        using grid_t = MdMap<const double, MdExtents<Dynamic, Dynamic>, layout_policy>;
        // per evitare false sharing e rendere piu veloce (il mio computer ha 64 byte in cacheline credo tutti ormai, nel caso da verificare su linux con $ cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size  )
        struct alignas(64) AlignedPair {
            double first = std::numeric_limits<double>::max();// inizializzato a massimo erch ein problema cerchiamo minimo
            vector_t second;
        };

        constexpr double NaN = std::numeric_limits<double>::quiet_NaN();

        grid_t grid_;
        value_ = std::numeric_limits<double>::max();
        if constexpr (internals::is_vector_like_v<GridT>) {
            fdapde_assert(grid.size() % size_ == 0);
            grid_ = grid_t(grid.data(), grid.size() / size_, size_);
        } else {
            fdapde_assert(grid.cols() == size_);
            grid_ = grid_t(grid.data(), grid.rows(), size_);
        }
        
        int n_threads = Tp.get_n_worker();
        // vettore di (value,optimum) per ogni worker, alla fine ci saranno min,argmin trovati da ogni worker e poi reduce di questo vettore darà min argmin finali
        std::vector<AlignedPair> value_optimum_workers(n_threads); //inizializzato con n_thread elementi vuoti cosi da non riallocare ed essere threadsafe
        

        // variabile locale per ogni thread (evita dover creare una x_curr_local per ogni iterazione)
        thread_local vector_t x_curr_local_thread;
        thread_local double obj_curr_local_thread;

        int granularity = std::max(int(grid_.rows()/(n_threads*job_per_worker)),1); //OSS: granularity massima (cioè t.c. 1 job per worker fa si che massimo cache friendly durante scorrimento di grid_)
        
        Tp.parallel_for(0,grid_.rows(), [&, this](int i){ //tutto tramite ref per occupare meno memoria ma piu lento
            int index_worker = Tp.get_index_worker_from_thread();
            grid_.row(i).assign_to(x_curr_local_thread.transpose()); 
            double obj_curr_local_thread = objective(x_curr_local_thread);
            // update minimum of worker if better optimum found
            if (obj_curr_local_thread < value_optimum_workers[index_worker].first) {
                value_optimum_workers[index_worker].first = obj_curr_local_thread;
                value_optimum_workers[index_worker].second = x_curr_local_thread;
            }

        },granularity);

        // reduce di value_optimum_workers[], minimo in value_ argmin in optimum_
        value_ = value_optimum_workers[0].first;
        optimum_ = value_optimum_workers[0].second;
        for (int i = 1; i<n_threads; i++){
            if(value_optimum_workers[i].first < value_){
                value_ = value_optimum_workers[i].first;
                optimum_ = value_optimum_workers[i].second;
            }
        }

        return optimum_;
    }


    template <typename ObjectiveT, typename GridT>
        requires((internals::is_vector_like_v<GridT> || internals::is_matrix_like_v<GridT>))
    vector_t optimize2(ObjectiveT&& objective, const GridT& grid, execution::execution_parallel,int n_threads = std::thread::hardware_concurrency(), int job_per_worker = 1) { // per ora int job_per_worker in input perche piu comodo fare i test poi sostituire valore scelto

        //creazione threadpool
        fdapde::Threadpool<fdapde::steal::random> Tp(1024, n_threads); //n_worker = hardwer_thread di defaul, size queue 1024 hardcoded tanto visto job per worker da 1 a 10

        return optimize2(std::forward<ObjectiveT>(objective),grid,execution::par,Tp,job_per_worker);
    }

    // observers
    const vector_t& optimum() const { return optimum_; }
    double value() const { return value_; }
    const std::vector<double>& values() const { return values_; }
};

}   // namespace fdapde

#endif   // __FDAPDE_GRID_SEARCH_H__
