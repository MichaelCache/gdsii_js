#include "binding_utils.h"
#include "gdstk.h"

// #ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
using namespace emscripten;
// #endif

void gdstk_base_bind();
void gdstk_repetition_bind();
void gdstk_polygon_bind();
void gdstk_curve_bind();
void gdstk_flexpath_bind();
void gdstk_label_bind();
void gdstk_cell_bind();
void gdstk_reference_bind();
void gdstk_library_bind();
void gdstk_function_bind();

#define GDSTK_JS_VERSION_MAJOR 0
#define GDSTK_JS_VERSION_MINOR 9
#define GDSTK_JS_VERSION_PATCH 0

// #ifdef __EMSCRIPTEN__
EMSCRIPTEN_BINDINGS(Gdstk)
{
  gdstk_base_bind();
  gdstk_repetition_bind();
  gdstk_polygon_bind();
  gdstk_curve_bind();
  gdstk_flexpath_bind();
  gdstk_label_bind();
  gdstk_cell_bind();
  gdstk_reference_bind();
  gdstk_library_bind();
  gdstk_function_bind();
}
// #endif