#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

#include "gdstk.h"

// #ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
using namespace emscripten;
template <typename Cls>
using Array = gdstk::Array<Cls>;
using Cell = gdstk::Cell;
using RawCell = gdstk::RawCell;
using Library = gdstk::Library;
using Vec2 = gdstk::Vec2;
using Polygon = gdstk::Polygon;
using Tag = gdstk::Tag;
using ErrorCode = gdstk::ErrorCode;
using Operation = gdstk::Operation;
using OffsetJoin = gdstk::OffsetJoin;
using Curve = gdstk::Curve;
using FlexPath = gdstk::FlexPath;
using FlexPathElement = gdstk::FlexPathElement;
using JoinType = gdstk::JoinType;
using EndType = gdstk::EndType;
using BendType = gdstk::BendType;
using BendFunction = gdstk::BendFunction;
using JoinFunction = gdstk::JoinFunction;
using EndFunction = gdstk::EndFunction;
using ParametricVec2 = gdstk::ParametricVec2;
using CurveInstruction = gdstk::CurveInstruction;
using RobustPath = gdstk::RobustPath;
using RobustPathElement = gdstk::RobustPathElement;
using Reference = gdstk::Reference;
using ReferenceType = gdstk::ReferenceType;
using Repetition = gdstk::Repetition;
using RepetitionType = gdstk::RepetitionType;
using CurveInstruction = gdstk::CurveInstruction;
using Label = gdstk::Label;
using Anchor = gdstk::Anchor;
using Property = gdstk::Property;
using PropertyValue = gdstk::PropertyValue;
using PropertyType = gdstk::PropertyType;

namespace utils {

// shared_ptr container -------------------------------------------------------

// *******************  WARNNING: thread race zone started  **********************

// global containor to hold emscripten val of js function
extern std::unordered_map<FlexPathElement *, val> JOIN_FUNC_SET;
extern std::unordered_map<FlexPathElement *, val> END_FUNC_SET;
extern std::unordered_map<FlexPathElement *, val> BEND_FUNC_SET;
extern std::unordered_map<FlexPath *, val> PATH_FUNC_SET;

extern std::unordered_map<Curve *, val> CURVE_FUNC_SET;
struct GeomPtr {
  std::unordered_map<Polygon *, std::shared_ptr<Polygon>> polygons;
  std::unordered_map<Reference *, std::shared_ptr<Reference>> references;
  std::unordered_map<FlexPath *, std::shared_ptr<FlexPath>> flexpaths;
  std::unordered_map<RobustPath *, std::shared_ptr<RobustPath>> robustpaths;
  std::unordered_map<Label *, std::shared_ptr<Label>> labels;
};

// containors keep cell contains polygon/path/reference object alive
extern std::unordered_map<Cell *, GeomPtr> CELL_KEEP_ALIVE_GEOM;

// containors keep reference to cell/rawcell object alive
extern std::unordered_map<Reference *, std::shared_ptr<Cell>>
    REF_KEEP_ALIVE_CELL;
extern std::unordered_map<Reference *, std::shared_ptr<RawCell>>
    REF_KEEP_ALIVE_RAWCELL;

extern std::unordered_map<Library *, std::unordered_set<std::shared_ptr<Cell>>>
    LIB_KEEP_ALIVE_CELL;
extern std::unordered_map<Library *,
                          std::unordered_set<std::shared_ptr<RawCell>>>
    LIB_KEEP_ALIVE_RAWCELL;
// *******************  WARNNING: thread race zone end  **********************


// shared_ptr deleter
// ---------------------------------------------------------
struct RepetitionDeleter {
  void operator()(Repetition *repetition) const;
};

struct PolygonDeleter {
  void operator()(Polygon *polygon) const;
};

struct CurveDeleter {
  void operator()(Curve *curve) const;
};

struct FlexPathDeleter {
  void operator()(FlexPath *flexpath) const;
};

struct RobustPathDeleter {
  void operator()(RobustPath *flexpath) const;
};

struct CellDeleter {
  void operator()(Cell *cell) const;
};

struct RawCellDeleter {
  void operator()(RawCell *cell) const;
};

struct ReferenceDeleter {
  void operator()(Reference *reference) const;
};

struct LabelDeleter {
  void operator()(Label *label) const;
};

struct LibraryDeleter {
  void operator()(Library *library) const;
};

// empty deleter for disable shared_ptr delete pointer
struct nodelete {
  template <typename T>
  void operator()(T *t) const {}
};

struct ArrayDeleter {
  template <typename T>
  void operator()(Array<T> *array) const {
    array->clear();
    delete array;
  }
};

template <typename T>
std::shared_ptr<Array<T>> make_gdstk_array() {
  return std::shared_ptr<Array<T>>(
      (Array<T> *)gdstk::allocate_clear(sizeof(Array<T>)), ArrayDeleter());
}

template <typename T>
const val gdstk_array2js_array_by_value(const Array<T> &array) {
  auto js_array = val::array();
  for (uint64_t it = 0; it < array.count; ++it) {
    js_array.call<void>("push", val(*(array.items + it)));
  }
  return js_array;
}

const val gdstk_array2js_array_by_value(const Array<Vec2> &array);

template <typename T>
const val gdstk_array2js_array_by_ref(const Array<T *> &array) {
  auto js_array = val::array();
  for (size_t i = 0; i < array.count; i++) {
    // convert raw pointer to shared_ptr to auto dellocate
    auto p = std::shared_ptr<T>(array[i]);
    js_array.call<void>("push", val(p));
  }
  return js_array;
}

template <typename T, typename Deleter>
const val gdstk_array2js_array_by_ref(const Array<T *> &array,
                                      const Deleter &deleter) {
  auto js_array = val::array();
  for (size_t i = 0; i < array.count; i++) {
    // convert raw pointer to shared_ptr
    auto p = std::shared_ptr<T>(array[i], deleter);
    js_array.call<void>("push", val(p));
  }
  return js_array;
}

// make a memery view like js array[[float,float],...] to gdstk::Array<Vec2>
const val gdstk_array2js_array_by_ref(const Array<Vec2> &array);

// convert js array data to gdstk Array by value or reference
template <typename T>
std::shared_ptr<Array<T>> js_array2gdstk_array(const val &array) {
  auto length = array["length"].as<size_t>();
  auto gdstk_array = make_gdstk_array<T>();
  for (size_t i = 0; i < length; i++) {
    gdstk_array->append(array[i].as<T>(allow_raw_pointers()));
  }
  return gdstk_array;
}

std::shared_ptr<Array<Vec2>> js_array2gdstk_arrayvec2(const val &array);

Vec2 js_array2vec2(const val &point);

static std::unordered_map<std::string, JoinType> join_table{
    {"natural", JoinType::Natural}, {"miter", JoinType::Miter},
    {"bevel", JoinType::Bevel},     {"round", JoinType::Round},
    {"smooth", JoinType::Smooth},   {"function", JoinType::Function}};

static std::unordered_map<std::string, EndType> end_table{
    {"flush", EndType::Flush},         {"round", EndType::Round},
    {"halfwidth", EndType::HalfWidth}, {"extend", EndType::Extended},
    {"smooth", EndType::Smooth},       {"function", EndType::Function}};
}  // namespace utils
