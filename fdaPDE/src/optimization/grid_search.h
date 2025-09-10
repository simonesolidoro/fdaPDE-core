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

    // versione che usa parallel_for_reduce_min()
    template <typename ObjectiveT, typename GridT, typename... Callbacks>
        requires((internals::is_vector_like_v<GridT> || internals::is_matrix_like_v<GridT>))
    vector_t optimize(ObjectiveT&& objective, const GridT& grid, execution::execution_parallel,int job_per_worker, int n_threads = std::thread::hardware_concurrency(), Callbacks&&... callbacks) {
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
        values_.clear();       
        // optimize field over supplied grid
        
        // variabile locale per ogni thread (evita dover creare una x_curr_local per ogni iterazione)
        thread_local vector_t x_curr_local_thread;
        
        //fix size di vector values_ cosi da modifica threadsafe con accesso tramite indice [i]
        values_.resize(grid_.rows());

        //creazione threadpool
        fdapde::Threadpool<fdapde::steal::random> Tp(grid.size() / size_, n_threads); //n_worker = hardwer_thread di defaul, size queue di worker = numero poit da valutare (male che va 1 worker e u jo per ogni iterazioe stao i queue)

        int granularity = std::max(int(grid_.rows()/(n_threads*job_per_worker)),1); //per ora job_per_worker input per test piu semplici, poi valore scelto

        //TODO: logica di stop anticipato da capire, se possibile aggiungere in metodo tp.paralle_for_reduce il passaggio di una ref a bool stop cosi da stoppare il job e non fare iterazioni. 
        //      per ora no stop anticipato, si finisce quando scorre tutta griglia
        // problema: non si puo usare i metodi in callbacks.h perchè *this (e quidi x_curr_, ...) non sono modificati nel mentre, lo fossero bisognerebbe renderli threadsafe ma poi modifica sequenziale
        std::pair<double,int> min_argmin = Tp.parallel_for_reduce_min(0,grid_.rows(), [&, this](int i) -> double {
            grid_.row(i).assign_to(x_curr_local_thread.transpose());
            double obj_of_iteration = objective(x_curr_local_thread);
            //std::cout<<std::this_thread::get_id()<<" da x_curr: "<<x_curr_local_thread<<" da value: "<<obj_of_iteration<<std::endl;
            values_[i]= obj_of_iteration;
            return obj_of_iteration;
            
            //stop |= internals::exec_stop_if(*this, objective); 

        },granularity);

        grid_.row(min_argmin.second).assign_to(optimum_.transpose());
        value_ = min_argmin.first;

        return optimum_;
    }

    //versione con parallel_for e reduce "fatto da qui"

    // per evitare false sharing e rendere piu veloce (il mio computer ha 64 byte in cacheline credo tutti ormai, nel caso da verificare su linux con $ cat /sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size  )
    // TODO: dove mettere definizione di struct magari in multithreading/.  TODO: first e second template
    struct alignas(64) AlignedPair {
        double first = std::numeric_limits<double>::max();// inizializzato a massimo erch ein problema cerchiamo minimo
        vector_t second;
    };

    template <typename ObjectiveT, typename GridT, typename... Callbacks>
        requires((internals::is_vector_like_v<GridT> || internals::is_matrix_like_v<GridT>))
    vector_t optimize2(ObjectiveT&& objective, const GridT& grid, execution::execution_parallel,int job_per_worker,int n_threads = std::thread::hardware_concurrency(), Callbacks&&... callbacks) { // per ora int job_per_worker in input perche piu comodo fare i test poi sostituire valore scelto
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
        
               
        // optimize field over supplied grid
        
        // variabile locale per ogni thread (evita dover creare una x_curr_local per ogni iterazione)
        thread_local vector_t x_curr_local_thread;
        // vettore di (value,optimum) per ogni worker, alla fine ci saranno min,argmin trovati da ogni worker e poi reduce di questo vettore darà min argmin finali
        std::vector<AlignedPair> value_optimum_workers(n_threads); //inizializzato con n_thread elementi vuoti cosi da non riallocare ed essere threadsafe
        
        //fix size di vector values_ cosi da modifica threadsafe con accesso tramite indice [i]
        //values_.resize(grid_.rows()); tolto perche no stop anticipato e poi era costoso da tenere in parallelo (ogni worker calcola il suo e poi unire e riordinare costosissimo)
        //values_.clear();

        //creazione threadpool
        fdapde::Threadpool<fdapde::steal::random> Tp(grid.size() / size_, n_threads); //n_worker = hardwer_thread di defaul, size queue di worker = numero poit da valutare (male che va 1 worker e u jo per ogni iterazioe stao i queue)

        int granularity = std::max(int(grid_.rows()/(n_threads*job_per_worker)),1); //per ora hardcode, poi versioe con gran "optimal" di defaul ( tipo grid_.rows()/Tp.get_n_worker()/10)
        
        Tp.parallel_for(0,grid_.rows(), [&, this](int i){ //tutto tramite ref per occupare meno memoria ma piu lento
        //Tp.parallel_for(0,grid_.rows(), [&,grid_, this](int i){ //copiare la griglia per ogni job è costoso e in piu limita il numero di job non va bene
            int index_worker = Tp.get_index_worker_from_thread();
            grid_.row(i).assign_to(x_curr_local_thread.transpose());
            double obj_of_iteration = objective(x_curr_local_thread);
            //std::cout<<std::this_thread::get_id()<<" da x_curr: "<<x_curr_local_thread<<" da value: "<<obj_of_iteration<<std::endl;
            //values_[i]= obj_of_iteration;
            // update minimum of worker if better optimum found
            if (obj_of_iteration < value_optimum_workers[index_worker].first) {
                value_optimum_workers[index_worker].first = obj_of_iteration;
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

    //idea:ogni worker ha un job cosi possibile salvare direttamente le porzioni di griglia di sottogriglie thread local, cosi ogni worker ha sua e non deve copiare o accedere tramite ref 
    template <typename ObjectiveT, typename GridT, typename... Callbacks>
        requires((internals::is_vector_like_v<GridT> || internals::is_matrix_like_v<GridT>))
    vector_t optimize3(ObjectiveT&& objective, const GridT& grid, execution::execution_parallel,int n_threads = std::thread::hardware_concurrency(), Callbacks&&... callbacks) {
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
        value_ = std::numeric_limits<double>::max();
    	//creazione threadpool
        fdapde::Threadpool<fdapde::steal::random> Tp(grid.size() / size_, n_threads); //n_worker = hardwer_thread di defaul, size queue di worker = numero poit da valutare (male che va 1 worker e u jo per ogni iterazioe stao i queue)
        int n_worker = Tp.get_n_worker(); //per poter usare anche poi quando n_threads non passato

        // vettore di (value,optimum) per ogni worker, alla fine ci saranno min,argmin trovati da ogni worker e poi reduce di questo vettore darà min argmin finali
        std::vector<AlignedPair> value_optimum_workers(n_threads); //inizializzato con n_thread elementi vuoti cosi da non riallocare ed essere threadsafe
        
        //fix size di vector values_ cosi da modifica threadsafe con accesso tramite indice [i]
        //TODO: vettore di values_ cosi ognuno ha il suo (da fare aligned) e poi unire dopo (unire non ha senso perche si perde ordine decrescente, andrebbero riordinati ma sara costos, da capire se necessario values_)
        //values_.resize(grid_.rows());


        //paraleliziamo danto dunnjob per worker
        Tp.parallel_for(0,n_worker, [&, this](int i){ //tutto tramite ref per occupare meno memoria ma piu lento
            //copia di proprio pezzo di griglia
            grid_t grid_t_local;
            Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> grid_local;
            //variabili curr locali per ogni worker (sarebbero per ogni job ma qui facciamo un job per worker)
            vector_t x_curr_local;
            double obj_curr_local;
            int local_row = 0;
            int row_per_worker = 0; //= a local_row per tutti i worker tranne l'ultimo che avrà inn piu le row restanti
            if constexpr (internals::is_vector_like_v<GridT>) {
                //per il momento solo matrix like che usato quello in nostro esempio, poi faremo anche qui
                fdapde_assert(grid.size() % size_ == 0);
                grid_local = grid_t(grid.data(), (grid.size() / size_), size_);
            } else {
                fdapde_assert(grid.cols() == size_);
                //numero righe per worker i
                local_row = grid.rows()/n_worker; //numero di righe per worker
                row_per_worker = local_row;
                if(i == n_worker-1){//se ultimo worker la griglia avra righe per worker + righe date da resto di divisione
                    local_row += (grid.rows() % n_worker);
                }
                //griglia local di ogni worker 
                grid_local.resize(local_row,2);
                for(int r = 0; r < local_row; r++){
                    for(int c = 0; c<size_; c++){
                        grid_local(r,c) = grid(r+i*row_per_worker,c);
                    }
                }
                //grid_t locale di ogni worker
                grid_t_local = grid_t(grid_local.data(), local_row, size_);
            }
            //std::cout<<grid_local<<std::endl; //per debug
            grid_t_local.row(0).assign_to(x_curr_local.transpose());
            obj_curr_local = objective(x_curr_local);
            if (obj_curr_local < value_optimum_workers[i].first) {
                value_optimum_workers[i].first = obj_curr_local;
                value_optimum_workers[i].second = x_curr_local;
            }
            // optimize field over supplied grid
            for (std::size_t j = 1; j < local_row ; ++j) {
                grid_t_local.row(j).assign_to(x_curr_local.transpose());
                obj_curr_local = objective(x_curr_local);
                // update minimum if better optimum found
                if (obj_curr_local < value_optimum_workers[i].first) {
                    value_optimum_workers[i].first = obj_curr_local;
                    value_optimum_workers[i].second = x_curr_local;
                }
            }
        });

        // reduce di value_optimum_workers[], minimo in value_ argmin in optimum
        value_ = value_optimum_workers[0].first;
        optimum_ = value_optimum_workers[0].second;
        std::cout<<"VALUE: "<<value_<<"  "<<value_optimum_workers[0].first<<"---";
        for (int i = 1; i<n_threads; i++){
            std::cout<<value_optimum_workers[i].first<<"---";
            if(value_optimum_workers[i].first < value_){
                value_ = value_optimum_workers[i].first;
                optimum_ = value_optimum_workers[i].second;
            }
        }

        return optimum_;
    }

    // observers
    const vector_t& optimum() const { return optimum_; }
    double value() const { return value_; }
    const std::vector<double>& values() const { return values_; }
};

}   // namespace fdapde

#endif   // __FDAPDE_GRID_SEARCH_H__
