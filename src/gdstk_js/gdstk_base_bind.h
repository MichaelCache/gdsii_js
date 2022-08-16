#pragma once

#include "binding_utils.h"

extern "C" {
EM_VAL make_array_proxy(EM_VAL);
EM_VAL make_vec2_proxy(EM_VAL);
}

Vec2 to_vec2(const val& obj);
std::shared_ptr<Array<Vec2>> to_array_vec2(const val& array);

val vec2ref_to_js_proxy(std::shared_ptr<Vec2> vec);
val arrayref_to_js_proxy(std::shared_ptr<Array<Vec2>> array);

val vec2_to_js_array(const Vec2& vec);
