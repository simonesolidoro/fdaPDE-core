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

// geometrical layer, a plain_data_layer coupled with geometrical indexing
// template <typename... GeoIndex> struct GeoLayer {
//     using storage_t = internals::plain_data_layer;
//     using index_t = typename storage_t::index_t;
//     using size_t = typename storage_t::size_t;
//     template <typename T> using reference = typename storage_t::template reference<T>;
//     template <typename T> using const_reference = typename storage_t::template const_reference<T>;
//     static constexpr int Order = sizeof...(GeoIndex);
//     static constexpr std::array<int, Order> local_dim {GeoIndex::local_dim...};
//     static constexpr std::array<int, Order> embed_dim {GeoIndex::embed_dim...};
  
//     GeoLayer() noexcept = default;
//     template <typename... GeoIndexData>
//         requires(internals::is_pair_v<GeoIndexData> && ...) && (sizeof...(GeoIndexData) == Order)
//     GeoLayer(bool is_structured, GeoIndexData&&... geo_index_data) :
//         is_structured_(is_structured),
//         geo_index_(std::make_tuple(GeoIndex(std::get<0>(geo_index_data), std::get<1>(geo_index_data))...)) {
//         std::array<int, Order> sizes;
//         internals::for_each_index_and_args<Order>(
//           [&]<int Ns, typename GeoData>(GeoData data) {
// 	    if constexpr (std::is_same_v<std::tuple_element_t<1, std::decay_t<GeoData>>, int>) {
//                   const auto& triangulation_ = std::get<0>(data);
//                   sizes[Ns] = triangulation_->n_nodes();
//               } else {
//                   const auto& data_ = std::get<1>(data);
//                   sizes[Ns] = internals::is_eigen_dense_xpr_v<std::tuple_element_t<1, std::decay_t<GeoData>>> ?
//                                 data_.rows() :
//                                 data_.size();
//               }
//           },
//           geo_index_data...);
//         if (!is_structured_) {   // for unstructured layers, check that geo_data have same cardinality
//             if (Order > 1) {
//                 for (int i = 0; i < Order - 1; ++i) {
//                     geoframe_assert(
//                       sizes[i] == sizes[i + 1], "bad layer construction, invalid dimensions for unstructured layer.");
//                 }
//             }
//             n_rows_ = sizes[0];
//         } else {
//             n_rows_ = std::accumulate(sizes.begin(), sizes.end(), 1, std::multiplies<int>());
//         }
//     }
//     // observers
//     size_t rows() { return n_rows_; }
//     size_t cols() { return data_.cols(); }
//     size_t size() const { return n_rows_; }
//     const auto& field_descriptor(const std::string& colname) const { return data_.field_descriptor(colname); }
//     const auto& field_descriptors() const { return data_.field_descriptors(); }
//     bool contains(const std::string& column) const { return data_.contains(column); }
//     const storage_t& data() const { return data_; }
//     template <typename T> auto col(size_t col) const { data_.template col<T>(col); }
//     template <typename T> auto col(const std::string& colname) const { return data_.template col<T>(colname); }
//     // modifiers
//     template <typename T> auto col(size_t col) { return data_.template col<T>(col); }
//     template <typename T> auto col(const std::string& colname) { return data_.template col<T>(colname); }
//     storage_t& data() { return data_; }
//     template <typename... DataT> void set_data(DataT&&... data) {
//         data_ = storage_t(std::forward<DataT>(data)...);
//         geoframe_assert(data_.rows() == n_rows_, "bad layer construction, invalid data dimensions.")
//     }
//     void gridify() {
//         (   // operation well defined only for fully point layers
//           []() {
//               fdapde_static_assert(
//                 std::is_same_v<typename GeoIndex::layer_category FDAPDE_COMMA point_layer_tag>,
//                 THIS_METHOD_IS_FOR_POINT_LAYERS_ONLY);
//           }(),
//           ...);
//         if (is_structured_) return;
//         constexpr int full_embed_dim = std::accumulate(embed_dim.begin(), embed_dim.end(), 0);
// 	using geo_index_t = std::tuple<GeoIndex...>;
// 	using point_t = std::array<double, full_embed_dim>;
//         // compute full point set for each layer direction
//         std::tuple<GeoIndex...> grid_geo_index_;
//         std::array<int, Order> sizes;
//         internals::for_each_index_in_pack<Order>([&, this]<int Ns_>() mutable {
//             auto& layer = std::get<Ns_>(geo_index_);
//             std::get<Ns_>(grid_geo_index_) =
//               std::tuple_element_t<Ns_, geo_index_t>(std::addressof(layer.triangulation()), layer.unique_coordinates());
//             sizes[Ns_] = std::get<Ns_>(grid_geo_index_).rows();
//         });
//         // build temporary point index for amortized O(1) search
//         std::unordered_set<point_t, internals::std_array_hash<double, full_embed_dim>> grid_set_;
//         point_t p;
//         for (int i = 0; i < n_rows_; ++i) {
//             int offset = 0;
//             internals::for_each_index_in_pack<Order>([&]<int Ns_>() {
//                 const auto& coords = std::get<Ns_>(geo_index_).coordinates();
//                 for (int j = 0; j < coords.cols(); ++j) { p[offset + j] = coords(i, j); }
//                 offset += embed_dim[Ns_];
//             });
// 	    grid_set_.insert(p);
//         }
// 	// overall number of points in structured grid
// 	n_rows_ = std::accumulate(sizes.begin(), sizes.end(), 1, std::multiplies<int>());
//         // data storage buffer
//         std::vector<std::pair<std::string, internals::dtype>> column_descriptors;
//         for (auto field : data_.field_descriptors()) { column_descriptors.emplace_back(field.colname, field.type_id); }
//         internals::hetero_data_vector hetero_vec(column_descriptors);
// 	BinaryMatrix<Dynamic, Dynamic> nan_pattern(n_rows_, data_.cols());
// 	// start loop
//         std::array<int, Order> multi_index_ {};
//         std::fill_n(multi_index_.begin(), Order, 0);
//         int j = 0;
//         for (int i = 0; i < n_rows_; ++i) {
//             // check if coordinate was in the initial unstructured grid
//             int offset = 0;
//             internals::for_each_index_in_pack<Order>([&]<int Ns_>() {   // build coordinate in cartesian product
//                 const auto& coords = std::get<Ns_>(grid_geo_index_).coord(multi_index_[Ns_]);
//                 for (int j = 0; j < embed_dim[Ns_]; ++j) { p[offset + j] = coords[j]; }
//                 offset += embed_dim[Ns_];
//             });
//             bool found = grid_set_.contains(p);
//             {   // increase multi index
//                 multi_index_[0]++;
//                 int k = 0;
//                 while (multi_index_[k] >= sizes[k]) {
//                     multi_index_[k] = 0;
//                     multi_index_[++k]++;
//                 }
//             }
//             if (found) {
//                 hetero_vec.push_back(data_.row(j));
//                 nan_pattern.row(i) = data_.row(j).nan();
//                 j++;
//             } else {   // push nan for each supported type
//                 nan_pattern.row(i).set();
//                 hetero_vec.push_back([]<typename T>([[maybe_unused]] T) -> T {
//                     if constexpr (std::is_same_v<T, std::string>) {
//                         return std::string("NA");
//                     } else {
//                         return std::numeric_limits<T>::quiet_NaN();
//                     }
//                 });
//             }
//         }
// 	// finalize
//         data_ = storage_t(hetero_vec);
// 	data_.set_nan(data_.colnames(), nan_pattern);
// 	geo_index_ = grid_geo_index_;
//         is_structured_ = true;
//     }


  
//   // if i ask for the j-th point, we need to proper index the information at geo_indexes_, this depends on the type of geometrical structure we have
  

//     // geometry access
//     template <int N> auto& geometry() { return std::get<N>(geo_index_); }
//     template <int N> const auto& geometry() const { return std::get<N>(geo_index_); }
//    private:
//     bool is_structured_;
//     int n_rows_;
//     storage_t data_;
//     std::string layer_name_;
//     std::tuple<GeoIndex...> geo_index_;
// };

  // a structured GeoLayer is when there is a geo_index for each direction of the layer
  // if there is only one, and layer is point, we speek about unstructured GeoLayer
  
  // unstructured GeoLayer?
  
template <typename... Triangulation_> struct GeoFrame {
    fdapde_static_assert(sizeof...(Triangulation_) > 0, AT_LEAST_ONE_TRIANGULATION_REQUIRED);
  //private:
    using This = GeoFrame<Triangulation_...>;
    using Triangulation = std::tuple<std::decay_t<Triangulation_>...>;
    static constexpr int Order = sizeof...(Triangulation_);

    struct layer_t {   // better naming... layer_t?
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
    // template <typename LayerTag, typename Triangulation>
    // using layer_type_from_layer_tag = std::conditional_t<
    //   std::is_same_v<std::decay_t<LayerTag>, point_layer_tag>, internals::point_layer<Triangulation>,
    //   internals::areal_layer<Triangulation>>;

    // template <typename Triangulation, typename GeoDescriptor> struct is_valid_geo_point_descriptor {
    //     static constexpr bool value = []() {
    //         if constexpr (requires(GeoDescriptor) { typename GeoDescriptor::iterator; }) {
    //             return std::contiguous_iterator<typename GeoDescriptor::iterator> ||
    //                    internals::is_eigen_dense_xpr_v<GeoDescriptor>;
    //         } else {
    //             return false;
    //         }
    //     }();
    // };
    // template <typename Triangulation, typename GeoDescriptor> struct is_valid_geo_areal_descriptor {
    //     static constexpr bool value = []() {
    //         if constexpr (requires(GeoDescriptor) {
    //                           typename GeoDescriptor::iterator;
    //                           typename GeoDescriptor::value_type;
    //                       }) {
    //             return std::contiguous_iterator<typename GeoDescriptor::iterator> &&
    //                    std::is_same_v<
    //                      typename GeoDescriptor::value_type,
    //                      MultiPolygon<Triangulation::local_dim, Triangulation::embed_dim>>;
    //         } else {
    //             return std::is_same_v<GeoDescriptor, MultiPolygon<Triangulation::local_dim, Triangulation::embed_dim>>;
    //         }
    //     }();
    // };
    // template <typename Triangulation, typename GeoDescriptor> struct is_valid_geo_descriptor {
    //     using Triangulation__ = std::decay_t<Triangulation>;
    //     using GeoDescriptor__ = std::decay_t<GeoDescriptor>;
    //     static constexpr bool value = std::is_same_v<GeoDescriptor__, int> ||
    //                                   std::is_same_v<GeoDescriptor__, std::string> ||
    //                                   std::is_same_v<GeoDescriptor__, const char*> ||
    //                                   is_valid_geo_point_descriptor<Triangulation__, GeoDescriptor__>::value ||
    //                                   is_valid_geo_areal_descriptor<Triangulation__, GeoDescriptor__>::value;
    // };
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

    // modifiers

  // layer category must have the same Order of the geoframe
  // if all directions are point, then geo_descriptors can be sized one for an unstructured layer, otherwise must have Order cardinality
  // shapefile can be load only for Order 1 geoframes
  // you can supply csv files as coordinate files

  // we need to put coordinates in shared_ptr, then do packed push

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

    // getters

    // template <typename ColnamesContainer>
    // void push(const ColnamesContainer& layers, layer_t::point_t) {
    //     for (const auto& name : layers) { push(name, layer_t::point); }
    // }
    // // multipoint layer with specified locations
    // template <typename T>
    //     requires(std::is_convertible_v<T, std::string>)
    // void push(const std::initializer_list<T>& layers, layer_t::point_t) {
    //     for (auto it = layers.begin(); it != layers.end(); ++it) { push(*it, layer_t::point); }
    // }
    // template <typename CoordsType>
    //     requires(internals::is_eigen_dense_xpr_v<CoordsType> || std::contiguous_iterator<typename CoordsType::iterator>)
    // void push(const std::string& name, layer_t::point_t, const CoordsType& coords) {
    //     geoframe_assert(!name.empty() && !has_layer(name), "empty or duplicated name.");
    // 	using layer_t = internals::point_layer<This>;
    //     if constexpr (internals::is_eigen_dense_xpr_v<CoordsType>) {
    //         geoframe_assert(
    //           coords.cols() == embed_dim && coords.rows() > 0, "empty or wrongly sized coordinate matrix.");
    //         using Scalar__ = typename CoordsType::Scalar;
    //         using MatrixType = Eigen::Matrix<Scalar__, Dynamic, Dynamic>;
    //         std::shared_ptr<MatrixType> coords_ptr = std::make_shared<MatrixType>(coords);
    //         fetch_<layer_t>(layers_).insert({name, layer_t(name, this, coords_ptr)});
    //     } else {
    //         geoframe_assert(
    //           coords.size() > 0 && coords.size() % embed_dim == 0, "empty or wrongly sized coordinate matrix.");
    //         using Scalar__ = typename CoordsType::value_type;
    //         using MatrixType = Eigen::Matrix<Scalar__, Dynamic, Dynamic>;
    //         int n_rows = coords.size() / embed_dim;
    //         int n_cols = embed_dim;
    //         std::shared_ptr<MatrixType> coords_ptr =
    //           std::make_shared<MatrixType>(Eigen::Map<const MatrixType>(coords.data(), n_rows, n_cols));
    //         fetch_<layer_t>(layers_).insert({name, layer_t(name, this, coords_ptr)});
    //     }
    //     idx_to_layer_name_[n_layers_] = name;
    //     n_layers_++;
    //     return;
    // }
    // // packed multipoint layers sharing the same locations
    // template <typename T, typename CoordsType>
    //     requires(
    //       (internals::is_eigen_dense_xpr_v<CoordsType> || std::contiguous_iterator<typename CoordsType::iterator>) &&
    //       std::is_convertible_v<T, std::string>)
    // void push(const std::initializer_list<T>& layers, layer_t::point_t, const CoordsType& coords) {
    //     using layer_t = internals::point_layer<This>;
    //     // layers share the same locations, allocate here once
    //     using Scalar__ = decltype([]() {
    //         if constexpr (internals::is_eigen_dense_xpr_v<CoordsType>) return typename CoordsType::Scalar();
    //         else return typename CoordsType::value_type();
    //     }());
    // 	using MatrixType = Eigen::Matrix<Scalar__, Dynamic, Dynamic>;
    //     std::shared_ptr<MatrixType> coords_ptr;
    //     if constexpr (internals::is_eigen_dense_xpr_v<CoordsType>) {
    //         coords_ptr = std::make_shared<MatrixType>(coords);
    //     } else {
    //         int n_rows = coords.size() / embed_dim;
    //         int n_cols = embed_dim;
    //         coords_ptr = std::make_shared<MatrixType>(Eigen::Map<const MatrixType>(coords.data(), n_rows, n_cols));
    //     }
    //     for (auto it = layers.begin(); it != layers.end(); ++it) {
    //         std::string name(*it);
    //         geoframe_assert(!name.empty() && !has_layer(name), "empty or duplicated name.");
    //         fetch_<layer_t>(layers_).insert({name, layer_t(name, this, coords_ptr)});
    //         idx_to_layer_name_[n_layers_] = name;
    //         n_layers_++;
    //     }
    // }

    // // areal layer
    // void push(
    //   const std::string& name, layer_t::areal_t, const std::vector<MultiPolygon<local_dim, embed_dim>>& regions) {
    //     geoframe_assert(!name.empty() && !has_layer(name), "empty or duplicated name.");
    //     using layer_t = internals::areal_layer<This>;
    //     using areal_t = std::vector<MultiPolygon<local_dim, embed_dim>>;
    //     std::shared_ptr<areal_t> regions_ptr = std::make_shared<areal_t>(regions);
    //     fetch_<layer_t>(layers_).insert({name, layer_t(name, this, regions_ptr)});
    //     idx_to_layer_name_[n_layers_] = name;
    // 	n_layers_++;
    //     return;
    // }
    // // packed areal layers sharing the same regions
    // template <typename T>
    //     requires(std::is_convertible_v<T, std::string>)
    // void push(
    //   const std::initializer_list<T>& layers, layer_t::areal_t,
    //   const std::vector<MultiPolygon<local_dim, embed_dim>>& regions) {
    //     using layer_t = internals::areal_layer<This>;
    // 	using areal_t = std::vector<MultiPolygon<local_dim, embed_dim>>;
    //     // allocate shared memory
    //     std::shared_ptr<areal_t> regions_ptr = std::make_shared<areal_t>(regions);
    //     for (auto it = layers.begin(); it != layers.end(); ++it) {
    //         std::string name(*it);
    //         geoframe_assert(!it->empty() && !has_layer(name), "empty or duplicated name.");
    //         fetch_<layer_t>(layers_).insert({name, layer_t(name, this, regions_ptr)});
    //         idx_to_layer_name_[n_layers_] = name;
    // 	    n_layers_++;
    //     }
    //     return;
    // }
    // // construct layer from a subset of another layer
    // template <typename T>
    //     requires(std::is_convertible_v<T, std::string> || std::is_convertible_v<T, index_t>)
    // void push(
    //   const std::string& name, layer_t::areal_t,
    //   const internals::plain_row_filter<internals::areal_layer<This>>& filter, const std::vector<T>& cols) {
    //     std::vector<MultiPolygon<local_dim, embed_dim>> regions;
    //     regions.reserve(filter.rows());
    // 	for (int i = 0, n = filter.rows(); i < n; ++i) { regions.emplace_back(filter(i).geometry()); }
    //     push(name, layer_t::areal, regions);
    //     if constexpr (std::is_same_v<T, std::string>) {
    //         get_as(layer_t::areal, name).data() = typename internals::areal_layer<This>::storage_t(filter, cols);
    //     }
    //     if constexpr (std::is_convertible_v<T, index_t>) {
    //         auto field_descriptors = filter.field_descriptors();
    //         std::vector<std::string> cols_;
    //         for (index_t i : cols) { cols_.push_back(field_descriptors[i].colname); }
    //         get_as(layer_t::areal, name).data() = typename internals::areal_layer<This>::storage_t(filter, cols_);
    //     }
    // }
    // template <typename T>
    //     requires(std::is_convertible_v<T, std::string> || std::is_convertible_v<T, index_t>)
    // void push(
    //   const std::string& name, layer_t::areal_t,
    //   const internals::plain_row_filter<internals::areal_layer<This>>& filter, const std::initializer_list<T>& cols) {
    //     std::vector<std::string> cols_;
    //     if constexpr (std::is_convertible_v<T, std::string>) { cols_.insert(cols_.begin(), cols.begin(), cols.end()); }
    //     if constexpr (std::is_convertible_v<T, index_t>) {
    //         auto field_descriptors = filter.field_descriptors();
    //         for (index_t i : cols) { cols_.push_back(field_descriptors[i].colname); }
    //     }
    //     push(name, layer_t::areal_t {}, filter, cols_);
    // }
    // void push(
    //   const std::string& name, layer_t::areal_t,
    //   const internals::plain_row_filter<internals::areal_layer<This>>& filter) {
    //     push(name, layer_t::areal_t {}, filter, filter.colnames());
    // }

    // // directly push an externally created layer
    // template <typename Layer>
    //     requires(is_supported_layer_v<std::decay_t<Layer>>)
    // void push(const std::string& name, Layer&& layer) {
    //     using layer_t = std::decay_t<Layer>;
    //     fetch_<layer_t>(layers_).insert({name, layer});
    //     idx_to_layer_name_[n_layers_] = name;
    // 	n_layers_++;
    //     return;
    // }
    // void erase(const std::string& layer_name) {
    //     // search for layer_name (if no layer found does nothing)
    //     if (!has_layer(layer_name)) return;
    //     std::apply(
    //       [&](auto&&... layer) {
    //           (std::erase_if(
    //              layer,
    //              [&](const auto& item) {
    //                  auto const& [k, v] = item;
    //                  return k == layer_name;
    //              }),
    //            ...);
    //       },
    //       layers_);
    // 	// update idx - layer_name mapping
    // 	int i = 0;
    //     for (; i < n_layers_; ++i) {
    //         if (idx_to_layer_name_.at(i) == layer_name) {
    //             idx_to_layer_name_.erase(i);
    //             break;
    //         }
    //     }
    //     for (; i < n_layers_ - 1; ++i) { idx_to_layer_name_[i] = idx_to_layer_name_.at(i + 1); }
    // 	idx_to_layer_name_.erase(n_layers_ - 1);
    // 	n_layers_--;
    //     return;
    // }

  // there are two orthogonal variables here: the type of spatial information (point/areal/raster) and the type of datum (scalar/vector/tensor)

// with csv we can only make scalar-unstructured point layers
  
  
    // layer access
    // template <typename LayerCategory> auto& get_as(LayerCategory tag, const std::string& layer) {
    //     return get_as_(tag, layer);
    // }
    // template <typename LayerCategory> const auto& get_as(LayerCategory tag, const std::string& layer) const {
    //     return get_as_(tag, layer);
    // }
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
  
  
    // std::optional<ltype> layer_category(const std::string& name) const {
    //     geoframe_assert(has_layer(name), std::string("key " + name + " not found."));
    //     if (fetch_<point_layer_t>(layers_).contains(name)) return ltype::point;
    //     if (fetch_<areal_layer_t>(layers_).contains(name)) return ltype::areal;
    // 	return std::nullopt;
    // }
    // std::optional<ltype> layer_category(int idx) const { return layer_category(idx_to_layer_name_.at(idx)); }
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
    // internal utilities
    // template <typename LayerCategory> decltype(auto) get_as_(LayerCategory tag, const std::string& layer) {
    //     using layer_t = typename strip_tuple_into<
    //       GeoLayer,
    //       std::decay_t<decltype(internals::apply_index_pack<LayerCategory::Order>([]<int... Ns>() {
    //           return std::tuple<layer_type_from_layer_tag<
    //             std::tuple_element_t<Ns, typename LayerCategory::type>,
    //             std::tuple_element_t<Ns, Triangulation>>...> {};
    //       }))>>::type;
    //     // layer category check
    //     internals::for_each_index_in_pack<LayerCategory::Order>([&]<int Ns>() {
    //         fdapde_assert(
    //           ltype_from_layer_tag<std::tuple_element_t<Ns FDAPDE_COMMA typename LayerCategory::type>>() ==
    //           layers_[layer_name_to_idx_[layer]].layer_category_[Ns]);
    //     });
    //     return *reinterpret_cast<layer_t*>(layers_[layer_name_to_idx_[layer]].layer_.get());
    // }
    // data members
    std::tuple<std::add_pointer_t<std::decay_t<Triangulation_>>...> triangulation_;
    std::vector<layer_t> layers_;
    int n_layers_ = 0;
    std::unordered_map<std::string, int> layer_name_to_idx_;
};

template <typename... GeoInfo, typename DataLayer> auto geo_cast(DataLayer&& data_layer) {
    return *reinterpret_cast<GeoLayer<typename std::decay_t<DataLayer>::Triangulation, std::tuple<GeoInfo...>>*>(
      data_layer.layer_.get());
}

}   // namespace fdapde

#endif // __FDAPDE_GEOFRAME_H__
