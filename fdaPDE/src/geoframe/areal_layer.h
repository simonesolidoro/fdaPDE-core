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
    using binary_t = BinaryMatrix<Dynamic, Dynamic>;
    static constexpr int local_dim = Triangulation::local_dim;
    static constexpr int embed_dim = Triangulation::embed_dim;

    areal_layer() : triangulation_(nullptr), regions_(), n_regions_(0) { }
    template <typename GeoDescriptor>
        requires(is_vector_like_v<GeoDescriptor> &&
                 std::is_convertible_v<subscript_result_of_t<GeoDescriptor, int>, int>)
    areal_layer(Triangulation_* triangulation, const GeoDescriptor& regions) noexcept :
        triangulation_(triangulation), regions_(), n_regions_(0) {
        fdapde_assert(regions.size() == triangulation_->n_cells());
        // count number of regions
        std::unordered_set<int> unique_region_ids;
	regions_.resize(regions.size());
        for (int i = 0, n = regions.size(); i < n; ++i) {
            regions_[i] = regions[i];
            unique_region_ids.insert(regions[i]);
        }
        n_regions_ = unique_region_ids.size();
    }
    template <typename GeoDescriptor>
        requires(is_vector_like_v<GeoDescriptor> &&
                 std::is_same_v<
                   std::decay_t<subscript_result_of_t<GeoDescriptor, int>>, MultiPolygon<local_dim, embed_dim>>)
    areal_layer(Triangulation_* triangulation, const GeoDescriptor& regions) noexcept :
        triangulation_(triangulation), regions_(), n_regions_(0) {
        fdapde_assert(regions.size() == triangulation_->n_cells());
        // count number of regions
	n_regions_ = regions.size();
	regions_.resize(triangulation_->n_cells());
	int j = 0;
        for (auto it = triangulation_->cells_begin(); it != triangulation_->cells_end(); ++it) {
            for (int i = 0, n = regions.size(); i < n; ++i) {
                if (regions[i].contains(it->barycenter())) { regions_[it->id()] = i; }
            }
        }
    }

    // observers
    int rows() const { return n_regions_; }
    // geometry
    BinaryVector<Dynamic> operator[](int i) const {
        int n = triangulation_->n_cells();
        BinaryVector<Dynamic> vec(n);
        for (int j = 0; j < n; ++j) {
            if (regions_[j] == i) { vec.set(j); }
        }
        return vec;
    }
    Triangulation& triangulation() { return *triangulation_; }
    const Triangulation& triangulation() const { return *triangulation_; }
    // computes measures of subdomains
    std::vector<double> measures() const {
        std::vector<double> m_(n_regions_, 0);
        for (auto it = triangulation().cells_begin(); it != triangulation().cells_end(); ++it) {
            m_[regions_[it->id()]] += it->measure();
        }
        return m_;
    }
    // computes matrix [M]_{ij} : [M]_{ij} == 1 \iff cell j is inside region i, 0 otherwise
    BinaryMatrix<Dynamic, Dynamic> incidence_matrix() const {
        BinaryMatrix<Dynamic, Dynamic> m(n_regions_, triangulation().n_cells());
        for (int i = 0, n = triangulation_->n_cells(); i < n; ++i) { m.set(regions_[i], i); }
        return m;
    }
    // random sample points in areal layer
    // Eigen::Matrix<double, Dynamic, Dynamic> sample(int n_samples, int seed = random_seed) const {
    //     if (rows() == 1) { return geometry(0).sample(n_samples, seed); }
    //     // set up random number generation
    //     int seed_ = (seed == random_seed) ? std::random_device()() : seed;
    //     std::mt19937 rng(seed_);
    //     // probability of random sampling a region equals (measure of region)/(measure of layer)
    //     std::vector<double> weights = measures();
    //     std::discrete_distribution<int> rand_region(weights.begin(), weights.end());
    //     Eigen::Matrix<double, Dynamic, Dynamic> coords(n_samples, embed_dim);
    //     for (int i = 0; i < n_samples; ++i) {
    //         // generate random point in randomly selected region
    //         coords.row(i) = geometry(rand_region(rng)).sample(1, seed + i).row(0);
    //     }
    //     return coords;
    // }
   private:
    Triangulation* triangulation_;
    std::vector<int> regions_;
    int n_regions_;
};

}   // namespace internals
}   // namespace fdapde

#endif // __FDAPDE_AREAL_LAYER_H__
