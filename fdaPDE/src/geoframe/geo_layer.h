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

#ifndef __FDAPDE_GEO_LAYER_H__
#define __FDAPDE_GEO_LAYER_H__

#include "header_check.h"

namespace fdapde {

namespace internals {

struct point_tag {
    using layer_tag = point_layer_tag;
    template <int LocalDim, int EmbedDim> using element_type = Eigen::Matrix<double, LocalDim, 1>;
};
struct polygon_tag {
    using layer_tag = areal_layer_tag;
    template <int LocalDim, int EmbedDim> using element_type = MultiPolygon<LocalDim, EmbedDim>;
};

}   // namespace internals

// available geometric features
using POLYGON = internals::polygon_tag;
using POINT   = internals::point_tag;

// geometrical layer, a plain_data_layer coupled with geometrical indexing
template </*int Rows, int Cols, */ typename Triangulation_, typename GeoInfo_>
    requires(
      internals::is_tuple_v<GeoInfo_> && internals::is_tuple_v<Triangulation_> &&
      std::tuple_size_v<Triangulation_> == std::tuple_size_v<GeoInfo_>)
struct GeoLayer {
   private:
    template <typename LayerTag, typename Triangulation>
    using layer_type_from_layer_tag = std::conditional_t<
      std::is_same_v<LayerTag, internals::point_tag>, internals::point_layer<Triangulation>,
      internals::areal_layer<Triangulation>>;
    template <typename U, typename V> struct geo_storage_t_impl;
    template <typename... Us, typename... Vs> struct geo_storage_t_impl<std::tuple<Us...>, std::tuple<Vs...>> {
        using type = std::tuple<layer_type_from_layer_tag<Us, Vs>...>;
    };
    template <typename U> struct triangulation_storage;
    template <typename... Us> struct triangulation_storage<std::tuple<Us...>> {
        using type = std::tuple<std::add_pointer_t<Us>...>;
    };
    using triangulation_t = typename triangulation_storage<Triangulation_>::type;
   public:
    using storage_t = internals::scalar_data_layer;
    using geo_storage_t = typename geo_storage_t_impl<GeoInfo_, Triangulation_>::type;
    using index_t = typename storage_t::index_t;
    using size_t = typename storage_t::size_t;
    using Triangulation = Triangulation_;
    using GeoInfo = GeoInfo_;
    template <typename T> using reference = typename storage_t::template reference<T>;
    template <typename T> using const_reference = typename storage_t::template const_reference<T>;
    static constexpr int Order = std::tuple_size_v<Triangulation>;
    static constexpr std::array<int, Order> local_dim = internals::apply_index_pack<Order>(
      []<int... Ns>() { return std::array<int, Order> {std::tuple_element_t<Ns, Triangulation>::local_dim...}; });
    static constexpr std::array<int, Order> embed_dim = internals::apply_index_pack<Order>(
      []<int... Ns>() { return std::array<int, Order> {std::tuple_element_t<Ns, Triangulation>::embed_dim...}; });
    static constexpr bool is_full_geo_point = []() {
        bool value_ = true;
        internals::for_each_index_in_pack<Order>(
          [&]<int Ns>() { value_ &= std::is_same_v<std::tuple_element_t<Ns, GeoInfo>, internals::point_tag>; });
        return value_;
    }();

    // static constexpr std::array<index_t, 2> Shape {Rows, Cols};

    GeoLayer(triangulation_t triangulation) :
        triangulation_(triangulation), data_(), geo_data_(), n_rows_(0), strides_(), extents_(), is_structured_(false) {
        std::fill(strides_.begin(), strides_.end(), 1);   // default: unstructured layer
    }
    // observers
    // const std::array<int, 2>& shape() const { return Shape; }
    size_t rows() const { return n_rows_; }
    size_t cols() const { return data_.cols(); }
    size_t size() const { return rows() * cols(); }
    const auto& field_descriptor(const std::string& colname) const { return data_.field_descriptor(colname); }
    const auto& field_descriptors() const { return data_.field_descriptors(); }
    bool contains(const std::string& column) const { return data_.contains(column); }
    const storage_t& data() const { return data_; }
    template <typename T> auto col(size_t col) const { data_.template col<T>(col); }
    template <typename T> auto col(const std::string& colname) const { return data_.template col<T>(colname); }
    auto nan_pattern(const std::string& colname) const { return data_.nan_pattern(colname); }
    // modifiers
    template <typename T> auto col(size_t col) { return data_.template col<T>(col); }
    template <typename T> auto col(const std::string& colname) { return data_.template col<T>(colname); }
    storage_t& data() { return data_; }

    template <typename... GeoEntity>
        requires(sizeof...(GeoEntity) == Order)
    void push_grid(GeoEntity&&... geo_entity) {
        fdapde_static_assert(is_full_geo_point, THIS_METHOD_IS_FOR_FULL_POINT_LAYERS_ONLY);
	
        constexpr int full_embed_dim = std::accumulate(embed_dim.begin(), embed_dim.end(), 0);
        using point_t = std::array<double, full_embed_dim>;
        // compute full point set for each layer direction
        internals::for_each_index_and_args<Order>(
          [&, this]<int Ns_, typename T>(T coords) mutable {
              auto& layer = std::get<Ns_>(geo_data_);
              // unique coordinates
              std::unordered_set<Eigen::Matrix<double, embed_dim[Ns_], 1>, internals::eigen_matrix_hash> coords_set;
              std::vector<int> coords_idx;
              for (int i = 0; i < coords.rows(); ++i) {
                  Eigen::Matrix<double, embed_dim[Ns_], 1> p(coords.row(i));
                  if (!coords_set.contains(p)) {
                      coords_set.insert(p);
                      coords_idx.push_back(i);
                  }
              }
              Eigen::Matrix<double, Dynamic, Dynamic> coords_unique(coords_set.size(), embed_dim[Ns_]);
              int i = 0;
              for (int idx : coords_idx) { coords_unique.row(i++) = coords.row(idx); }
              // push in layer
              std::get<Ns_>(geo_data_) =
                std::tuple_element_t<Ns_, geo_storage_t>(std::get<Ns_>(triangulation_), coords_unique);
              extents_[Ns_] = std::get<Ns_>(geo_data_).rows();
          },
          geo_entity...);
        // overall number of points in structured grid
        n_rows_ = std::accumulate(extents_.begin(), extents_.end(), 1, std::multiplies<int>());
	// set strides
	strides_[0] = 1;
        for (int i = 1; i < Order; ++i) {
            for (int j = 0; j < i; ++j) { strides_[i] *= extents_[j]; }
        }
	is_structured_ = true;
        return;
    }

    // template <typename... GeoEntity>
    //     requires(sizeof...(GeoEntity) == Order)
    // void push_back(GeoEntity&&... geo_entity) {
    //     // if GeoEntity is either a matrix or a vector, gridify
    //     // if full point and a single matrix, simply copy
    //     // if not vector nor matrix (like a polygon - point pair, push back single line)
    // }
    void push_back(const Eigen::Matrix<double, Dynamic, Dynamic>& coords) {
        fdapde_static_assert(is_full_geo_point, THIS_METHOD_IS_FOR_FULL_POINT_LAYERS_ONLY);
	
        constexpr int full_embed_dim = std::accumulate(embed_dim.begin(), embed_dim.end(), 0);
        fdapde_assert(coords.cols() == full_embed_dim);
        int offset = 0;
        internals::for_each_index_in_pack<Order>([&]<int Ns_>() {
            Eigen::Matrix<double, Dynamic, Dynamic> coords_ = coords.middleCols(offset, embed_dim[Ns_]);
            std::get<Ns_>(geo_data_) = std::tuple_element_t<Ns_, geo_storage_t>(std::get<Ns_>(triangulation_), coords_);
            offset += embed_dim[Ns_];
        });
        n_rows_ = coords.rows();
        return;
    }

    void gridify() {
        fdapde_static_assert(is_full_geo_point, THIS_METHOD_IS_FOR_FULL_POINT_LAYERS_ONLY);
        internals::for_each_index_in_pack<Order>(
          [&]<int Ns_>() { fdapde_assert(std::get<Ns_>(geo_data_).rows() != 0); });

        constexpr int full_embed_dim = std::accumulate(embed_dim.begin(), embed_dim.end(), 0);	
        using point_t = std::array<double, full_embed_dim>;

          // compute full point set for each layer direction
        geo_storage_t grid_geo_data_;
        internals::for_each_index_in_pack<Order>([&, this]<int Ns_>() mutable {
            auto& layer = std::get<Ns_>(geo_data_);
            std::get<Ns_>(grid_geo_data_) =
              std::tuple_element_t<Ns_, geo_storage_t>(std::get<Ns_>(triangulation_), layer.unique_coordinates());
            extents_[Ns_] = std::get<Ns_>(grid_geo_data_).rows();
        });
        // build temporary point index for amortized O(1) search
        std::unordered_set<point_t, internals::std_array_hash<double, full_embed_dim>> grid_set_;
        point_t p;
        for (int i = 0; i < n_rows_; ++i) {
            int offset = 0;
            internals::for_each_index_in_pack<Order>([&]<int Ns_>() {
                const auto& coords = std::get<Ns_>(geo_data_).coordinates();
                for (int j = 0; j < coords.cols(); ++j) { p[offset + j] = coords(i, j); }
                offset += embed_dim[Ns_];
            });
            grid_set_.insert(p);
        }
        // overall number of points in structured grid
        n_rows_ = std::accumulate(extents_.begin(), extents_.end(), 1, std::multiplies<int>());
        if (data_.rows() != 0) {
            // data storage buffer
            std::vector<std::pair<std::string, internals::dtype>> column_descriptors;
            for (auto field : data_.field_descriptors()) {
                column_descriptors.emplace_back(field.colname, field.type_id);
            }
            internals::hetero_data_vector hetero_vec(column_descriptors);
            BinaryMatrix<Dynamic, Dynamic> nan_pattern(n_rows_, data_.cols());
            // start loop
            std::array<int, Order> multi_index_ {};
            std::fill_n(multi_index_.begin(), Order, 0);
            int j = 0;
            for (int i = 0; i < n_rows_; ++i) {
                // check if coordinate was in the initial unstructured grid
                int offset = 0;
                internals::for_each_index_in_pack<Order>([&]<int Ns_>() {   // build tensor product coordinate
                    const auto& coords = std::get<Ns_>(grid_geo_data_).coord(multi_index_[Ns_]);
                    for (int j = 0; j < embed_dim[Ns_]; ++j) { p[offset + j] = coords[j]; }
                    offset += embed_dim[Ns_];
                });
                bool found = grid_set_.contains(p);
                {   // increase multi index
                    multi_index_[0]++;
                    int k = 0;
                    while (multi_index_[k] >= extents_[k]) {
                        multi_index_[k] = 0;
                        multi_index_[++k]++;
                    }
                }
                if (found) {
                    hetero_vec.push_back(data_.row(j));
                    nan_pattern.row(i) = data_.row(j).nan();
                    j++;
                } else {   // push nan for each supported type
                    nan_pattern.row(i).set();
                    hetero_vec.push_back([]<typename T>([[maybe_unused]] T) -> T {
                        if constexpr (std::is_same_v<T, std::string>) {
                            return std::string("NA");
                        } else {
                            return std::numeric_limits<T>::quiet_NaN();
                        }
                    });
                }
            }
            // finalize
            data_ = storage_t(hetero_vec);
            data_.set_nan(data_.colnames(), nan_pattern);
        }
        geo_data_ = grid_geo_data_;
	// set strides
	strides_[0] = 1;
        for (int i = 1; i < Order; ++i) {
            for (int j = 0; j < i; ++j) { strides_[i] *= extents_[j]; }
        }
        is_structured_ = true;
        return;
    }

    // data import
    template <typename T>
    void load_csv(
      const std::vector<std::string>& colnames, std::string filename, bool header = true, bool index_col = false) {
        auto csv = read_csv<T>(filename, header, index_col);
	if (csv.cols() != colnames.size()) { throw std::runtime_error("invalid number of columns."); }
	load_table_<T>(colnames, csv);
        return;
    }
    template <typename T> void load_csv(std::string filename, bool header = true, bool index_col = false) {
        auto csv = read_csv<T>(filename, header, index_col);
        load_table_<T>(csv.colnames(), csv);
        return;
    }
    template <typename T>
    void load_txt(
      const std::vector<std::string>& colnames, std::string filename, bool header = true, bool index_col = false) {
        auto txt = read_txt<T>(filename, header, index_col);
	if (txt.cols() != colnames.size()) { throw std::runtime_error("invalid number of columns."); }
	load_table_<T>(colnames, txt);
        return;
    }
    template <typename T> void load_txt(std::string filename, bool header = true, bool index_col = false) {
        auto txt = read_txt<T>(filename, header, index_col);
        load_table_<T>(txt.colnames(), txt);
        return;
    }
    void load_shp(const std::string& name, const std::string& filename) {
        fdapde_static_assert(
          Order == 1 && std::is_same_v<std::tuple_element_t<0 FDAPDE_COMMA GeoInfo> FDAPDE_COMMA internals::point_tag>,
          THIS_METHOD_IS_FOR_POLYGONAL_ORDER_ONE_LAYERS_ONLY);
        std::string filename_ = std::filesystem::current_path().string() + "/" + filename;
        if (!std::filesystem::exists(filename_)) { throw std::runtime_error("file " + filename_ + " not found."); }
        SHPFile shp(filename_);
        // dispatch to processing logic
        switch (shp.shape_type()) {
        case shp_reader::Polygon: {
            // load polygon as areal layer
            std::vector<MultiPolygon<local_dim[0], embed_dim[0]>> regions;
            regions.reserve(shp.n_records());
            for (int i = 0, n = shp.n_records(); i < n; ++i) { regions.emplace_back(shp.polygon(i).nodes()); }
	    std::get<0>(geo_data_) = std::tuple_element_t<0, geo_storage_t>(std::get<0>(triangulation_), regions);
	    break;
        }
        }
        std::vector<std::pair<std::string, std::vector<double     >>> dbl_data;
        std::vector<std::pair<std::string, std::vector<std::string>>> str_data;
        // TODO: std::vector<std::pair<std::string, std::vector<bool       >>> bin_data;
        for (const auto& [name, field_type] : shp.field_descriptors()) {
            if (field_type == 'N' || field_type == 'F') { dbl_data.emplace_back(name, shp.get<double>(name)); }
            if (field_type == 'C') { str_data.emplace_back(name, shp.get<std::string>(name)); }
        }
	data_ = storage_t(dbl_data, str_data);
	return;
    }

    template <typename... Ts>
        requires(
          (std::contiguous_iterator<typename Ts::iterator> &&
           storage_t::is_type_supported_v<typename Ts::value_type>) &&
          ...)
    void load_vec(const std::vector<std::string>& colnames, const Ts&... data) {
        fdapde_assert(colnames.size() == sizeof...(data));
	data_ = storage_t(colnames, data...);
	return;
    }

    // geometry access
    template <int N> auto& geometry() { return std::get<N>(geo_data_); }
    template <int N> const auto& geometry() const { return std::get<N>(geo_data_); }
    auto geometry(int i) const {
        fdapde_assert(i < n_rows_);
	using internals::apply_index_pack;
        if constexpr (Order == 1) {
            return apply_index_pack<Order>([&]<int... Ns>() { return std::make_tuple(geometry<Ns>()[i]...); });
        } else {
            // move index i to tensorized index (i_1, i_2, ..., i_Order)
            std::array<int, Order> index {};
            int k = i;
            for (int j = 0; j < Order - 1; ++j) {
                while ((k - strides_[Order - 1 - j]) >= 0) {
                    k = k - strides_[Order - 1 - j];
                    index[Order - 1 - j]++;
                }
            }
	    index[0] = k;
            return apply_index_pack<Order>([&]<int... Ns>() { return std::make_tuple(geometry<Ns>()[index[Ns]]...); });
        }
    }

    // output stream
    friend std::ostream& operator<<(std::ostream& os, const GeoLayer& data) {
      	int n_rows = std::min(size_t(8), data.rows());
        int n_cols = Order + data.cols();
        std::vector<std::vector<std::string>> out;
        out.resize(n_cols);
        std::vector<std::size_t> max_size(n_cols, 0);
        auto print =
          [&]<typename T>(std::vector<std::string>& out, const std::string& typestring, const std::string& colname) {
              out.push_back(colname);
              out.push_back(typestring);
              auto col = data.col<T>(colname);
              if constexpr (!std::is_same_v<T, std::string>) {
                  for (int i = 0; i < n_rows; ++i) {
                      out.push_back(data.nan_pattern(colname)[i] ? "NA" : std::to_string(col(i)));
                  }
              } else {
                  for (int i = 0; i < n_rows; ++i) { out.push_back(data.nan_pattern(colname)[i] ? "NA" : col(i)); }
              }
          };
	// push geometrical description
        internals::for_each_index_in_pack<Order>([&]<int Ns>() {
            out[Ns].push_back("");
            using geo_t = std::tuple_element_t<Ns, GeoInfo>;
            if constexpr (std::is_same_v<geo_t, internals::point_tag>) {
                out[Ns].push_back("<POINT>");
                for (int i = 0; i < n_rows; ++i) {
                    auto coord = std::get<Ns>(data.geometry(i));
                    std::string coord_str = "(";
                    for (int j = 0; j < coord.size() - 1; ++j) { coord_str += std::to_string(coord[j]) + ", "; }
                    coord_str += std::to_string(coord[coord.size() - 1]) + ")";
                    out[Ns].push_back(coord_str);
                }
            }
            if constexpr (std::is_same_v<geo_t, internals::polygon_tag>) { out[Ns].push_back("<POLYGON>"); }
        });
	using internals::dtype;
        for (int i = Order, n = n_cols; i < n; ++i) {
            const std::string& colname = data.field_descriptors()[i - Order].colname;
            dtype coltype = data.field_descriptors()[i - Order].type_id;
            if (coltype == dtype::flt64) { print.template operator()<double      >(out[i], "<flt64>", colname); }
            if (coltype == dtype::flt32) { print.template operator()<float       >(out[i], "<flt32>", colname); }
            if (coltype == dtype::int64) { print.template operator()<std::int64_t>(out[i], "<int64>", colname); }
            if (coltype == dtype::int32) { print.template operator()<std::int32_t>(out[i], "<int32>", colname); }
            if (coltype == dtype::bin)   { print.template operator()<bool        >(out[i], "<bin>"  , colname); }
            if (coltype == dtype::str)   { print.template operator()<std::string >(out[i], "<str>"  , colname); }
        }
        // pretty format
        for (int i = 0, n = n_cols; i < n; ++i) {
            for (int j = 0, m = out[i].size(); j < m; ++j) { max_size[i] = std::max(max_size[i], out[i][j].size()); }
        }
        for (int i = 0, n = n_cols; i < n; ++i) {
            for (int j = 0, m = out[i].size(); j < m; ++j) {
                out[i][j].append(max_size[i] - out[i][j].size() + 1, ' ');
            }
        }
	// send to output stream
        for (int j = 0, m = out[0].size(); j < m - 1; ++j) {
            for (int i = 0, n = n_cols; i < n; ++i) { os << out[i][j]; }
            os << std::endl;
        }
        for (int i = 0, n = n_cols; i < n; ++i) { os << out[i][out[0].size() - 1]; }
        return os;
    }
   private:
    // internal reading utility
    template <typename T, typename ParsedFile>
    void load_table_(const std::vector<std::string>& colnames, const ParsedFile& parsed_file) {
        std::vector<std::pair<std::string, std::vector<T>>> data;
        for (const std::string& n : colnames) { data.emplace_back(n, std::vector<T> {}); }
        int i = 0, n_col = parsed_file.colnames().size();
        for (const T& s : parsed_file.data()) {
            data[i].second.push_back(s);
            i = (i + 1) % n_col;
        }
        data_ = storage_t(data);
        n_rows_ = data_.rows();
	return;
    }

    storage_t data_;
    geo_storage_t geo_data_;
    triangulation_t triangulation_;
    int n_rows_;
    std::array<int, Order> strides_;   // for unstructured layers: array of 1s
    std::array<int, Order> extents_;
    bool is_structured_;
};

}   // namespace fdapde

#endif // __FDAPDE_GEO_LAYER_H__
