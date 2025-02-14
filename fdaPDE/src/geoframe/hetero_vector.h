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

#ifndef __FDAPDE_HETERO_VECTOR_H__
#define __FDAPDE_HETERO_VECTOR_H__

#include "header_check.h"

namespace fdapde {
namespace internals {

// a convinient heterogeneous structure for populating a plain_data_layer
struct hetero_data_vector {
   private:
    using index_t = int;
    template <template <typename...> typename T, typename U> struct strip_tuple_into;
    template <template <typename...> typename T, typename... Us>
    struct strip_tuple_into<T, std::tuple<Us...>> : std::type_identity<T<Us...>> { };
    // storage type definition
    using types = std::tuple<double, float, std::int64_t, std::int32_t, bool, std::string>;
    template <typename... Ts> using data_map_ = std::tuple<std::vector<std::pair<std::string, std::vector<Ts>>>...>;
    using storage_t = typename strip_tuple_into<data_map_, types>::type;
    template <typename T>
    static constexpr bool is_type_supported_v = internals::has_type<std::decay_t<T>, types>::value;
    template <typename T> struct mapped_type {
        using type = std::decay_t<decltype([](T) {
            if constexpr (internals::is_integer_v<T> && !std::is_same_v<T, std::int64_t>) {
                return std::int32_t();   // map integral, not 64 bit and not boolean types, to 32 bit int
            } else {
                return T();   // do not map other types
            }
        }(T()))>;
    };
    template <typename T> using mapped_type_t = typename mapped_type<T>::type;
    template <typename T>
    struct is_valid_pair {
      using colname_type = std::tuple_element_t<0, T>;
      using value_type   = mapped_type_t<std::tuple_element_t<1, T>>;
      static constexpr bool value =
        (std::is_same_v<colname_type, std::string> || std::is_same_v<colname_type, const char*>) &&
        is_type_supported_v<value_type>;
    };
    template <typename T> static constexpr bool is_valid_pair_v = is_valid_pair<T>::value;
    // internal getters based on type
    template <typename T, typename U> auto& fetch_(U& u) { return std::get<index_of<T, types>::value>(u); }
    template <typename T, typename U> const auto& fetch_(const U& u) const {
        return std::get<index_of<T, types>::value>(u);
    }
    storage_t data_;
    int size_ = 0;
    int n_cols_ = 0;
   public:
    hetero_data_vector() = default;
    hetero_data_vector(const std::vector<std::pair<std::string, dtype>>& column_descriptors) : size_(0), n_cols_(0) {
        for (const auto& col_descriptor : column_descriptors) {
            const auto& [colname, type_id] = col_descriptor;
            internals::dispatch_to_dtype(
              type_id,
              [&]<typename T>(hetero_data_vector& dst) mutable {
                  dst.get<T>().push_back(std::make_pair(colname, std::vector<T> {}));
              },
              *this);
            n_cols_++;
        }
    }
    // induce dtype from actual static types
    template <typename... Ts>
        requires(
          (std::contiguous_iterator<typename Ts::iterator> && is_type_supported_v<typename Ts::value_type>) && ...)
    hetero_data_vector(const std::vector<std::string>& colnames, const Ts&... data) {
        fdapde_assert(colnames.size() == sizeof...(Ts));
        internals::for_each_index_and_args<sizeof...(Ts)>(
          [&, this]<int Ns, typename T>([[maybe_unused]] const T& t) {
              internals::dispatch_to_dtype(
                internals::dtype_from_static_type<T>(),
                [&]<typename T_>(hetero_data_vector& dst) mutable {
                    dst.get<T_>().push_back(std::make_pair(colnames[Ns], std::vector<T_> {}));
                },
                *this);
          },
          data...);
        n_cols_ = colnames.size();
    }
    // observers
    template <typename T> auto& get() { return fetch_<T>(data_); }
    template <typename T> const auto& get() const { return fetch_<T>(data_); }
    int size() const { return size_; }
    // modifiers
    // template <typename DataLayer> void push_back(const plain_row_view<DataLayer>& row) {
    //     for (const auto& f : row.field_descriptors()) {
    //         internals::dispatch_to_dtype(
    //           f.type_id,
    //           [&]<typename T>(hetero_data_vector& dst) mutable {
    //               auto& typed_data = dst.template get<T>();
    //               for (auto& [colname, data] : typed_data) {
    //                   if (colname == f.colname) { data.push_back(row.template get<T>(f.colname)); }
    //               }
    //           },
    //           *this);
    //     }
    //     size_++;
    //     return;
    // }
    template <typename... DataT>
        requires(
          ([]() {
              if constexpr (internals::is_pair_v<std::decay_t<DataT>>) {
                  return is_valid_pair_v<std::decay_t<DataT>>;
              } else if constexpr (requires(DataT data, index_t i) {
                                       { data[i] };
                                   }) {
                  using subscript_t = std::decay_t<decltype(std::declval<DataT>()[std::declval<index_t>()])>;
                  if constexpr (internals::is_pair_v<subscript_t>) {   // check if subscripting DataT returns a tuple
                      return is_valid_pair_v<subscript_t>;
                  } else {
                      return false;
                  }
              } else {
                  return false;
              }
          }()) &&
          ...)
    void push_back(DataT&&... data) {
        int cols_ = 0;
        auto push_column = [&, this]<typename T>(const std::string& src_colname, const T& src_data) {
            auto& typed_data = get<std::tuple_element_t<1, T>>();
            bool inserted = false;
            for (auto& [dst_colname, dst_data] : typed_data) {   // search insertion column
                if (src_colname == dst_colname) {
                    dst_data.push_back(src_data);
                    inserted = true;
		    cols_++;
		    break;
                }
            }
            if (!inserted) { throw std::runtime_error("GeoFrame: column not found."); }
        };
        // perform insertion
        internals::for_each_index_and_args<sizeof...(DataT)>(
          [&]<int Ns, typename T>(const T& t) {
              if constexpr (internals::is_pair_v<T>) {
                  push_column(std::get<0>(t), std::get<1>(t));
              } else { // vector of pairs
                  for (const auto& [src_colname, src_data] : t) { push_column(src_colname, src_data); }
              }
          },
          data...);
        if (cols_ != n_cols_) { throw std::runtime_error("GeoFrame: invalid number of columns."); }
        size_++;
        return;
    }
    template <typename F_>
        requires(std::is_invocable_v<F_, int>)
    void push_back(F_&& f) {   // push for each column the result of F_<T> for each T in types
        std::apply(
          [&, this](auto&... ts) {
              (
                [&]() {
                    if (!ts.empty()) {
                        for (auto& [colname, vec] : ts) {
                            using value_type = typename std::decay_t<decltype(vec)>::value_type;
                            vec.push_back(f.template operator()<value_type>(value_type {}));
                        }
                    }
                }(),
                ...);
          },
          data_);
        size_++;
        return;
    }
};

}   // namespace internals
}   // namespace fdapde

#endif   // __FDAPDE_HETERO_VECTOR_H__
