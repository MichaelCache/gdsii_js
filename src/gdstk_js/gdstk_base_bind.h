#pragma once

#include "binding_utils.h"

Vec2 to_vec2(const val& obj);
std::shared_ptr<Array<Vec2>> to_array_vec2(const val& array);

struct FlexPathElementArray;

val vec2ref_to_js_proxy(std::shared_ptr<Vec2> vec);
val arrayref_to_js_proxy(std::shared_ptr<Array<Vec2>> array);
val pathlayers_to_js_proxy(std::shared_ptr<FlexPathElementArray> elem_array);
val pathtypes_to_js_proxy(std::shared_ptr<FlexPathElementArray> elem_array);
val pathwidths_to_js_proxy(std::shared_ptr<FlexPathElementArray> elem_array);
val pathoffsets_to_js_proxy(std::shared_ptr<FlexPathElementArray> elem_array);

val vec2_to_js_array(const Vec2& vec);

struct FlexPathElementArray {
 public:
  FlexPathElementArray(FlexPathElement* elem_ptr, uint64_t length);

  uint32_t get_layer(size_t idx);
  void set_layer(size_t idx, uint32_t layer);
  uint32_t get_type(size_t idx);
  void set_type(size_t idx, uint32_t type);

  double get_width(size_t idx);
  void set_width(size_t idx, double width);
  double get_offset(size_t idx);
  void set_offset(size_t idx, double offset);

  int length() const;

 private:
  FlexPathElement* address_{nullptr};
  uint64_t length_{0};
};
