#include <unordered_map>

#include "binding_utils.h"
#include "gdstk.h"
#include "gdstk_base_bind.h"

namespace {

Vec2 eval_parametric_vec2(double u, val *function) {
  auto r = function->operator()(u);
  return to_vec2(r);
}

void curve_interpolation(Curve &self, const val &points,
                         const val &js_angles = val::null(),
                         const val &tension_in = val(1),
                         const val &tension_out = val(1),
                         double initial_curl = 1, double final_curl = 1,
                         bool cycle = false, bool relative = false) {
  auto array = to_array_vec2(points);
  const uint64_t count = array->count;
  Vec2 *tension = (Vec2 *)gdstk::allocate(
      (sizeof(Vec2) + sizeof(double) + sizeof(bool)) * (count + 1));
  double *angles = (double *)(tension + (count + 1));
  bool *angle_constraints = (bool *)(angles + (count + 1));

  if (js_angles.isNull()) {
    memset(angle_constraints, 0, sizeof(bool) * (count + 1));
  } else {
    if (js_angles["length"].as<int>() != count + 1) {
      free_allocation(tension);
      throw std::runtime_error(
          "Argument angles must be None or a sequence "
          "with count len(points) + 1.");
    }
    for (uint64_t i = 0; i < count + 1; i++) {
      auto item = js_angles[i];
      if (item.isNull())
        angle_constraints[i] = false;
      else {
        angle_constraints[i] = true;
        angles[i] = item.as<double>();
      }
    }
  }

  if (tension_in.isNumber()) {
    double t_in = tension_in.as<double>();
    Vec2 *t = tension;
    for (uint64_t i = 0; i < count + 1; i++) (t++)->u = t_in;
  } else if (tension_in.isArray()) {
    if (tension_in["length"].as<int>() != count + 1) {
      free_allocation(tension);
      throw std::runtime_error(
          "Argument tension_in must be a number or a "
          "sequence with count len(points) + 1.");
    }
    for (uint64_t i = 0; i < count + 1; i++) {
      auto item = tension_in[i];
      tension[i].u = item.as<double>();
    }
  } else {
    Vec2 *t = tension;
    for (uint64_t i = 0; i < count + 1; i++) (t++)->u = 1;
  }

  if (tension_out.isNumber()) {
    double t_out = tension_out.as<double>();
    Vec2 *t = tension;
    for (uint64_t i = 0; i < count + 1; i++) (t++)->v = t_out;
  } else if (tension_out.isArray()) {
    if (tension_out["length"].as<int>() != count + 1) {
      free_allocation(tension);
      throw std::runtime_error(
          "Argument tension_out must be a number or a "
          "sequence with count len(points) + 1.");
    }
    for (uint64_t i = 0; i < count + 1; i++) {
      auto item = tension_out[i];

      tension[i].v = item.as<double>();
    }
  } else {
    Vec2 *t = tension;
    for (uint64_t i = 0; i < count + 1; i++) (t++)->v = 1;
  }

  self.interpolation(*array, angles, angle_constraints, tension, initial_curl,
                     final_curl, cycle > 0, relative > 0);

  free_allocation(tension);
}

}  // namespace

void gdstk_curve_bind() {
  class_<Curve>("Curve")
      .smart_ptr<std::shared_ptr<Curve>>("Curve_shared_ptr")
      .constructor(optional_override([](const val &xy) {
        auto curve = std::shared_ptr<Curve>(
            (Curve *)gdstk::allocate_clear(sizeof(Curve)),
            utils::CurveDeleter());
        curve->append(to_vec2(xy));
        curve->tolerance = 0.01;
        return curve;
      }))
      .constructor(optional_override([](const val &xy, double tolerance) {
        auto curve = std::shared_ptr<Curve>(
            (Curve *)gdstk::allocate_clear(sizeof(Curve)),
            utils::CurveDeleter());
        curve->append(to_vec2(xy));
        if (tolerance <= 0) {
          throw std::runtime_error("Tolerance must be positive.");
        }
        curve->tolerance = tolerance;
        return curve;
      }))
      .property("tolerance", &Curve::tolerance)
      // .property("points", optional_override([](const Curve &self) {
      //             auto array = std::shared_ptr<Array<Vec2>>(
      //                 &const_cast<Curve &>(self).point_array,
      //                 utils::nodelete());
      //             return arrayref_to_js_proxy(array);
      //           }))
      .function("points", optional_override([](const Curve &self) {
                  return utils::gdstk_array2js_array_by_value(self.point_array);
                }))
      // TODO: refactor functions below
      .function(
          "horizontal",
          optional_override([](Curve &self, const val &x, bool relative) {
            if (x.isNumber()) {
              self.horizontal(x.as<double>(), relative);
            } else if (x.isArray()) {
              auto points = utils::js_array2gdstk_array<double>(x);
              self.horizontal(*points, relative);
            } else {
              throw std::runtime_error("x must be number of array of number");
            }
          }))
      .function(
          "horizontal", optional_override([](Curve &self, const val &x) {
            bool relative = false;
            if (x.isNumber()) {
              self.horizontal(x.as<double>(), relative);
            } else if (x.isArray()) {
              auto points = utils::js_array2gdstk_array<double>(x);
              self.horizontal(*points, relative);
            } else {
              throw std::runtime_error("x must be number of array of number");
            }
          }))
      .function(
          "vertical",
          optional_override([](Curve &self, const val &y, bool relative) {
            if (y.isNumber()) {
              self.vertical(y.as<double>(), relative);
            } else if (y.isArray()) {
              auto points = utils::js_array2gdstk_array<double>(y);
              self.vertical(*points, relative);
            } else {
              throw std::runtime_error("y must be number of array of number");
            }
          }))
      .function(
          "vertical", optional_override([](Curve &self, const val &y) {
            bool relative = false;
            if (y.isNumber()) {
              self.vertical(y.as<double>(), relative);
            } else if (y.isArray()) {
              auto points = utils::js_array2gdstk_array<double>(y);
              self.vertical(*points, relative);
            } else {
              throw std::runtime_error("y must be number of array of number");
            }
          }))
      .function("segment", optional_override(
                               [](Curve &self, const val &xy, bool relative) {
                                 if (xy[0].isNumber()) {
                                   auto point = to_vec2(xy);
                                   self.segment(point, relative);
                                 } else {
                                   auto points = to_array_vec2(xy);
                                   self.segment(*points, relative);
                                 }
                               }))
      .function("segment", optional_override([](Curve &self, const val &xy) {
                  bool relative = false;
                  if (xy[0].isNumber()) {
                    auto point = to_vec2(xy);
                    self.segment(point, relative);
                  } else {
                    auto points = to_array_vec2(xy);
                    self.segment(*points, relative);
                  }
                }))
      .function("cubic", optional_override(
                             [](Curve &self, const val &xy, bool relative) {
                               auto points = to_array_vec2(xy);
                               self.cubic(*points, relative);
                             }))
      .function("cubic", optional_override([](Curve &self, const val &xy) {
                  bool relative = false;
                  auto points = to_array_vec2(xy);
                  self.cubic(*points, relative);
                }))
      .function("cubic_smooth", optional_override([](Curve &self, const val &xy,
                                                     bool relative) {
                  auto points = to_array_vec2(xy);
                  self.cubic_smooth(*points, relative);
                }))
      .function("cubic_smooth",
                optional_override([](Curve &self, const val &xy) {
                  bool relative = false;
                  auto points = to_array_vec2(xy);
                  self.cubic_smooth(*points, relative);
                }))
      .function("quadratic", optional_override(
                                 [](Curve &self, const val &xy, bool relative) {
                                   auto points = to_array_vec2(xy);
                                   self.quadratic(*points, relative);
                                 }))
      .function("quadratic", optional_override([](Curve &self, const val &xy) {
                  bool relative = false;
                  auto points = to_array_vec2(xy);
                  self.quadratic(*points, relative);
                }))
      .function(
          "quadratic_smooth",
          optional_override([](Curve &self, const val &xy, bool relative) {
            if (xy[0].isNumber()) {
              auto point = to_vec2(xy);
              self.quadratic_smooth(point, relative);
            } else {
              auto points = to_array_vec2(xy);
              self.quadratic_smooth(*points, relative);
            }
          }))
      .function("quadratic_smooth",
                optional_override([](Curve &self, const val &xy) {
                  bool relative = false;
                  if (xy[0].isNumber()) {
                    auto point = to_vec2(xy);
                    self.quadratic_smooth(point, relative);
                  } else {
                    auto points = to_array_vec2(xy);
                    self.quadratic_smooth(*points, relative);
                  }
                }))
      .function("bezier", optional_override(
                              [](Curve &self, const val &xy, bool relative) {
                                auto points = to_array_vec2(xy);
                                self.bezier(*points, relative);
                              }))
      .function("bezier", optional_override([](Curve &self, const val &xy) {
                  bool relative = false;
                  auto points = to_array_vec2(xy);
                  self.quadratic(*points, relative);
                }))
      .function(
          "interpolation",
          optional_override([](Curve &self, const val &points,
                               const val &angles, const val &tension_in,
                               const val &tension_out, double initial_curl,
                               double final_curl, bool cycle, bool relative) {
            return curve_interpolation(self, points, angles, tension_in,
                                       tension_out, initial_curl, final_curl,
                                       cycle, relative);
          }))
      .function("interpolation",
                optional_override([](Curve &self, const val &points) {
                  return curve_interpolation(self, points);
                }))
      .function(
          "arc", optional_override([](Curve &self, const val &radius,
                                      double initial_angle, double final_angle,
                                      double rotation) {
            double radius_x;
            double radius_y;
            if (radius.isNumber()) {
              radius_x = radius_y = radius.as<double>();
            } else if (radius.isArray()) {
              if (radius["length"].as<int>() != 2) {
                throw std::runtime_error(
                    "Argument radius must be a number of a sequence of 2 "
                    "numbers.");
              }
              radius_x = radius[0].as<double>();
              radius_y = radius[1].as<double>();
            }
            if (radius_x <= 0 || radius_y <= 0) {
              throw std::runtime_error("Arc radius must be positive.");
            }
            self.arc(radius_x, radius_y, initial_angle, final_angle, rotation);
          }))
      .function("arc",
                optional_override([](Curve &self, const val &radius,
                                     double initial_angle, double final_angle) {
                  double rotation = 0;
                  double radius_x;
                  double radius_y;
                  if (radius.isNumber()) {
                    radius_x = radius_y = radius.as<double>();
                  } else if (radius.isArray()) {
                    if (radius["length"].as<int>() != 2) {
                      throw std::runtime_error(
                          "Argument radius must be a number of a sequence of 2 "
                          "numbers.");
                    }
                    radius_x = radius[0].as<double>();
                    radius_y = radius[1].as<double>();
                  }
                  if (radius_x <= 0 || radius_y <= 0) {
                    throw std::runtime_error("Arc radius must be positive.");
                  }
                  self.arc(radius_x, radius_y, initial_angle, final_angle,
                           rotation);
                }))
      .function("turn",
                optional_override([](Curve &self, double radius, double angle) {
                  if (radius <= 0) {
                    throw std::runtime_error("Turn radius must be positive.");
                  }
                  self.turn(radius, angle);
                }))
      .function("parametric",
                optional_override(
                    [](Curve &self, const val &curve_function, bool relative) {
                      utils::CURVE_FUNC_SET.erase(&self);
                      utils::CURVE_FUNC_SET.emplace(&self, curve_function);
                      void *func = (void *)(&(utils::CURVE_FUNC_SET.at(&self)));
                      self.parametric((ParametricVec2)eval_parametric_vec2,
                                      func, relative);
                    }))
      .function("parametric",
                optional_override([](Curve &self, const val &curve_function) {
                  bool relative = true;
                  utils::CURVE_FUNC_SET.erase(&self);
                  utils::CURVE_FUNC_SET.emplace(&self, curve_function);
                  void *func = (void *)(&(utils::CURVE_FUNC_SET.at(&self)));
                  self.parametric((ParametricVec2)eval_parametric_vec2, func,
                                  relative);
                }))
      .function("commands",
                optional_override([](Curve &self, const val &path_commands) {
                  auto count = path_commands["length"].as<int>();
                  CurveInstruction *instructions =
                      (CurveInstruction *)gdstk::allocate_clear(
                          sizeof(CurveInstruction) * count * 2);
                  CurveInstruction *instr = instructions;
                  for (uint64_t i = 0; i < count; i++) {
                    auto item = path_commands[i];
                    if (item.isString()) {
                      std::string command = item.as<std::string>();
                      (instr++)->command = command[0];
                    } else if (item.isNumber()) {
                      (instr++)->number = item.as<double>();
                    } else {
                      gdstk::free_allocation(instructions);
                      throw std::runtime_error("Not valid command or number");
                    }
                  }

                  uint64_t instr_size = instr - instructions;
                  uint64_t processed = self.commands(instructions, instr_size);
                  if (processed < instr_size) {
                    auto error = std::string("Error parsing argument ") +
                                 std::to_string(processed) +
                                 " in curve construction.";
                    throw std::runtime_error(error);
                  }

                  gdstk::free_allocation(instructions);
                }))
#ifndef NDEBUG
      .function("print", &Curve::print)
#endif
      ;
}
