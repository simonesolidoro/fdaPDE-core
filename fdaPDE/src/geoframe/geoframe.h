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

#ifndef __FDAPDE_GEOFRAME_H__
#define __FDAPDE_GEOFRAME_H__

#include "header_check.h"

namespace fdapde {
namespace internals {
  
inline void throw_geoframe_error(const std::string& msg) { throw std::runtime_error("GeoFrame: " + msg); }
#define geoframe_assert(condition, msg)                                                                                \
    if (!(condition)) { internals::throw_geoframe_error(msg); }

}   // namespace internals

template <typename... Triangulation_> struct GeoFrame {
    fdapde_static_assert(sizeof...(Triangulation_) > 0, AT_LEAST_ONE_TRIANGULATION_REQUIRED);
  //private:
    using This = GeoFrame<Triangulation_...>;
    using Triangulation = std::tuple<std::decay_t<Triangulation_>...>;
    static constexpr int Order = sizeof...(Triangulation_);

    struct layer_t {
        using Triangulation = std::tuple<std::decay_t<Triangulation_>...>;
        std::shared_ptr<void> layer_;
        std::array<ltype, Order> layer_category_;
        std::string layer_name_;
        internals::scalar_data_layer* data_;

        layer_t() noexcept : layer_(), layer_category_(), layer_name_(), data_() { }
        template <typename CategoryType, typename LayerType>
        layer_t(const std::string& layer_name, const CategoryType& layer_category, const LayerType& layer) :
            layer_name_(layer_name),
            layer_(std::make_shared<LayerType>(layer)),
            data_(std::addressof(reinterpret_cast<LayerType*>(layer_.get())->data())) {
            geoframe_assert(layer_category.size() == Order, "bad layer construction, no matching order.");
            std::copy(layer_category.begin(), layer_category.end(), layer_category_.begin());
        }
    };

    template <typename T> constexpr auto ltype_from_layer_tag() const {
        using T_ = std::decay_t<T>;
        if constexpr (std::is_same_v<T_, point_layer_tag>) return ltype::point;
        if constexpr (std::is_same_v<T_, areal_layer_tag>) return ltype::areal;
    }
   public:
    // static constexpr int Order = sizeof...(Triangulation_);
    static constexpr std::array<int, Order> local_dim {Triangulation_::local_dim...};
    static constexpr std::array<int, Order> embed_dim {Triangulation_::embed_dim...};
    using index_t = int;
    using size_t  = std::size_t;

    // constructors
    GeoFrame() noexcept : triangulation_(), layers_(), n_layers_(0) { }
    explicit GeoFrame(Triangulation_&... triangulation) noexcept :
        triangulation_(std::make_tuple(std::addressof(triangulation)...)), layers_(), n_layers_(0) { }

    template <typename... GeoInfo>
        requires(
          sizeof...(GeoInfo) == Order &&
          (internals::is_any_same_v<GeoInfo, std::tuple<internals::polygon_tag, internals::point_tag>> && ...))
    void add_scalar_layer(const std::string& name) {
        fdapde_static_assert(sizeof...(GeoInfo) == Order, BAD_LAYER_CONSTRUCTION__NO_MATCHING_ORDER);
        geoframe_assert(!name.empty() && !has_layer(name), "empty or duplicated layer name.");
        using geo_layer_t = GeoLayer<Triangulation, std::tuple<GeoInfo...>>;
        layers_.emplace_back(
          name, internals::apply_index_pack<sizeof...(GeoInfo)>([&]<int... Ns>() {
              return std::array<ltype, sizeof...(GeoInfo)> {ltype_from_layer_tag<typename GeoInfo::layer_tag>()...};
          }),
          internals::apply_index_pack<sizeof...(GeoInfo)>(
            [&, this]<int... Ns>() { return geo_layer_t(triangulation_); }));
        layer_name_to_idx_[name] = n_layers_;
        n_layers_++;
        return;
    }

    const std::array<ltype, Order>& layer_category(int layer_id) const { return layers_[layer_id].layer_category_; }

    // observers
    bool has_layer(const std::string& name) const {
        for (const layer_t& layer : layers_) {
            if (layer.layer_name_ == name) { return true; }
        }
        return false;
    }
    bool contains(const std::string& column) const {   // true if column is in geoframe
        for (int i = 0; i < n_layers_; ++i) {
            if (operator[](i).contains(column)) { return true; }
        }
        return false;
    }
  
    int n_layers() const { return n_layers_; }
    // indexed access
    const layer_t& operator[](int idx) const {
        geoframe_assert(idx < n_layers_, "out of bound access.");
	return layers_[idx];
    }
    const layer_t& operator[](const std::string& colname) const { return layers_[layer_name_to_idx_.at(colname)]; }

    // iterator
    class iterator {
        const GeoFrame* gf_;
        int index_;
       public:
        using value_type = internals::scalar_data_layer;
        using pointer = std::add_pointer_t<value_type>;
        using reference = std::add_lvalue_reference_t<value_type>;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        iterator(const GeoFrame* gf, int index) : gf_(gf), index_(index) { }
        const value_type* operator->() const { return std::addressof(gf_->operator[](index_)); }
        const value_type& operator*() const { return gf_->operator[](index_); }
        iterator& operator++() {
            index_++;
            return *this;
        }
        // ltype category() const { return *(gf_->layer_category(index_)); }
        const reference data() const { return operator*(); }
        // template <typename Tag> const auto& as(Tag t) const {
        //     return gf_->get_as(t, gf_->idx_to_layer_name_.at(index_));
        // }
        friend bool operator!=(const iterator& lhs, const iterator& rhs) { return lhs.index_ != rhs.index_; }
        friend bool operator==(const iterator& lhs, const iterator& rhs) { return lhs.index_ == rhs.index_; }
    };
    iterator begin() const { return iterator(this, 0); }
    iterator end() const { return iterator(this, n_layers_); }
    // geometry access
    // Triangulation_& triangulation() { return *triangulation_; }
    // const Triangulation_& triangulation() const { return *triangulation_; }
    // int n_cells() const { return triangulation_->n_cells(); }
    // int n_nodes() const { return triangulation_->n_nodes(); }
    // const Eigen::Matrix<double, Dynamic, Dynamic>& nodes() const { return triangulation_->nodes(); }
    // const Eigen::Matrix<int, Dynamic, Dynamic, Eigen::RowMajor>& cells() const { return triangulation_->cells(); }
   private:
    // data members
    std::tuple<std::add_pointer_t<std::decay_t<Triangulation_>>...> triangulation_;
    std::vector<layer_t> layers_;
    int n_layers_ = 0;
    std::unordered_map<std::string, int> layer_name_to_idx_;
};

template <typename... GeoInfo, typename DataLayer> auto& geo_cast(DataLayer&& data_layer) {
    return *reinterpret_cast<GeoLayer<typename std::decay_t<DataLayer>::Triangulation, std::tuple<GeoInfo...>>*>(
      data_layer.layer_.get());
}

}   // namespace fdapde

#endif // __FDAPDE_GEOFRAME_H__
