#include <unordered_map>

#include "binding_utils.h"
#include "gdstk_base_bind.h"

namespace {
static std::shared_ptr<Array<Vec2>> custom_end_function(
    const Vec2 first_point, const Vec2 first_direction, const Vec2 second_point,
    const Vec2 second_direction, val *function) {
  auto r = function->operator()(
      vec2_to_js_array(first_point), vec2_to_js_array(first_direction),
      vec2_to_js_array(second_point), vec2_to_js_array(second_direction));
  return utils::js_array2gdstk_arrayvec2(r);
}

static std::shared_ptr<Array<Vec2>> custom_join_function(
    const Vec2 first_point, const Vec2 first_direction, const Vec2 second_point,
    const Vec2 second_direction, const Vec2 center, double width,
    val *function) {
  auto r = function->operator()(
      vec2_to_js_array(first_point), vec2_to_js_array(first_direction),
      vec2_to_js_array(second_point), vec2_to_js_array(second_direction),
      vec2_to_js_array(center), width);
  return utils::js_array2gdstk_arrayvec2(r);
}

static std::shared_ptr<Array<Vec2>> custom_bend_function(double radius,
                                                         double initial_angle,
                                                         double final_angle,
                                                         const Vec2 center,
                                                         val *function) {
  auto r = function->operator()(radius, initial_angle, final_angle,
                                vec2_to_js_array(center));
  return utils::js_array2gdstk_arrayvec2(r);
}

Vec2 eval_parametric_vec2(double u, val *function) {
  auto r = function->operator()(u);
  return to_vec2(r);
}

// ----------------------------------------------------------------------------
void set_join(FlexPathElement *el, const val &join) {
  if (join.isString()) {
    el->join_type = utils::join_table.at(join.as<std::string>());
  } else if (join.typeOf().as<std::string>() == "function") {
    el->join_type = JoinType::Function;
    el->join_function = (JoinFunction)custom_join_function;
    utils::JOIN_FUNC_SET.erase(el);
    utils::JOIN_FUNC_SET.emplace(el, join);
    el->join_function_data = (void *)(&(utils::JOIN_FUNC_SET.at(el)));
  } else {
    throw std::runtime_error(
        "Argument joins must be one of 'natural', 'miter', 'bevel', "
        "'round', 'smooth', a callable, or a list of those.");
  }
}

void set_end(FlexPathElement *el, const val &end) {
  if (end.isString()) {
    if (end.as<std::string>() == "extend") {
      // logic from gdstk python code
      el->end_type = EndType::HalfWidth;
    } else {
      el->end_type = utils::end_table.at(end.as<std::string>());
    }
  } else if (end.typeOf().as<std::string>() == "function") {
    el->end_type = EndType::Function;
    el->end_function = (EndFunction)custom_end_function;
    utils::END_FUNC_SET.erase(el);
    utils::END_FUNC_SET.emplace(el, end);
    el->end_function_data = (void *)(&(utils::END_FUNC_SET.at(el)));
  } else if (end.isArray()) {
    // if end is a 2-array
    el->end_type = EndType::Extended;
    el->end_extensions.u = end[0].as<double>();
    el->end_extensions.v = end[1].as<double>();
  } else {
    throw std::runtime_error(
        "Argument ends must be one of 'flush', 'extended', 'round', "
        "'smooth', a 2-tuple, a callable, or a list of those.");
  }
}

void set_bend_function(FlexPathElement *el, const val &bend_func) {
  // if pass in js function
  if (bend_func.typeOf().as<std::string>() == "function") {
    el->bend_type = BendType::Function;
    el->bend_function = (BendFunction)custom_bend_function;
    utils::BEND_FUNC_SET.erase(el);
    utils::BEND_FUNC_SET.emplace(el, bend_func);
    el->bend_function_data = (void *)(&(utils::BEND_FUNC_SET.at(el)));
  } else if (bend_func.isNull()) {
  } else {
    throw std::runtime_error(
        "Argument bend_function must be a list or a callable.");
  }
}

static int parse_flexpath_width(const FlexPath &flexpath, const val &js_width,
                                double *width) {
  if (js_width.isArray()) {
    if (js_width["length"].as<int>() < flexpath.num_elements) {
      throw std::runtime_error("Sequence width doesn't have enough elements.");
    }
    for (uint64_t i = 0; i < flexpath.num_elements; i++) {
      const double value = js_width[i].as<double>();
      if (value < 0) {
        throw std::runtime_error("Negative width value not allowed");
      }
      *width++ = value;
    }
  } else {
    const double value = js_width.as<double>();
    if (value < 0) {
      throw std::runtime_error("Negative width value not allowed");
    }
    for (uint64_t i = 0; i < flexpath.num_elements; i++) *width++ = value;
  }
  return 0;
}

static int parse_flexpath_offset(const FlexPath &flexpath, const val &js_offset,
                                 double *offset) {
  if (js_offset.isArray()) {
    if (js_offset["length"].as<int>() < flexpath.num_elements) {
      throw std::runtime_error("Sequence offset doesn't have enough elements.");
    }
    for (uint64_t i = 0; i < flexpath.num_elements; i++) {
      *offset++ = js_offset[i].as<double>();
    }
  } else {
    const double value = js_offset.as<double>();
    for (uint64_t i = 0; i < flexpath.num_elements; i++)
      *offset++ = (i - 0.5 * (flexpath.num_elements - 1)) * value;
  }
  return 0;
}

static std::shared_ptr<FlexPath> make_flexpath(
    const val &points, const val &width, const val &offset = val(0),
    const val &joins = val("natural"), const val &ends = val("flush"),
    const val &bend_radius = val(0), const val &bend_function = val::null(),
    double tolerance = 1e-2, bool simple_path = false, bool scale_width = true,
    const val &layer = val(0), const val &datatype = val(0)) {
  if (tolerance <= 0) {
    throw std::runtime_error("Tolerance must be positive.");
  }

  std::shared_ptr<Array<Vec2>> points_array = utils::make_gdstk_array<Vec2>();
  if (points[0].isNumber()) {
    points_array->append(to_vec2(points));
  } else if (points[0].isArray()) {
    points_array = utils::js_array2gdstk_arrayvec2(points);
  } else {
    throw std::runtime_error(
        "Argument points must be point or array of points");
  }

  auto flexpath = std::shared_ptr<FlexPath>(
      (FlexPath *)gdstk::allocate_clear(sizeof(FlexPath)),
      utils::FlexPathDeleter());
  flexpath->spine.point_array.extend(*points_array);

  const uint64_t count = flexpath->spine.point_array.count;
  if (count > 1)
    flexpath->spine.last_ctrl = flexpath->spine.point_array[count - 2];

  // width and offset
  uint64_t num_elements = 1;
  if (width.isArray()) {
    num_elements = width["length"].as<int>();
    flexpath->num_elements = num_elements;
    flexpath->elements = (FlexPathElement *)gdstk::allocate_clear(
        num_elements * sizeof(FlexPathElement));
    if (!offset.isNull() && offset.isArray()) {
      if (offset["length"].as<int>() != num_elements) {
        throw std::runtime_error(
            "Sequences width and offset must have the same length.");
      }

      // Case 1: width and offset are sequences with the same length
      FlexPathElement *el = flexpath->elements;
      for (uint64_t i = 0; i < num_elements; i++, el++) {
        const double half_width = 0.5 * width[i].as<double>();
        if (half_width < 0) {
          throw std::runtime_error("Negative width value not allowed");
        }

        const double offset_value = offset[i].as<double>();

        const Vec2 half_width_and_offset = {half_width, offset_value};
        el->half_width_and_offset.ensure_slots(count);
        Vec2 *wo = el->half_width_and_offset.items;
        for (uint64_t j = 0; j < count; j++) *wo++ = half_width_and_offset;
        el->half_width_and_offset.count = count;
      }
    } else {
      // Case 2: width is a sequence, offset a number
      const double offset_value = offset.isNull() ? 0 : offset.as<double>();

      FlexPathElement *el = flexpath->elements;
      for (uint64_t i = 0; i < num_elements; i++, el++) {
        const double half_width = 0.5 * width[i].as<double>();
        if (half_width < 0) {
          throw std::runtime_error("Negative width value not allowed");
        }

        const Vec2 half_width_and_offset = {
            half_width, (i - 0.5 * (num_elements - 1)) * offset_value};
        el->half_width_and_offset.ensure_slots(count);
        Vec2 *wo = el->half_width_and_offset.items;
        for (uint64_t j = 0; j < count; j++) *wo++ = half_width_and_offset;
        el->half_width_and_offset.count = count;
      }
    }
  } else if (!offset.isNull() && offset.isArray()) {
    // Case 3: offset is a sequence, width a number
    num_elements = offset["length"].as<int>();
    flexpath->num_elements = num_elements;
    flexpath->elements = (FlexPathElement *)gdstk::allocate_clear(
        num_elements * sizeof(FlexPathElement));
    const double half_width = 0.5 * width.as<double>();
    if (half_width < 0) {
      throw std::runtime_error("Negative width value not allowed.");
    }

    FlexPathElement *el = flexpath->elements;
    for (uint64_t i = 0; i < num_elements; i++, el++) {
      const double offset_value = offset[i].as<double>();

      const Vec2 half_width_and_offset = {half_width, offset_value};
      el->half_width_and_offset.ensure_slots(count);
      Vec2 *wo = el->half_width_and_offset.items;
      for (uint64_t j = 0; j < count; j++) *wo++ = half_width_and_offset;
      el->half_width_and_offset.count = count;
    }
  } else {
    // Case 4: width and offset are numbers
    flexpath->num_elements = 1;
    flexpath->elements =
        (FlexPathElement *)gdstk::allocate_clear(sizeof(FlexPathElement));
    FlexPathElement *el = flexpath->elements;
    const double half_width = 0.5 * width.as<double>();
    if (half_width < 0) {
      throw std::runtime_error("Negative width value not allowed.");
    }
    const double offset_value = offset.isNull() ? 0 : offset.as<double>();

    const Vec2 half_width_and_offset = {half_width, offset_value};
    el->half_width_and_offset.ensure_slots(count);
    Vec2 *wo = el->half_width_and_offset.items;
    for (uint64_t j = 0; j < count; j++) *wo++ = half_width_and_offset;
    el->half_width_and_offset.count = count;
  }

  // layer
  if (!layer.isNull()) {
    if (layer.isArray()) {
      if (layer["length"].as<int>() != num_elements) {
        throw std::runtime_error(
            "List layer must have the same length as the number of paths.");
      }
      FlexPathElement *el = flexpath->elements;
      for (uint64_t i = 0; i < num_elements; i++, el++) {
        gdstk::set_layer(el->tag, layer[i].as<uint32_t>());
      }
    } else {
      const uint32_t layer_value = layer.as<uint32_t>();
      FlexPathElement *el = flexpath->elements;
      for (uint64_t i = 0; i < num_elements; i++)
        gdstk::set_layer((el++)->tag, layer_value);
    }
  }

  // datatype
  if (!datatype.isNull()) {
    if (datatype.isArray()) {
      if (datatype["length"].as<int>() != num_elements) {
        throw std::runtime_error(
            "List datatype must have the same length as the number of paths.");
      }
      FlexPathElement *el = flexpath->elements;
      for (uint64_t i = 0; i < num_elements; i++, el++) {
        gdstk::set_type(el->tag, datatype[i].as<uint32_t>());
      }
    } else {
      const uint32_t datatype_value = datatype.as<uint32_t>();
      FlexPathElement *el = flexpath->elements;
      for (uint64_t i = 0; i < num_elements; i++)
        gdstk::set_type((el++)->tag, datatype_value);
    }
  }

  // jointype
  if (!joins.isNull()) {
    if (joins.isArray()) {
      if (joins["length"].as<int>() != num_elements) {
        throw std::runtime_error(
            "List joins must have the same length as the number of paths.");
      }
      FlexPathElement *el = flexpath->elements;
      for (uint64_t i = 0; i < num_elements; i++, el++) set_join(el, joins[i]);
    } else {
      FlexPathElement *el = flexpath->elements;
      for (uint64_t i = 0; i < num_elements; i++) set_join(el, joins);
    }
  }

  // ends
  if (!ends.isNull()) {
    if (ends.isArray()) {
      if (ends["length"].as<int>() != num_elements) {
        throw std::runtime_error(
            "List ends must have the same length as the number of paths.");
      }
      FlexPathElement *el = flexpath->elements;
      for (uint64_t i = 0; i < num_elements; i++, el++) set_end(el, ends[i]);
    } else {
      FlexPathElement *el = flexpath->elements;
      for (uint64_t i = 0; i < num_elements; i++, el++) set_end(el, ends);
    }
  }

  // bend radius
  if (!bend_radius.isNull()) {
    if (bend_radius.isArray()) {
      if (bend_radius["length"].as<int>() != num_elements) {
        throw std::runtime_error(
            "Sequence bend_radius must have the same length as the "
            "number of paths.");
      }
      FlexPathElement *el = flexpath->elements;
      for (uint64_t i = 0; i < num_elements; i++, el++) {
        const double bend_radius_value = bend_radius[i].as<double>();
        if (bend_radius_value > 0) {
          el->bend_type = BendType::Circular;
          el->bend_radius = bend_radius_value;
        }
      }
    } else {
      const double bend_radius_value = bend_radius.as<double>();
      if (bend_radius_value > 0) {
        FlexPathElement *el = flexpath->elements;
        for (uint64_t i = 0; i < num_elements; i++, el++) {
          el->bend_type = BendType::Circular;
          el->bend_radius = bend_radius_value;
        }
      }
    }
  }

  // bend function
  if (!bend_function.isNull()) {
    if (bend_function.isArray()) {
      if (bend_function["length"].as<int>() != num_elements) {
        throw std::runtime_error(
            "Sequence bend_function must have the same length as "
            "the number of paths.");
      }
      FlexPathElement *el = flexpath->elements;
      for (uint64_t i = 0; i < num_elements; i++, el++)
        set_bend_function(el, bend_function[i]);
    } else {
      FlexPathElement *el = flexpath->elements;
      for (uint64_t i = 0; i < num_elements; i++, el++)
        set_bend_function(el, bend_function);
    }
  }

  flexpath->spine.tolerance = tolerance;
  flexpath->simple_path = simple_path;
  flexpath->scale_width = scale_width;

  return flexpath;
}

// ----------------------------------------------------------------------------
void flexpath_interpolation(
    FlexPath &self, const val &points, const val &js_angles = val::null(),
    const val &js_tension_in = val(1), const val &js_tension_out = val(1),
    double initial_curl = 1, double final_curl = 1, bool cycle = false,
    const val &js_width = val::null(), const val &js_offset = val::null(),
    bool relative = false) {
  Vec2 *tension;
  double *angles;
  bool *angle_constraints;

  auto point_array = utils::js_array2gdstk_arrayvec2(points);
  const uint64_t count = point_array->count;

  tension = (Vec2 *)gdstk::allocate(
      (sizeof(Vec2) + sizeof(double) + sizeof(bool)) * (count + 1));
  angles = (double *)(tension + (count + 1));
  angle_constraints = (bool *)(angles + (count + 1));

  if (js_angles.isNull()) {
    memset(angle_constraints, 0, sizeof(bool) * (count + 1));
  } else {
    if (js_angles["length"].as<int>() != count + 1) {
      gdstk::free_allocation(tension);
      throw std::runtime_error(
          "Argument angles must be None or a sequence with count "
          "len(points) + 1.");
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

  if (!js_tension_in.isNull()) {
    Vec2 *t = tension;
    for (uint64_t i = 0; i < count + 1; i++) (t++)->u = 1;
  } else if (js_tension_in.isArray()) {
    double t_in = js_tension_in.as<double>();
    Vec2 *t = tension;
    for (uint64_t i = 0; i < count + 1; i++) (t++)->u = t_in;
  } else {
    if (js_tension_in["length"].as<int>() != count + 1) {
      gdstk::free_allocation(tension);
      throw std::runtime_error(
          "Argument tension_in must be a number or a sequence with "
          "count len(points) + 1.");
    }
    for (uint64_t i = 0; i < count + 1; i++) {
      tension[i].u = js_tension_in[i].as<double>();
    }
  }

  if (!js_tension_out.isNull()) {
    Vec2 *t = tension;
    for (uint64_t i = 0; i < count + 1; i++) (t++)->v = 1;
  } else if (!js_tension_out.isArray()) {
    double t_out = js_tension_out.as<double>();
    Vec2 *t = tension;
    for (uint64_t i = 0; i < count + 1; i++) (t++)->v = t_out;
  } else {
    if (js_tension_out["length"].as<int>() != count + 1) {
      gdstk::free_allocation(tension);
      throw std::runtime_error(
          "Argument tension_out must be a number or a sequence "
          "with count len(points) + 1.");
    }
    for (uint64_t i = 0; i < count + 1; i++) {
      tension[i].v = js_tension_out[i].as<double>();
    }
  }

  double *buffer =
      (double *)gdstk::allocate(sizeof(double) * self.num_elements * 2);
  double *width = NULL;
  if (!js_width.isNull()) {
    width = buffer;
    parse_flexpath_width(self, js_width, width);
  }
  double *offset = NULL;
  if (!js_offset.isNull()) {
    offset = buffer + self.num_elements;
    parse_flexpath_offset(self, js_offset, offset);
  }

  self.interpolation(*point_array, angles, angle_constraints, tension,
                     initial_curl, final_curl, cycle, width, offset, relative);

  gdstk::free_allocation(tension);
  gdstk::free_allocation(buffer);
}

void flexpath_arc(FlexPath &self, const val &radius, double initial_angle,
                  double final_angle, double rotation = 0,
                  const val &js_width = val::null(),
                  const val &js_offset = val::null()) {
  double radius_x, radius_y;

  if (!radius.isArray()) {
    radius_x = radius_y = radius.as<double>();
  } else if (radius["length"].as<int>() != 2) {
    throw std::runtime_error(
        "Argument radius must be a number of a sequence of 2 numbers.");
  } else {
    auto item = to_vec2(radius);
    radius_x = item.x;
    radius_y = item.y;
  }

  double *buffer =
      (double *)gdstk::allocate(sizeof(double) * self.num_elements * 2);
  double *width = NULL;
  if (!js_width.isNull()) {
    width = buffer;
    parse_flexpath_width(self, js_width, width);
  }
  double *offset = NULL;
  if (!js_offset.isNull()) {
    offset = buffer + self.num_elements;
    parse_flexpath_offset(self, js_offset, offset);
  }

  if (radius_x <= 0 || radius_y <= 0) {
    gdstk::free_allocation(buffer);
    throw std::runtime_error("Arc radius must be positive.");
  }

  self.arc(radius_x, radius_y, initial_angle, final_angle, rotation, width,
           offset);
  gdstk::free_allocation(buffer);
}

}  // namespace

EM_JS(EM_VAL, make_proxy_flexpath, (EM_VAL array, EM_VAL obj), {
  var proxy = new Proxy(Emval.toValue(array), {
    self : Emval.toValue(obj),
  deleteProperty:
    function(target, property) {
      if (!isNaN(property)) {
        try{
this.self.del_point(parseInt(property));
        return true;
    } catch (err) {
  console.log(err.message);
  return false;
    }
}
delete target[property];
return true;
}
, set : function(target, property, value, receiver) {
  if (!isNaN(property)) {
    try {
      this.self.set_point(parseInt(property), value);
      target[property] = value;
      return true;
    } catch (err) {
      console.log(err.message);
      return false;
    }
  }
  target[property] = value;
  return true;
}
});
return Emval.toHandle(proxy);
});

// ----------------------------------------------------------------------------
void gdstk_flexpath_bind() {
  class_<FlexPath>("FlexPath")
      .smart_ptr<std::shared_ptr<FlexPath>>("FlexPath_share_ptr")
      .constructor(optional_override(
          [](const val &points, const val &width, const val &offset,
             const val &joins, const val &ends, const val &bend_radius,
             const val &bend_function, double tolerance, bool simple_path,
             bool scale_width, const val &layer, const val &datatype) {
            return make_flexpath(points, width, offset, joins, ends,
                                 bend_radius, bend_function, tolerance,
                                 simple_path, scale_width, layer, datatype);
          }))
      .constructor(optional_override([](const val &points, const val &width) {
        return make_flexpath(points, width);
      }))
      .property("layers", optional_override([](const FlexPath &self) {
                  val r = val::array();
                  for (uint64_t i = 0; i < self.num_elements; i++) {
                    r.call<void>("push",
                                 gdstk::get_layer(self.elements[i].tag));
                  }
                  return r;
                }))
      .property("datatypes", optional_override([](const FlexPath &self) {
                  val r = val::array();
                  for (uint64_t i = 0; i < self.num_elements; i++) {
                    r.call<void>("push", gdstk::get_type(self.elements[i].tag));
                  }
                  return r;
                }))
      .property("num_paths", optional_override([](const FlexPath &self) {
                  return int(self.num_elements);
                }))
      .property("size", optional_override([](const FlexPath &self) {
                  return int(self.spine.point_array.count);
                }))
      .property("joins", optional_override([](const FlexPath &self) {
                  val r = val::array();
                  for (uint64_t i = 0; i < self.num_elements; i++) {
                    FlexPathElement *element = self.elements + i;
                    switch (element->join_type) {
                      case JoinType::Natural:
                        r.call<void>("push", val("natural"));
                        break;
                      case JoinType::Miter:
                        r.call<void>("push", val("miter"));
                        break;
                      case JoinType::Bevel:
                        r.call<void>("push", val("bevel"));
                        break;
                      case JoinType::Round:
                        r.call<void>("push", val("round"));
                        break;
                      case JoinType::Smooth:
                        r.call<void>("push", val("smooth"));
                        break;
                      case JoinType::Function:
                        val func = *((val *)element->join_function_data);
                        r.call<void>("push", func);
                        break;
                    }
                  }
                  return r;
                }))
      .property("ends", optional_override([](const FlexPath &self) {
                  val r = val::array();
                  for (uint64_t i = 0; i < self.num_elements; i++) {
                    FlexPathElement *element = self.elements + i;
                    switch (element->end_type) {
                      case EndType::Flush:
                        r.call<void>("push", val("flush"));
                        break;
                      case EndType::Round:
                        r.call<void>("push", val("round"));
                        break;
                      case EndType::HalfWidth:
                        r.call<void>("push", val("extendend"));
                        break;
                      case EndType::Smooth:
                        r.call<void>("push", val("smooth"));
                        break;
                      case EndType::Extended:
                        r.call<void>("push", element->end_extensions);
                        break;
                      case EndType::Function:
                        val func = *((val *)element->end_function_data);
                        r.call<void>("push", func);
                        break;
                    }
                  }
                  return r;
                }))
      .property("bend_radius", optional_override([](const FlexPath &self) {
                  val r = val::array();
                  for (uint64_t i = 0; i < self.num_elements; i++) {
                    r.call<void>("push", self.elements[i].bend_radius);
                  }
                  return r;
                }))
      .property("bend_function", optional_override([](const FlexPath &self) {
                  val r = val::array();
                  for (uint64_t i = 0; i < self.num_elements; i++) {
                    FlexPathElement *element = self.elements + i;
                    if (element->bend_type == BendType::Function) {
                      val func = *((val *)(element->bend_function_data));
                      r.call<void>("push", func);
                    } else {
                      r.call<void>("push", val::null());
                    }
                  }
                  return r;
                }))
      .property("tolerance", optional_override([](const FlexPath &self) {
                  return self.spine.tolerance;
                }),
                optional_override([](FlexPath &self, double value) {
                  self.spine.tolerance = value;
                }))
      .property("simple_path", &FlexPath::simple_path)
      .property("scale_width", &FlexPath::scale_width)
      .property(
          "repetition", optional_override([](const FlexPath &self) {
            auto new_repetition = std::shared_ptr<Repetition>(
                (Repetition *)gdstk::allocate_clear(sizeof(Repetition)),
                utils::RepetitionDeleter());
            new_repetition->copy_from(self.repetition);
            return new_repetition;
          }),
          optional_override([](FlexPath &self, const val &repetition) {
            if (repetition.isNull()) {
              self.repetition.clear();
            } else if (repetition["constructor"]["name"].as<std::string>() !=
                       "Repetition") {
              throw std::runtime_error("Value must be a Repetition object.");
            }
            self.repetition.clear();
            self.repetition.copy_from(repetition.as<Repetition>());
          }))
      // points from spine
      .property("points", optional_override([](const FlexPath &self) {
                  auto point_array = std::shared_ptr<Array<Vec2>>(
                      &const_cast<FlexPath &>(self).spine.point_array,
                      utils::nodelete());
                  return arrayref_to_js_proxy(point_array);
                }),
                optional_override([](FlexPath &self, const val &new_points) {
                  auto points_array =
                      utils::js_array2gdstk_arrayvec2(new_points);
                  self.spine.point_array.clear();
                  self.spine.point_array.copy_from(*points_array);
                  // return arrayref_to_js_proxy(point_array);
                }))
      .function(
          "get_points", optional_override([](const FlexPath &self) {
            return utils::gdstk_array2js_array_by_value(self.spine.point_array);
          }))
      // TODO: export property "properties"
      .function("copy", optional_override([](const FlexPath &self) {
                  auto p = std::shared_ptr<FlexPath>(
                      (FlexPath *)gdstk::allocate_clear(sizeof(FlexPath)),
                      utils::FlexPathDeleter());
                  p->copy_from(self);
                  return p;
                }))
      .function(
          "spine", optional_override([](const FlexPath &self) {
            return utils::gdstk_array2js_array_by_value(self.spine.point_array);
          }))
      .function("path_spines", optional_override([](FlexPath &self) {
                  val r = val::array();
                  Array<Vec2> point_array = {0};
                  FlexPathElement *el = self.elements;
                  for (uint64_t i = 0; i < self.num_elements; i++) {
                    self.element_center(el++, point_array);
                    r.call<void>("push", utils::gdstk_array2js_array_by_value(
                                             point_array));
                    point_array.count = 0;
                  }
                  point_array.clear();
                  return r;
                }))
      .function("widths", optional_override([](FlexPath &self) {
                  val r = val::array();
                  auto point_count = self.spine.point_array.count;
                  auto el_count = self.num_elements;
                  for (uint64_t j = 0; j < point_count; j++) {
                    const FlexPathElement *el = self.elements;
                    val wd = val::array();
                    for (uint64_t i = 0; i < el_count; i++) {
                      wd.call<void>("push",
                                    2 * (el++)->half_width_and_offset[j].u);
                    }
                    r.call<void>("push", wd);
                  }
                  return r;
                }))
      .function("offsets", optional_override([](FlexPath &self) {
                  val r = val::array();
                  auto point_count = self.spine.point_array.count;
                  auto el_count = self.num_elements;
                  for (uint64_t j = 0; j < point_count; j++) {
                    const FlexPathElement *el = self.elements;
                    val wd = val::array();
                    for (uint64_t i = 0; i < el_count; i++) {
                      wd.call<void>("push",
                                    2 * (el++)->half_width_and_offset[j].v);
                    }
                    r.call<void>("push", wd);
                  }
                  return r;
                }))
      .function("to_polygons", optional_override([](FlexPath &self) {
                  Array<Polygon *> array = {0};
                  self.to_polygons(false, 0, array);
                  auto result = utils::gdstk_array2js_array_by_ref(
                      array, utils::PolygonDeleter());
                  array.clear();
                  return result;
                }))
      .function(
          "set_layers",
          optional_override([](FlexPath &self, const val &layers) {
            auto layers_array = utils::js_array2gdstk_array<uint32_t>(layers);
            if (layers_array->count != self.num_elements) {
              throw std::runtime_error(
                  "Length of layer sequence must match the number of paths.");
            }
            for (uint64_t i = 0; i < layers_array->count; i++) {
              gdstk::set_layer(self.elements[i].tag, (*layers_array)[i]);
            }
          }))
      .function("set_datatypes",
                optional_override([](FlexPath &self, const val &types) {
                  auto types_array =
                      utils::js_array2gdstk_array<uint32_t>(types);
                  if (types_array->count != self.num_elements) {
                    throw std::runtime_error(
                        "Length of datatype sequence must match the number of "
                        "paths.");
                  }
                  for (uint64_t i = 0; i < types_array->count; i++) {
                    gdstk::set_type(self.elements[i].tag, (*types_array)[i]);
                  }
                }))
      .function("set_joins",
                optional_override([](FlexPath &self, const val &joins) {
                  assert(joins.isArray());
                  auto join_count = joins["length"].as<int>();
                  if (join_count != self.num_elements) {
                    throw std::runtime_error(
                        "Length of sequence must match the number of paths.");
                  }
                  for (size_t i = 0; i < join_count; i++) {
                    auto join = joins[i];
                    auto el = self.elements + i;
                    set_join(el, join);
                  }
                }))
      .function("set_ends",
                optional_override([](FlexPath &self, const val &ends) {
                  assert(ends.isArray());
                  auto end_count = ends["length"].as<int>();
                  if (end_count != self.num_elements) {
                    throw std::runtime_error(
                        "Length of sequence must match the number of paths.");
                  }
                  for (size_t i = 0; i < end_count; i++) {
                    auto end = ends[i];
                    auto el = self.elements + i;
                    set_end(el, end);
                  }
                }))
      .function("set_bend_radius",
                optional_override([](FlexPath &self, const val &bend_radius) {
                  assert(bend_radius.isArray());
                  auto radius_count = bend_radius["length"].as<int>();
                  if (radius_count != self.num_elements) {
                    throw std::runtime_error(
                        "Length of sequence must match the number of paths.");
                  }
                  for (size_t i = 0; i < radius_count; i++) {
                    auto bend_radiu = bend_radius[i].as<double>();
                    auto el = self.elements + i;
                    if (bend_radiu > 0) {
                      if (el->bend_type == BendType::None) {
                        el->bend_type = BendType::Circular;
                      }
                      el->bend_radius = bend_radiu;
                    } else if (el->bend_type == BendType::Circular) {
                      el->bend_type = BendType::None;
                      el->bend_radius = 0;
                    }
                  }
                }))
      .function(
          "set_bend_function",
          optional_override([](FlexPath &self, const val &bend_functions) {
            assert(bend_functions.isArray());
            auto func_count = bend_functions["length"].as<int>();
            if (func_count != self.num_elements) {
              throw std::runtime_error(
                  "Length of sequence must match the number of paths.");
            }
            for (size_t i = 0; i < func_count; i++) {
              auto func = bend_functions[i];
              auto el = self.elements + i;
              if (el->bend_type == BendType::Function) {
                el->bend_type =
                    el->bend_radius > 0 ? BendType::Circular : BendType::None;
                el->bend_function = NULL;
                el->bend_function_data = NULL;
              }
              set_bend_function(el, func);
            }
          }))
      .function("horizontal",
                optional_override([](FlexPath &self, const val &x,
                                     const val &js_width, const val &js_offset,
                                     bool relative) {
                  double *buffer = (double *)gdstk::allocate(
                      sizeof(double) * self.num_elements * 2);
                  double *width = NULL;
                  if (!js_width.isNull()) {
                    width = buffer;
                    parse_flexpath_width(self, js_width, width);
                  }

                  double *offset = NULL;
                  if (!js_offset.isNull()) {
                    offset = buffer + self.num_elements;
                    parse_flexpath_offset(self, js_offset, offset);
                  }

                  if (x.isNumber()) {
                    self.horizontal(x.as<double>(), width, offset, relative);
                  } else if (x.isArray()) {
                    auto coord_x_array = utils::js_array2gdstk_array<double>(x);
                    self.horizontal(*coord_x_array, width, offset, relative);
                  }
                  gdstk::free_allocation(buffer);
                }))
      .function("horizontal",
                optional_override([](FlexPath &self, const val &x) {
                  double *width = NULL;
                  double *offset = NULL;
                  bool relative = false;
                  if (x.isNumber()) {
                    self.horizontal(x.as<double>(), width, offset, relative);
                  } else if (x.isArray()) {
                    auto coord_x_array = utils::js_array2gdstk_array<double>(x);
                    self.horizontal(*coord_x_array, width, offset, relative);
                  }
                }))
      .function("vertical",
                optional_override([](FlexPath &self, const val &y,
                                     const val &js_width, const val &js_offset,
                                     bool relative) {
                  double *buffer = (double *)gdstk::allocate(
                      sizeof(double) * self.num_elements * 2);
                  double *width = NULL;
                  if (!js_width.isNull()) {
                    width = buffer;
                    parse_flexpath_width(self, js_width, width);
                  }

                  double *offset = NULL;
                  if (!js_offset.isNull()) {
                    offset = buffer + self.num_elements;
                    parse_flexpath_offset(self, js_offset, offset);
                  }
                  if (y.isNumber()) {
                    self.vertical(y.as<double>(), width, offset, relative);
                  } else if (y.isArray()) {
                    auto coord_y_array = utils::js_array2gdstk_array<double>(y);
                    self.vertical(*coord_y_array, width, offset, relative);
                  }
                  gdstk::free_allocation(buffer);
                }))
      .function("vertical", optional_override([](FlexPath &self, const val &y) {
                  double *width = NULL;
                  double *offset = NULL;
                  bool relative = false;
                  if (y.isNumber()) {
                    self.horizontal(y.as<double>(), width, offset, relative);
                  } else if (y.isArray()) {
                    auto coord_y_array = utils::js_array2gdstk_array<double>(y);
                    self.horizontal(*coord_y_array, width, offset, relative);
                  }
                }))
      .function(
          "segment", optional_override([](FlexPath &self, const val &xy,
                                          const val &js_width,
                                          const val &js_offset, bool relative) {
            double *buffer = (double *)gdstk::allocate(sizeof(double) *
                                                       self.num_elements * 2);
            double *width = NULL;
            if (!js_width.isNull()) {
              width = buffer;
              parse_flexpath_width(self, js_width, width);
            }

            double *offset = NULL;
            if (!js_offset.isNull()) {
              offset = buffer + self.num_elements;
              parse_flexpath_offset(self, js_offset, offset);
            }
            if (xy[0].isNumber()) {
              self.segment(to_vec2(xy), width, offset, relative);
            } else if (xy[0].isArray()) {
              auto xy_coord_array = utils::js_array2gdstk_arrayvec2(xy);
              self.segment(*xy_coord_array, width, offset, relative);
            }
            gdstk::free_allocation(buffer);
          }))
      .function("segment", optional_override([](FlexPath &self, const val &xy) {
                  double *width = NULL;
                  double *offset = NULL;
                  bool relative = false;
                  if (xy[0].isNumber()) {
                    self.segment(to_vec2(xy), width, offset, relative);
                  } else if (xy[0].isArray()) {
                    auto xy_coord_array = utils::js_array2gdstk_arrayvec2(xy);
                    self.segment(*xy_coord_array, width, offset, relative);
                  }
                }))
      .function(
          "cubic", optional_override([](FlexPath &self, const val &xy,
                                        const val &js_width,
                                        const val &js_offset, bool relative) {
            double *buffer = (double *)gdstk::allocate(sizeof(double) *
                                                       self.num_elements * 2);
            double *width = NULL;
            if (!js_width.isNull()) {
              width = buffer;
              parse_flexpath_width(self, js_width, width);
            }

            double *offset = NULL;
            if (!js_offset.isNull()) {
              offset = buffer + self.num_elements;
              parse_flexpath_offset(self, js_offset, offset);
            }

            auto points_array = utils::js_array2gdstk_arrayvec2(xy);
            self.cubic(*points_array, width, offset, relative);
            gdstk::free_allocation(buffer);
          }))
      .function("cubic",
                optional_override([](FlexPath &self, const val &points) {
                  double *width = NULL;
                  double *offset = NULL;
                  bool relative = false;
                  auto points_array = utils::js_array2gdstk_arrayvec2(points);
                  self.cubic(*points_array, width, offset, relative);
                }))
      .function("cubic_smooth",
                optional_override([](FlexPath &self, const val &xy,
                                     const val &js_width, const val &js_offset,
                                     bool relative) {
                  double *buffer = (double *)gdstk::allocate(
                      sizeof(double) * self.num_elements * 2);
                  double *width = NULL;
                  if (!js_width.isNull()) {
                    width = buffer;
                    parse_flexpath_width(self, js_width, width);
                  }

                  double *offset = NULL;
                  if (!js_offset.isNull()) {
                    offset = buffer + self.num_elements;
                    parse_flexpath_offset(self, js_offset, offset);
                  }
                  auto points_array = utils::js_array2gdstk_arrayvec2(xy);
                  self.cubic_smooth(*points_array, width, offset, relative);
                  gdstk::free_allocation(buffer);
                }))
      .function("cubic_smooth",
                optional_override([](FlexPath &self, const val &xy) {
                  double *width = NULL;
                  double *offset = NULL;
                  bool relative = false;
                  auto points_array = utils::js_array2gdstk_arrayvec2(xy);
                  self.cubic_smooth(*points_array, width, offset, relative);
                }),
                allow_raw_pointers())
      .function("quadratic",
                optional_override([](FlexPath &self, const val &xy,
                                     const val &js_width, const val &js_offset,
                                     bool relative) {
                  double *buffer = (double *)gdstk::allocate(
                      sizeof(double) * self.num_elements * 2);
                  double *width = NULL;
                  if (!js_width.isNull()) {
                    width = buffer;
                    parse_flexpath_width(self, js_width, width);
                  }

                  double *offset = NULL;
                  if (!js_offset.isNull()) {
                    offset = buffer + self.num_elements;
                    parse_flexpath_offset(self, js_offset, offset);
                  }
                  auto points_array = utils::js_array2gdstk_arrayvec2(xy);
                  self.quadratic(*points_array, width, offset, relative);
                  gdstk::free_allocation(buffer);
                }))
      .function("quadratic",
                optional_override([](FlexPath &self, const val &xy) {
                  double *width = NULL;
                  double *offset = NULL;
                  bool relative = false;
                  auto points_array = utils::js_array2gdstk_arrayvec2(xy);
                  self.quadratic(*points_array, width, offset, relative);
                }))
      .function("quadratic_smooth",
                optional_override([](FlexPath &self, const val &xy,
                                     const val &js_width, const val &js_offset,
                                     bool relative) {
                  double *buffer = (double *)gdstk::allocate(
                      sizeof(double) * self.num_elements * 2);
                  double *width = NULL;
                  if (!js_width.isNull()) {
                    width = buffer;
                    parse_flexpath_width(self, js_width, width);
                  }

                  double *offset = NULL;
                  if (!js_offset.isNull()) {
                    offset = buffer + self.num_elements;
                    parse_flexpath_offset(self, js_offset, offset);
                  }
                  auto points_array = utils::js_array2gdstk_arrayvec2(xy);
                  self.quadratic_smooth(*points_array, width, offset, relative);
                  gdstk::free_allocation(buffer);
                }))
      .function("quadratic_smooth",
                optional_override([](FlexPath &self, const val &xy) {
                  double *width = NULL;
                  double *offset = NULL;
                  bool relative = false;
                  auto points_array = utils::js_array2gdstk_arrayvec2(xy);
                  self.quadratic_smooth(*points_array, width, offset, relative);
                }))
      .function(
          "bezier", optional_override([](FlexPath &self, const val &xy,
                                         const val &js_width,
                                         const val &js_offset, bool relative) {
            double *buffer = (double *)gdstk::allocate(sizeof(double) *
                                                       self.num_elements * 2);
            double *width = NULL;
            if (!js_width.isNull()) {
              width = buffer;
              parse_flexpath_width(self, js_width, width);
            }

            double *offset = NULL;
            if (!js_offset.isNull()) {
              offset = buffer + self.num_elements;
              parse_flexpath_offset(self, js_offset, offset);
            }
            auto points_array = utils::js_array2gdstk_arrayvec2(xy);
            self.bezier(*points_array, width, offset, relative);
            gdstk::free_allocation(buffer);
          }))
      .function("bezier", optional_override([](FlexPath &self, const val &xy) {
                  double *width = NULL;
                  double *offset = NULL;
                  bool relative = false;
                  auto points_array = utils::js_array2gdstk_arrayvec2(xy);
                  self.bezier(*points_array, width, offset, relative);
                }))
      .function("interpolation",
                optional_override(
                    [](FlexPath &self, const val &points, const val &angles,
                       const val &tension_in, const val &tension_out,
                       double initial_curl, double final_curl, bool cycle,
                       const val &width, const val &offset, bool relative) {
                      flexpath_interpolation(self, points, angles, tension_in,
                                             tension_out, initial_curl,
                                             final_curl, cycle, width, offset,
                                             relative);
                    }))
      .function("interpolation",
                optional_override([](FlexPath &self, const val &points) {
                  flexpath_interpolation(self, points);
                }))
      .function("arc",
                optional_override([](FlexPath &self, const val &radius,
                                     double initial_angle, double final_angle,
                                     double rotation, const val &js_width,
                                     const val &js_offset) {
                  flexpath_arc(self, radius, initial_angle, final_angle,
                               rotation, js_width, js_offset);
                }))
      .function("arc",
                optional_override([](FlexPath &self, const val &radius,
                                     double initial_angle, double final_angle) {
                  flexpath_arc(self, radius, initial_angle, final_angle);
                }))
      .function("turn", optional_override([](FlexPath &self, double radius,
                                             double angle, const val &js_width,
                                             const val &js_offset) {
                  double *buffer = (double *)gdstk::allocate(
                      sizeof(double) * self.num_elements * 2);
                  double *width = NULL;
                  if (!js_width.isNull()) {
                    width = buffer;
                    parse_flexpath_width(self, js_width, width);
                  }

                  double *offset = NULL;
                  if (!js_offset.isNull()) {
                    offset = buffer + self.num_elements;
                    parse_flexpath_offset(self, js_offset, offset);
                  }

                  if (radius <= 0) {
                    gdstk::free_allocation(buffer);
                    throw std::runtime_error("Turn radius must be positive.");
                  }
                  self.turn(radius, angle, width, offset);
                  gdstk::free_allocation(buffer);
                }))
      .function("turn", optional_override([](FlexPath &self, double radius,
                                             double angle) {
                  double *width = NULL;
                  double *offset = NULL;

                  if (radius <= 0) {
                    throw std::runtime_error("Turn radius must be positive.");
                  }
                  self.turn(radius, angle, width, offset);
                }))
      .function("parametric",
                optional_override([](FlexPath &self, const val &path_function,
                                     const val &js_width, const val &js_offset,
                                     bool relative) {
                  if (path_function.typeOf().as<std::string>() != "function") {
                    throw std::runtime_error(
                        "Argument path_function must be callable.");
                  }

                  double *buffer = (double *)gdstk::allocate(
                      sizeof(double) * self.num_elements * 2);
                  double *width = NULL;
                  if (!js_width.isNull()) {
                    width = buffer;
                    parse_flexpath_width(self, js_width, width);
                  }
                  double *offset = NULL;
                  if (!js_offset.isNull()) {
                    offset = buffer + self.num_elements;
                    parse_flexpath_offset(self, js_offset, offset);
                  }

                  utils::PATH_FUNC_SET.erase(&self);
                  utils::PATH_FUNC_SET.emplace(&self, path_function);
                  void *func = (void *)(&(utils::PATH_FUNC_SET.at(&self)));

                  self.parametric((ParametricVec2)eval_parametric_vec2, func,
                                  width, offset, relative);
                  gdstk::free_allocation(buffer);
                }))
      .function("parametric",
                optional_override([](FlexPath &self, const val &path_function) {
                  double *width = NULL;
                  double *offset = NULL;
                  bool relative = true;

                  utils::PATH_FUNC_SET.erase(&self);
                  utils::PATH_FUNC_SET.emplace(&self, path_function);
                  void *func = (void *)(&(utils::PATH_FUNC_SET.at(&self)));

                  self.parametric((ParametricVec2)eval_parametric_vec2, func,
                                  width, offset, relative);
                }))
      .function("commands",
                optional_override([](FlexPath &self, const val &path_commands) {
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
                }),
                allow_raw_pointers())
      .function("translate",
                optional_override([](FlexPath &self, double dx, double dy) {
                  return self.translate({dx, dy});
                }))
      .function("translate",
                optional_override([](FlexPath &self, const val &dx) {
                  return self.translate(to_vec2(dx));
                }))
      .function("scale", optional_override(
                             [](FlexPath &self, double s, const val &center) {
                               return self.scale(s, to_vec2(center));
                             }))
      .function("scale", optional_override([](FlexPath &self, double s) {
                  Vec2 center{0, 0};
                  return self.scale(s, center);
                }))
      .function("mirror", optional_override(
                              [](FlexPath &self, const val &p0, const val &p1) {
                                return self.mirror(to_vec2(p0), to_vec2(p1));
                              }))
      .function("mirror", optional_override([](FlexPath &self, const val &p0) {
                  const Vec2 p1{0, 0};
                  return self.mirror(to_vec2(p0), p1);
                }))
      .function("rotate", optional_override([](FlexPath &self, double angle,
                                               const val &center) {
                  return self.rotate(angle, to_vec2(center));
                }))
      .function("rotate", optional_override([](FlexPath &self, double angle) {
                  Vec2 center{0, 0};
                  return self.rotate(angle, center);
                }))
      .function("apply_repetition", optional_override([](FlexPath &self) {
                  Array<FlexPath *> array = {0};
                  self.apply_repetition(array);
                  auto result = utils::gdstk_array2js_array_by_ref(
                      array, utils::FlexPathDeleter());
                  array.clear();
                  return result;
                }))
      .function("set_gds_property",
                optional_override(
                    [](FlexPath &self, unsigned int attr, const val &value) {
                      gdstk::set_gds_property(self.properties, attr,
                                              value.as<std::string>().c_str());
                    }))
      .function("get_gds_property",
                optional_override([](FlexPath &self, unsigned int attr) {
                  const PropertyValue *value =
                      gdstk::get_gds_property(self.properties, attr);
                  return val::u8string((char *)value->bytes);
                }))
      .function("delete_gds_property",
                optional_override([](FlexPath &self, unsigned int attr) {
                  gdstk::remove_gds_property(self.properties, attr);
                }))
  // TODO: export method "set_property", "get_property", "delete_property",
#ifndef NDEBUG
      .function("print", &FlexPath::print)
#endif
      ;
}
