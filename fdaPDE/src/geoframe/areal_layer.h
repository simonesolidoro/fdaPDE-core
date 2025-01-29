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

#ifndef __FDAPDE_AREAL_LAYER_H__
#define __FDAPDE_AREAL_LAYER_H__

#include "header_check.h"

namespace fdapde {
namespace internals {

  // TODO: count number of points in a region
  
template <typename Triangulation_> struct areal_layer {
    using Triangulation = typename std::remove_pointer_t<std::decay_t<Triangulation_>>;
    using layer_category = areal_layer_tag;
     static constexpr int local_dim = Triangulation::local_dim;
     static constexpr int embed_dim = Triangulation::embed_dim;

     areal_layer() : triangulation_(nullptr), regions_() { }
     template <typename SubregionsType>
         requires(std::is_same_v<SubregionsType, std::vector<MultiPolygon<local_dim, embed_dim>>>)
     areal_layer(Triangulation_ triangulation, const /*std::shared_ptr<*/SubregionsType/*>*/& regions) noexcept :
       triangulation_(triangulation)/*, regions_(regions)*/ { }
      // observers
      size_t n_regions() const { return regions_->size(); }
     //  std::vector<std::string> colnames() const { return data_.colnames(); }
     //  const storage_t& data_layer() const { return data_; }
     //  template <typename T> const auto& data() const { return data_.template data<T>(); }
     //  template <typename T> auto& data() { return data_.template data<T>(); }
      // geometry
      const MultiPolygon<local_dim, embed_dim>& geometry(int i) const { return regions_->operator[](i); }
      Triangulation& triangulation() { return *triangulation_; }
      const Triangulation& triangulation() const { return *triangulation_; }

      // computes measures of subdomains
      std::vector<double> measures() const {
          std::vector<double> m_(regions_->size(), 0);
	for(int i = 0; i < regions_->size(); ++i) { m_[i] = regions_->operator[](i).measure(); }
          return m_;
      }
      // computes matrix [M]_{ij} : [M]_{ij} == 1 \iff cell j is inside region i, 0 otherwise
      BinaryMatrix<Dynamic, Dynamic> incidence_matrix() const {
          BinaryMatrix<Dynamic, Dynamic> m(regions_->size(), triangulation().n_cells());
          // for each cell, check in which region its barycenter lies
          for (auto it = triangulation().cells_begin(); it != triangulation().cells_end(); ++it) {
              Eigen::Matrix<double, embed_dim, 1> barycenter = it->barycenter();
              for (int i = 0, n = regions_->size(); i < n; ++i) {
                  if (regions_->operator[](i).contains(barycenter)) { m.set(i, it->id()); }
              }
          }
          return m;
      }
      // random sample points in areal layer
      Eigen::Matrix<double, Dynamic, Dynamic> sample(int n_samples, int seed = random_seed) const {
          if (n_regions() == 1) { return geometry(0).sample(n_samples, seed); }
          // set up random number generation
          int seed_ = (seed == random_seed) ? std::random_device()() : seed;
          std::mt19937 rng(seed_);
          // probability of random sampling a region equals (measure of region)/(measure of layer)
          std::vector<double> weights = measures();
          std::discrete_distribution<int> rand_region(weights.begin(), weights.end());
          Eigen::Matrix<double, Dynamic, Dynamic> coords(n_samples, embed_dim);
          for (int i = 0; i < n_samples; ++i) {
              // generate random point in randomly selected region
              coords.row(i) = geometry(rand_region(rng)).sample(1, seed + i).row(0);
          }
          return coords;
      }

     //  struct areal_row_view : public plain_row_view<storage_t> {
     //      using Base = plain_row_view<storage_t>;

     //      areal_row_view() noexcept = default;
     //      areal_row_view(areal_layer<GeoFrame>* data, int row) :
     //          Base(std::addressof(data->data()), row), data_(data), row_(row) { }
     //      // observers
     //      const MultiPolygon<local_dim, embed_dim>& geometry() const { return data_->geometry(row_); }
     //     private:
     //      areal_layer<GeoFrame>* data_;
     //      index_t row_;
     //  };
     //  using row_view = areal_row_view;
     //  using const_row_view = areal_row_view;

     //  // row access
     //  row_view row(size_t row) {
     //      if (row >= n_regions()) { throw std::runtime_error("GeoFrame: out of bound access."); }
     //      return areal_row_view(this, row);
     //  }
     //  const_row_view row(size_t row) const {
     //      if (row >= n_regions()) { throw std::runtime_error("GeoFrame: out of bound access."); }
     //      return areal_row_view(this, row);
     //  }
     //  // row filtering operations
     //  template <typename Iterator>
     //      requires(internals::is_integer_v<typename Iterator::value_type>)
     //  plain_row_filter<areal_layer<GeoFrame>> operator()(Iterator begin, Iterator end) {
     //      return plain_row_filter<areal_layer<GeoFrame>>(this, begin, end);
     //  }
     //  template <typename T>
     //      requires(std::is_convertible_v<T, index_t>)
     //  plain_row_filter<areal_layer<GeoFrame>> operator()(const std::initializer_list<T>& idxs) {
     //      return plain_row_filter<areal_layer<GeoFrame>>(this, idxs.begin(), idxs.end());
     //  }
     //  template <typename Filter>
     //      requires(requires(Filter f, index_t i) {
     //          { f(i) } -> std::convertible_to<bool>;
     //      })
     //  plain_row_filter<areal_layer<GeoFrame>> operator()(Filter&& f) {
     //      return plain_row_filter<areal_layer<GeoFrame>>(this, std::forward<Filter>(f));
     //  }
     //  template <typename LogicalVec>
     //      requires(requires(LogicalVec vec, index_t i) {
     //          { vec[i] } -> std::convertible_to<bool>;
     //      })
     //  plain_row_filter<areal_layer<GeoFrame>> operator()(LogicalVec&& logical_vec) {
     //      return plain_row_filter<areal_layer<GeoFrame>>(this, logical_vec);
     //  }
      private:
     // data members
     Triangulation_* triangulation_;
     std::shared_ptr<std::vector<MultiPolygon<local_dim, embed_dim>>> regions_;
    //  storage_t data_;
};
  
}   // namespace internals
}   // namespace fdapde

#endif // __FDAPDE_AREAL_LAYER_H__
