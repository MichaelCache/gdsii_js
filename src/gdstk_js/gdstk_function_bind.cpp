#include <iostream>
#include <memory>

#include "binding_utils.h"
#include "gdstk_base_bind.h"

// js function for upload gds file, save file to wasm memory and return name to
// C++ as val type
EM_JS(void, upload_file, (), {
  var upload_input = document.getElementById('upload_gds');
  if (upload_input == null) {
    upload_input = document.createElement('input');
    upload_input.setAttribute('type', 'file');
    upload_input.setAttribute('id', 'upload_gds');
    upload_input.setAttribute('accept', '.gds');
    upload_input.style.display = 'none';
    upload_input.onchange = () => {
      let files = document.getElementById('upload_gds').files;
      let file = files[0];
      let filename = file.name;
      console.log(`Upload "${filename}" complete`);

      let reader = new FileReader();
      reader.onload = (e) => {
        let result = reader.result;
        const uint8_view = new Uint8Array(result);
        FS.writeFile(filename, uint8_view);
      };
      reader.readAsArrayBuffer(file);
    };
    document.body.appendChild(upload_input);
  }
  upload_input.click();
});

namespace {
static void parse_polygons(const val &polygons,
                           Array<Polygon *> &polygon_array) {
  auto cons = polygons["constructor"]["name"].as<std::string>();
  if (cons == "Polygon") {
    Polygon *polygon = (Polygon *)gdstk::allocate_clear(sizeof(Polygon));
    polygon->copy_from(*polygons.as<Polygon *>(allow_raw_pointers()));
    polygon_array.append(polygon);
  } else if (cons == "FlexPath") {
    polygons.as<FlexPath *>(allow_raw_pointers())
        ->to_polygons(false, 0, polygon_array);
  } else if (cons == "RobustPath") {
    polygons.as<RobustPath *>(allow_raw_pointers())
        ->to_polygons(false, 0, polygon_array);
  } else if (cons == "Reference") {
    polygons.as<Reference *>(allow_raw_pointers())
        ->get_polygons(true, true, -1, false, 0, polygon_array);
  } else if (polygons.isArray()) {
    auto count = polygons["length"].as<int>();
    for (int64_t i = count - 1; i >= 0; i--) {
      auto cons = polygons[i]["constructor"]["name"].as<std::string>();
      if (cons == "Polygon") {
        Polygon *polygon = (Polygon *)gdstk::allocate_clear(sizeof(Polygon));
        polygon->copy_from(*polygons[i].as<Polygon *>(allow_raw_pointers()));
        polygon_array.append(polygon);
      } else if (cons == "FlexPath") {
        polygons[i]
            .as<FlexPath *>(allow_raw_pointers())
            ->to_polygons(false, 0, polygon_array);
      } else if (cons == "RobustPath") {
        polygons[i]
            .as<RobustPath *>(allow_raw_pointers())
            ->to_polygons(false, 0, polygon_array);
      } else if (cons == "Reference") {
        polygons[i]
            .as<Reference *>(allow_raw_pointers())
            ->get_polygons(true, true, -1, false, 0, polygon_array);
      } else {
        std::string error = "Unable to parse item " + cons + " from sequnce " +
                            std::to_string(i);
        throw std::runtime_error(error);
      }
    }
  } else {
    throw std::runtime_error(
        "Argument must be a Polygon, FlexPath, RobustPath, References. "
        "It can also be a sequence where each item is one of those or a "
        "sequence of points.");
  }
}
std::shared_ptr<Polygon> make_rectangle(const val &corner1, const val &corner2,
                                        int layer = 0, int datatype = 0) {
  auto result = std::shared_ptr<Polygon>(
      (Polygon *)gdstk::allocate_clear(sizeof(Polygon)),
      utils::PolygonDeleter());
  *result = gdstk::rectangle(to_vec2(corner1), to_vec2(corner2),
                             gdstk::make_tag(layer, datatype));
  return result;
}

std::shared_ptr<Polygon> make_cross(const val &center, double full_size,
                                    double arm_width, int layer = 0,
                                    int datatype = 0) {
  auto result = std::shared_ptr<Polygon>(
      (Polygon *)gdstk::allocate_clear(sizeof(Polygon)),
      utils::PolygonDeleter());
  *result = gdstk::cross(to_vec2(center), full_size, arm_width,
                         gdstk::make_tag(layer, datatype));
  return result;
}
std::shared_ptr<Polygon> make_regular_polygon(const val &center,
                                              double side_length, int sides,
                                              double rotation = 0,
                                              int layer = 0, int datatype = 0) {
  auto result = std::shared_ptr<Polygon>(
      (Polygon *)gdstk::allocate_clear(sizeof(Polygon)),
      utils::PolygonDeleter());
  *result = gdstk::regular_polygon(to_vec2(center), side_length, sides,
                                   rotation, gdstk::make_tag(layer, datatype));
  return result;
}

std::shared_ptr<Polygon> make_ellipse(const val &center, const val &radius,
                                      const val &inner_radius = val::null(),
                                      double initial_angle = 0,
                                      double final_angle = 0,
                                      double tolerance = 0.01, int layer = 0,
                                      int datatype = 0) {
  double radius_x = 0;
  double radius_y = 0;
  if (radius.isNumber()) {
    radius_x = radius.as<double>();
    radius_y = radius.as<double>();
  } else if (radius.isArray()) {
    radius_x = to_vec2(radius).x;
    radius_y = to_vec2(radius).y;
  } else {
    throw std::runtime_error(
        "Argument radius shoulde be number or points like array");
  }

  if (radius_x <= 0 || radius_y <= 0) {
    throw std::runtime_error("Ellipse radius must be positive.");
  }

  double inner_radius_x = -1;
  double inner_radius_y = -1;
  if (!inner_radius.isNull()) {
    if (inner_radius.isNumber()) {
      inner_radius_x = inner_radius.as<double>();
      inner_radius_y = inner_radius.as<double>();
    } else if (inner_radius.isArray()) {
      inner_radius_x = to_vec2(inner_radius).x;
      inner_radius_y = to_vec2(inner_radius).y;
    } else {
      throw std::runtime_error(
          "Argument inner_radius shoulde be number or points like "
          "array or null");
    }
  }

  if (tolerance <= 0) {
    throw std::runtime_error("Tolerance must be positive.");
  }

  auto result = std::shared_ptr<Polygon>(
      (Polygon *)gdstk::allocate_clear(sizeof(Polygon)),
      utils::PolygonDeleter());
  *result = gdstk::ellipse(to_vec2(center), radius_x, radius_y, inner_radius_x,
                           inner_radius_y, initial_angle, final_angle,
                           tolerance, gdstk::make_tag(layer, datatype));
  return result;
}

std::shared_ptr<Polygon> make_racetrack(const val &center,
                                        double straight_length, double radius,
                                        double inner_radius = 0,
                                        bool vertical = false,
                                        double tolerance = 0.01, int layer = 0,
                                        int datatype = 0) {
  if (radius <= 0) {
    throw std::runtime_error("Radius must be positive.");
  }
  if (tolerance <= 0) {
    throw std::runtime_error("Tolerance must be positive.");
  }
  if (straight_length < 0) {
    throw std::runtime_error("Argument straight_length cannot be negative.");
  }
  auto result = std::shared_ptr<Polygon>(
      (Polygon *)gdstk::allocate_clear(sizeof(Polygon)),
      utils::PolygonDeleter());
  *result =
      gdstk::racetrack(to_vec2(center), straight_length, radius, inner_radius,
                       vertical, tolerance, gdstk::make_tag(layer, datatype));
  return result;
}

val make_text(const val &s, double size, const val &position,
              bool vertical = false, int layer = 0, int datatype = 0) {
  Array<Polygon *> result = {0};
  gdstk::text(s.as<std::string>().c_str(), size, to_vec2(position), vertical,
              gdstk::make_tag(layer, datatype), result);
  auto r = utils::gdstk_array2js_array_by_ref(result, utils::PolygonDeleter());
  result.clear();
  return r;
}

val make_contour(const val &data, double level = 0, double length_scale = 1,
                 double precision = 0.01, int layer = 0, int datatype = 0) {
  assert(data.isArray());
  Array<Array<double>> matrix = {0};
  auto rows = data["length"].as<int>();
  auto cols = 0;
  for (size_t i = 0; i < rows; i++) {
    Array<double> m_row = {0};

    cols = (cols == 0) ? data[i]["length"].as<int>() : cols;
    for (size_t j = 0; j < cols; j++) {
      m_row.append(data[i][j].as<double>());
    }
    matrix.append(m_row);
  }

  Array<Polygon *> result_array = {0};
  gdstk::contour(matrix.items->items, rows, cols, level,
                 length_scale / precision, result_array);

  for (size_t i = 0; i < matrix.count; i++) {
    matrix[i].clear();
  }
  matrix.clear();

  Tag tag = gdstk::make_tag(layer, datatype);
  const Vec2 scale = {length_scale, length_scale};
  const Vec2 center = {0, 0};
  for (size_t i = 0; i < result_array.count; i++) {
    result_array[i]->scale(scale, center);
    result_array[i]->tag = tag;
  }

  auto r =
      utils::gdstk_array2js_array_by_ref(result_array, utils::PolygonDeleter());
  result_array.clear();
  return r;
}

val make_offset(const val &polygons, double distance,
                const val &join = val("miter"), double tolerance = 2,
                double precision = 1e-3, bool use_union = false, int layer = 0,
                int datatype = 0) {
  assert(join.isString());
  auto join_str = join.as<std::string>();
  OffsetJoin offset_join = OffsetJoin::Miter;
  if (join_str == "bevel")
    offset_join = OffsetJoin::Bevel;
  else if (join_str == "round")
    offset_join = OffsetJoin::Round;
  else {
    throw std::runtime_error(
        "Argument join must be one of 'miter', 'bevel', or 'round'.");
  }

  if (tolerance <= 0) {
    throw std::runtime_error("Tolerance must be positive.");
  }

  if (precision <= 0) {
    throw std::runtime_error("Precision must be positive.");
  }

  Array<Polygon *> polygon_array = {0};
  parse_polygons(polygons, polygon_array);

  Array<Polygon *> result_array = {0};
  gdstk::offset(polygon_array, distance, offset_join, tolerance, 1 / precision,
                use_union, result_array);
  Tag tag = gdstk::make_tag(layer, datatype);
  for (size_t i = 0; i < result_array.count; i++) {
    result_array[i]->tag = tag;
  }

  for (size_t i = 0; i < polygon_array.count; i++) {
    polygon_array[i]->clear();
  }
  polygon_array.clear();

  auto r =
      utils::gdstk_array2js_array_by_ref(result_array, utils::PolygonDeleter());
  result_array.clear();
  return r;
}

val make_boolean(const val &operand1, const val &operand2, const val &operation,
                 double precision = 1e-3, int layer = 0, int datatype = 0) {
  assert(operation.isString());
  auto operation_str = operation.as<std::string>();
  Operation oper;
  if (operation_str == "or")
    oper = Operation::Or;
  else if (operation_str == "and")
    oper = Operation::And;
  else if (operation_str == "xor")
    oper = Operation::Xor;
  else if (operation_str == "not")
    oper = Operation::Not;
  else {
    throw std::runtime_error(
        "Argument operation must be one of 'or', 'and', "
        "'xor', or 'not'.");
  }

  Array<Polygon *> polygon_array1 = {0};
  Array<Polygon *> polygon_array2 = {0};
  parse_polygons(operand1, polygon_array1);
  parse_polygons(operand2, polygon_array2);

  Array<Polygon *> result_array = {0};
  boolean(polygon_array1, polygon_array2, oper, 1 / precision, result_array);

  for (size_t i = 0; i < polygon_array1.count; i++) {
    polygon_array1[i]->clear();
  }
  polygon_array1.clear();
  for (size_t i = 0; i < polygon_array2.count; i++) {
    polygon_array2[i]->clear();
  }
  polygon_array2.clear();

  Tag tag = gdstk::make_tag(layer, datatype);
  for (uint64_t i = 0; i < result_array.count; i++) {
    result_array[i]->tag = tag;
  }

  auto r =
      utils::gdstk_array2js_array_by_ref(result_array, utils::PolygonDeleter());
  result_array.clear();
  return r;
}

void parse_tag_sequence(const val &js_array, gdstk::Set<Tag> &dest) {
  assert(js_array.isArray());
  int64_t count = js_array["length"].as<int64_t>();
  for (size_t i = 0; i < count; i++) {
    auto item = js_array[i];
    assert(item.isArray());
    uint32_t layer = item[0].as<uint32_t>();
    uint32_t type = item[1].as<uint32_t>();
    dest.add(gdstk::make_tag(layer, type));
  }
}

void regist_reference(
    Cell *cell,
    const std::unordered_map<Cell *, std::shared_ptr<Cell>> &cell_table) {
  auto &ref_array = cell->reference_array;
  for (size_t i = 0; i < ref_array.count; i++) {
    Reference *reference = ref_array[i];
    if (reference->type == ReferenceType::Cell) {
      auto ref_cell = cell_table.at(reference->cell);
      utils::REF_KEEP_ALIVE_CELL[reference] = ref_cell;
      regist_reference(ref_cell.get(), cell_table);
    } else if (reference->type == ReferenceType::RawCell) {
      throw std::runtime_error(
          "regist Reference refer to RawCell not implemented yet");
    }
  }
}

void regist_cell(Cell *cell) {
  utils::CELL_KEEP_ALIVE_GEOM[cell];
  auto &poly_array = cell->polygon_array;
  for (size_t i = 0; i < poly_array.count; i++) {
    utils::CELL_KEEP_ALIVE_GEOM[cell].polygons.insert(
        {poly_array[i],
         std::shared_ptr<Polygon>(poly_array[i], utils::PolygonDeleter())});
  }

  auto &ref_array = cell->reference_array;
  for (size_t i = 0; i < ref_array.count; i++) {
    utils::CELL_KEEP_ALIVE_GEOM[cell].references.insert(
        {ref_array[i],
         std::shared_ptr<Reference>(ref_array[i], utils::ReferenceDeleter())});
  }

  auto &flex_array = cell->flexpath_array;
  for (size_t i = 0; i < flex_array.count; i++) {
    utils::CELL_KEEP_ALIVE_GEOM[cell].flexpaths.insert(
        {flex_array[i],
         std::shared_ptr<FlexPath>(flex_array[i], utils::FlexPathDeleter())});
  }

  auto &robust_array = cell->robustpath_array;
  for (size_t i = 0; i < robust_array.count; i++) {
    utils::CELL_KEEP_ALIVE_GEOM[cell].robustpaths.insert(
        {robust_array[i], std::shared_ptr<RobustPath>(
                              robust_array[i], utils::RobustPathDeleter())});
  }

  auto &label_array = cell->label_array;
  for (size_t i = 0; i < label_array.count; i++) {
    utils::CELL_KEEP_ALIVE_GEOM[cell].labels.insert(
        {label_array[i],
         std::shared_ptr<Label>(label_array[i], utils::LabelDeleter())});
  }
}

void regist_rawcell(RawCell *cell) {
  throw std::runtime_error("regits_rawcell not implemented");
}

void regist_lib(Library *library) {
  utils::LIB_KEEP_ALIVE_CELL[library];
  utils::LIB_KEEP_ALIVE_RAWCELL[library];
  auto &cell_array = library->cell_array;
  std::unordered_map<Cell *, std::shared_ptr<Cell>> cell_ptr_table;
  for (size_t i = 0; i < cell_array.count; i++) {
    auto cell_ptr = std::shared_ptr<Cell>(cell_array[i], utils::CellDeleter());
    utils::LIB_KEEP_ALIVE_CELL[library].insert(cell_ptr);
    cell_ptr_table[cell_array[i]] = cell_ptr;
    regist_cell(cell_array[i]);
  }

  auto &rawcell_array = library->rawcell_array;
  for (size_t i = 0; i < rawcell_array.count; i++) {
    utils::LIB_KEEP_ALIVE_RAWCELL[library].insert(
        std::shared_ptr<RawCell>(rawcell_array[i], utils::RawCellDeleter()));
    regist_rawcell(rawcell_array[i]);
  }

  // regist reference must after all cells be registed
  for (size_t i = 0; i < cell_array.count; i++) {
    regist_reference(cell_array[i], cell_ptr_table);
  }
}
}  // namespace

// ----------------------------------------------------------------------------

void gdstk_function_bind() {
  function("rectangle",
           optional_override([](const val &corner1, const val &corner2,
                                int layer, int datatype) {
             return make_rectangle(corner1, corner2, layer, datatype);
           }));
  function("rectangle",
           optional_override([](const val &corner1, const val &corner2) {
             return make_rectangle(corner1, corner2);
           }));
  function("cross",
           optional_override([](const val &center, double full_size,
                                double arm_width, int layer, int datatype) {
             return make_cross(center, full_size, arm_width, layer, datatype);
           }));
  function("cross", optional_override([](const val &center, double full_size,
                                         double arm_width) {
             return make_cross(center, full_size, arm_width);
           }));
  function(
      "regular_polygon",
      optional_override([](const val &center, double side_length, int sides,
                           double rotation, int layer, int datatype) {
        return make_regular_polygon(center, side_length, sides, rotation, layer,
                                    datatype);
      }));
  function(
      "regular_polygon",
      optional_override([](const val &center, double side_length, int sides) {
        return make_regular_polygon(center, side_length, sides);
      }));
  function("ellipse",
           optional_override([](const val &center, const val &radius,
                                const val &inner_radius, double initial_angle,
                                double final_angle, double tolerance, int layer,
                                int datatype) {
             return make_ellipse(center, radius, inner_radius, initial_angle,
                                 final_angle, tolerance, layer, datatype);
           }));
  function("ellipse",
           optional_override([](const val &center, const val &radius) {
             return make_ellipse(center, radius);
           }));
  function(
      "racetrack",
      optional_override([](const val &center, double straight_length,
                           double radius, double inner_radius, bool vertical,
                           double tolerance, int layer, int datatype) {
        return make_racetrack(center, straight_length, radius, inner_radius,
                              vertical, tolerance, layer, datatype);
      }));
  function("racetrack",
           optional_override(
               [](const val &center, double straight_length, double radius) {
                 return make_racetrack(center, straight_length, radius);
               }));
  function("text",
           optional_override([](const val &s, double size, const val &position,
                                bool vertical, int layer, int datatype) {
             return make_text(s, size, position, vertical, layer, datatype);
           }));
  function("text", optional_override(
                       [](const val &s, double size, const val &position) {
                         return make_text(s, size, position);
                       }));
  function("contour", optional_override(
                          [](const val &data, double level, double length_scale,
                             double precision, int layer, int datatype) {
                            return make_contour(data, level, length_scale,
                                                precision, layer, datatype);
                          }));
  function("contour", optional_override(
                          [](const val &data) { return make_contour(data); }));
  function("offset", optional_override([](const val &polygons, double distance,
                                          const val &join, double tolerance,
                                          double precision, bool use_union,
                                          int layer, int datatype) {
             return make_offset(polygons, distance, join, tolerance, precision,
                                use_union, layer, datatype);
           }));
  function("offset",
           optional_override([](const val &polygons, double distance) {
             return make_offset(polygons, distance);
           }));
  function("boolean",
           optional_override([](const val &operand1, const val &operand2,
                                const val &operation, double precision,
                                int layer, int datatype) {
             return make_boolean(operand1, operand2, operation, precision,
                                 layer, datatype);
           }));
  function("boolean",
           optional_override([](const val &operand1, const val &operand2,
                                const val &operation) {
             return make_boolean(operand1, operand2, operation);
           }));
  function(
      "slice", optional_override([](const val &polygons, const val &position,
                                    const val &axis, double precision) {
        auto axis_str = axis.as<std::string>();
        bool x_axis;
        if (axis_str == "x")
          x_axis = true;
        else if (axis_str == "y")
          x_axis = false;
        else {
          throw std::runtime_error("Argument axis must be 'x' or 'y'.");
        }

        double single_position;
        std::shared_ptr<Array<double>> positions =
            utils::make_gdstk_array<double>();
        if (position.isArray()) {
          positions = utils::js_array2gdstk_array<double>(position);
        } else if (position.isNumber()) {
          positions->append(position.as<double>());
        } else {
          throw std::runtime_error(
              "Argument position must be number or array of number");
        }

        Array<Polygon *> polygon_array = {0};
        parse_polygons(polygons, polygon_array);

        val result = val::array();

        for (uint64_t i = 0; i < polygon_array.count; i++) {
          Tag tag = polygon_array[i]->tag;
          Array<Polygon *> *slices = (Array<Polygon *> *)gdstk::allocate_clear(
              (positions->count + 1) * sizeof(Array<Polygon *>));
          // NOTE: slice should never result in an error
          slice(*polygon_array[i], *positions, x_axis, 1 / precision, slices);
          Array<Polygon *> *slice_array = slices;
          for (uint64_t s = 0; s <= positions->count; s++, slice_array++) {
            for (uint64_t j = 0; j < slice_array->count; j++) {
              slice_array->items[j]->tag = tag;
            }
            result.call<void>("push",
                              utils::gdstk_array2js_array_by_ref(
                                  *slice_array, utils::PolygonDeleter()));
            slice_array->clear();
          }
          gdstk::free_allocation(slices);
        }

        for (size_t i = 0; i < polygon_array.count; i++) {
          polygon_array[i]->clear();
        }
        polygon_array.clear();

        return result;
      }));
  function("inside",
           optional_override([](const val &points, const val &polygons) {
             auto points_array = utils::js_array2gdstk_arrayvec2(points);
             Array<Polygon *> polygons_array = {0};
             parse_polygons(polygons, polygons_array);
             bool *values =
                 (bool *)gdstk::allocate(points_array->count * sizeof(bool));
             gdstk::inside(*points_array, polygons_array, values);

             val result = val::array();
             for (uint64_t i = 0; i < points_array->count; i++) {
               result.call<void>("push", values[i]);
             }
             for (size_t i = 0; i < polygons_array.count; i++) {
               polygons_array[i]->clear();
             }
             polygons_array.clear();
             gdstk::free_allocation(values);

             return result;
           }));
  function("all_inside",
           optional_override([](const val &points, const val &polygons) {
             auto points_array = utils::js_array2gdstk_arrayvec2(points);
             Array<Polygon *> polygons_array = {0};
             parse_polygons(polygons, polygons_array);

             bool result = gdstk::all_inside(*points_array, polygons_array);

             for (size_t i = 0; i < polygons_array.count; i++) {
               polygons_array[i]->clear();
             }
             polygons_array.clear();

             return val(result);
           }));
  function("any_inside",
           optional_override([](const val &points, const val &polygons) {
             auto points_array = utils::js_array2gdstk_arrayvec2(points);
             Array<Polygon *> polygons_array = {0};
             parse_polygons(polygons, polygons_array);

             bool result = gdstk::any_inside(*points_array, polygons_array);

             for (size_t i = 0; i < polygons_array.count; i++) {
               polygons_array[i]->clear();
             }
             polygons_array.clear();
             return val(result);
           }),
           allow_raw_pointers());

  function("read_gds",
           optional_override([](const val &infile, double unit,
                                double tolerance, const val &filter) {
             if (tolerance <= 0) {
               throw std::runtime_error("Tolerance must be positive.");
             }

             gdstk::Set<Tag> shape_tags = {0};
             gdstk::Set<Tag> *shape_tags_ptr = NULL;
             if (!filter.isNull()) {
               parse_tag_sequence(filter, shape_tags);
               shape_tags_ptr = &shape_tags;
             }

             // auto filename = "read_gds.gds";
             auto filename = infile.as<std::string>();
             std::shared_ptr<Library> library = std::shared_ptr<Library>(
                 (Library *)gdstk::allocate_clear(sizeof(Library)),
                 utils::LibraryDeleter());
             ErrorCode error_code = ErrorCode::NoError;
             *library = read_gds(filename.c_str(), unit, tolerance,
                                 shape_tags_ptr, &error_code);

             shape_tags.clear();

             return library;
           }));
  function("read_gds", optional_override([](const val &infile) {
             double unit = 0;
             double tolerance = 1e-2;
             gdstk::Set<Tag> shape_tags = {0};
             gdstk::Set<Tag> *shape_tags_ptr = NULL;

             auto filename = infile.as<std::string>();

             std::shared_ptr<Library> library = std::shared_ptr<Library>(
                 (Library *)gdstk::allocate_clear(sizeof(Library)),
                 utils::LibraryDeleter());
             ErrorCode error_code = ErrorCode::NoError;
             *library = read_gds(filename.c_str(), unit, tolerance,
                                 shape_tags_ptr, &error_code);

             regist_lib(library.get());

             shape_tags.clear();

             return library;
           }));
}
