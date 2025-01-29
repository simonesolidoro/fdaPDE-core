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

#ifndef __FDAPDE_GEOFRAME_MODULE_H__
#define __FDAPDE_GEOFRAME_MODULE_H__

// clang-format off

#include <charconv>
#include <filesystem>
#include <fstream>

// include required modules
#include "linear_algebra.h"    // pull Eigen first
#include "utility.h"
#include "geometry.h"

// input/outupt
#include "src/geoframe/batched_istream.h"
#include "src/geoframe/parsing.h"
#include "src/geoframe/csv.h"
#include "src/geoframe/shp.h"
#include "src/geoframe/txt.h"

// define basic symbols
namespace fdapde {

struct areal_layer_tag {} gf_areal;
struct point_layer_tag {} gf_point;
enum class ltype : int { point = 0, areal = 1 };
static constexpr int gf_mesh_nodes = -1;
  
}   // namespace fdapde

// data structure
#include "src/geoframe/data_layer.h"
#include "src/geoframe/areal_layer.h"
#include "src/geoframe/point_layer.h"
#include "src/geoframe/geo_layer.h"
#include "src/geoframe/geoframe.h"


// clang-format on

#endif   // __FDAPDE_FIELDS_MODULE_H__
