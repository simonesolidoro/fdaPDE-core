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

#ifndef __FDAPDE_POINT_LAYER_H__
#define __FDAPDE_POINT_LAYER_H__

#include "header_check.h"

namespace fdapde {
namespace internals {

template <typename Triangulation_> struct point_layer {
    using Triangulation = typename std::remove_pointer_t<std::decay_t<Triangulation_>>;
    using layer_category = point_layer_tag;
    static constexpr int local_dim = Triangulation::local_dim;
    static constexpr int embed_dim = Triangulation::embed_dim;
    using geometry_type = Eigen::Matrix<double, embed_dim, 1>;

    point_layer() : triangulation_(nullptr), coords_(), locs_at_mesh_nodes_(false) { }
    template <typename GeoDescriptor>
    point_layer(Triangulation_* triangulation, GeoDescriptor data) noexcept :
        triangulation_(triangulation) {
        if constexpr (std::is_same_v<GeoDescriptor, int>) {
            fdapde_assert(data == gf_mesh_nodes);
            locs_at_mesh_nodes_ = true;
        } else {
            coords_ = std::make_shared<std::decay_t<GeoDescriptor>>(data);
            locs_at_mesh_nodes_ = false;
        }
    }
    // observers
    geometry_type coord(int i) const { return coordinates().row(i); }
    geometry_type operator[](int i) const { return coord(i); }
    int rows() const { return coordinates().rows(); }
    bool locs_at_mesh_nodes() const { return locs_at_mesh_nodes_; }
    // geometry
    const Eigen::Matrix<double, Dynamic, Dynamic>& coordinates() const {
        return coords_ ? *coords_ : triangulation().nodes();
    }
    // O(n) unique coordinates extraction
    Eigen::Matrix<double, Dynamic, Dynamic> unique_coordinates() const {
        if (locs_at_mesh_nodes_) { return triangulation().nodes(); }
        // find unique coordinates
        std::unordered_set<geometry_type, eigen_matrix_hash> coords_set;
        std::vector<int> coords_idx;
        for (int i = 0; i < coords_->rows(); ++i) {
            Eigen::Matrix<double, embed_dim, 1> p(coords_->row(i));
            if (!coords_set.contains(p)) {
                coords_set.insert(p);
                coords_idx.push_back(i);
            }
        }
	Eigen::Matrix<double, Dynamic, Dynamic> coords_unique(coords_set.size(), embed_dim);
        int i = 0;
        for (int idx : coords_idx) { coords_unique.row(i++) = coords_->row(idx); }
        return coords_unique;
    };
    // geometry
    Triangulation& triangulation() { return *triangulation_; }
    const Triangulation& triangulation() const { return *triangulation_; }
    Eigen::Matrix<int, Dynamic, 1> locate() const { return triangulation_.locate(*coords_); }
   private:
    bool locs_at_mesh_nodes_ = false;
    Triangulation* triangulation_;
    std::shared_ptr<Eigen::Matrix<double, Dynamic, Dynamic>> coords_;
};

}   // namespace internals
}   // namespace fdapde

#endif // __FDAPDE_POINT_LAYER_H__
