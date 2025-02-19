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
    areal_layer(Triangulation_* triangulation, const SubregionsType& regions) noexcept :
        triangulation_(triangulation), regions_(regions) { }
    // observers
    size_t n_regions() const { return regions_.size(); }
    // geometry
    const MultiPolygon<local_dim, embed_dim>& geometry(int i) const { return regions_[i]; }
    Triangulation& triangulation() { return *triangulation_; }
    const Triangulation& triangulation() const { return *triangulation_; }

    // computes measures of subdomains
    std::vector<double> measures() const {
        std::vector<double> m_(regions_.size(), 0);
        for (int i = 0; i < regions_.size(); ++i) { m_[i] = regions_[i].measure(); }
        return m_;
    }
    // computes matrix [M]_{ij} : [M]_{ij} == 1 \iff cell j is inside region i, 0 otherwise
    BinaryMatrix<Dynamic, Dynamic> incidence_matrix() const {
        BinaryMatrix<Dynamic, Dynamic> m(regions_.size(), triangulation().n_cells());
        // for each cell, check in which region its barycenter lies
        for (auto it = triangulation().cells_begin(); it != triangulation().cells_end(); ++it) {
            Eigen::Matrix<double, embed_dim, 1> barycenter = it->barycenter();
            for (int i = 0, n = regions_.size(); i < n; ++i) {
                if (regions_[i].contains(barycenter)) { m.set(i, it->id()); }
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
   private:
    Triangulation* triangulation_;
    std::vector<MultiPolygon<local_dim, embed_dim>> regions_;
    std::vector<std::vector<int>> incidence_list_;   // for each polygon, the cell ids belonging to it
};

}   // namespace internals
}   // namespace fdapde

#endif // __FDAPDE_AREAL_LAYER_H__
