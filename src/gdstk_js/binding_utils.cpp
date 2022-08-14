#include "binding_utils.h"

#include <unordered_map>

val vec2_to_js_array(const Vec2 &vec);

// make a memery view like js array[[float,float],...] to gdstk::Array<Vec2>
const val utils::gdstk_array2js_array_by_ref(const Array<Vec2> &array) {
  throw std::runtime_error(
      "Array<Vec2> should return by value other than reference");
}

const val utils::gdstk_array2js_array_by_value(const Array<Vec2> &array) {
  auto js_array = val::array();
  for (uint64_t it = 0; it < array.count; ++it) {
    js_array.call<void>("push", vec2_to_js_array(*(array.items + it)));
  }
  return js_array;
}

std::shared_ptr<Array<Vec2>> utils::js_array2gdstk_arrayvec2(const val &array) {
  auto length = array["length"].as<size_t>();
  auto gdstk_array = make_gdstk_array<Vec2>();
  for (size_t i = 0; i < length; i++) {
    gdstk_array->append(js_array2vec2(array[i]));
  }
  return gdstk_array;
}

Vec2 utils::js_array2vec2(const val &point) {
  return {point[0].as<double>(), point[1].as<double>()};
}

// shared_ptr container -------------------------------------------------------
std::unordered_map<FlexPathElement *, val> utils::JOIN_FUNC_SET;
std::unordered_map<FlexPathElement *, val> utils::END_FUNC_SET;
std::unordered_map<FlexPathElement *, val> utils::BEND_FUNC_SET;
std::unordered_map<FlexPath *, val> utils::PATH_FUNC_SET;

std::unordered_map<Curve *, val> utils::CURVE_FUNC_SET;

std::unordered_map<Cell *, utils::GeomPtr> utils::CELL_KEEP_ALIVE_GEOM;

// containors keep reference to cell/rawcell object alive
std::unordered_map<Reference *, std::shared_ptr<Cell>>
    utils::REF_KEEP_ALIVE_CELL;
std::unordered_map<Reference *, std::shared_ptr<RawCell>>
    utils::REF_KEEP_ALIVE_RAWCELL;

std::unordered_map<Library *, std::unordered_set<std::shared_ptr<Cell>>>
    utils::LIB_KEEP_ALIVE_CELL;
std::unordered_map<Library *, std::unordered_set<std::shared_ptr<RawCell>>>
    utils::LIB_KEEP_ALIVE_RAWCELL;

// point array containor ------------------------------------------------------

// shared_ptr deleter ---------------------------------------------------------
void utils::RepetitionDeleter::operator()(Repetition *repetition) const {
  repetition->clear();
  gdstk::free_allocation(repetition);
}

void utils::PolygonDeleter::operator()(Polygon *polygon) const {
  polygon->clear();
  gdstk::free_allocation(polygon);
}

void utils::CurveDeleter::operator()(Curve *curve) const {
  CURVE_FUNC_SET.erase(curve);
  curve->clear();
  gdstk::free_allocation(curve);
}

void utils::FlexPathDeleter::operator()(FlexPath *flexpath) const {
  auto el = flexpath->elements;
  for (uint64_t j = flexpath->num_elements; j > 0; j--, el++) {
    JOIN_FUNC_SET.erase(el);
    END_FUNC_SET.erase(el);
    BEND_FUNC_SET.erase(el);
  }
  PATH_FUNC_SET.erase(flexpath);
  flexpath->clear();
  gdstk::free_allocation(flexpath);
}

void utils::RobustPathDeleter::operator()(RobustPath *flexpath) const {
  throw std::runtime_error("Not implemented yet");
}

void utils::CellDeleter::operator()(Cell *cell) const {
  utils::CELL_KEEP_ALIVE_GEOM.erase(cell);
  cell->clear();
  gdstk::free_allocation(cell);
}

void utils::RawCellDeleter::operator()(RawCell *cell) const {
  throw std::runtime_error("Not implemented yet");
}

void utils::ReferenceDeleter::operator()(Reference *reference) const {
  if (reference->cell) {
    utils::REF_KEEP_ALIVE_CELL.erase(reference);
  }
  if (reference->rawcell) {
    utils::REF_KEEP_ALIVE_RAWCELL.erase(reference);
  }

  reference->clear();
  gdstk::free_allocation(reference);
}

void utils::LabelDeleter::operator()(Label *label) const {
  label->clear();
  gdstk::free_allocation(label);
}

void utils::LibraryDeleter::operator()(Library *library) const {
  utils::LIB_KEEP_ALIVE_CELL.erase(library);
  utils::LIB_KEEP_ALIVE_CELL.erase(library);
  library->clear();
  gdstk::free_allocation(library);
}
