#include "binding_utils.h"
#include "gdstk_base_bind.h"

namespace
{
  std::shared_ptr<Polygon> make_polygon(const Array<Vec2> &points,
                                        uint32_t layer = 0,
                                        uint32_t datatype = 0)
  {
    if (points.count == 0)
    {
      throw std::runtime_error("Cannot create a polygon without vertices.");
    }
    auto polygon = std::shared_ptr<Polygon>(
        (Polygon *)gdstk::allocate_clear(sizeof(Polygon)),
        utils::PolygonDeleter());
    polygon->point_array.extend(points);
    polygon->tag = gdstk::make_tag(layer, datatype);
    return polygon;
  }

  void polygon_fillet(Polygon &self, const val &radii, double tolerance = 0.01)
  {
    if (tolerance <= 0)
    {
      throw std::runtime_error("Tolerance must be positive.");
    }

    auto radii_array = utils::make_gdstk_array<double>();
    if (radii.isNumber())
    {
      radii_array->append(radii.as<double>());
    }
    else if (radii.isArray())
    {
      radii_array = utils::js_array2gdstk_array<double>(radii);
    }
    else
    {
      throw std::runtime_error(
          "Argument radii must be number or array of number");
    }

    self.fillet(*radii_array, tolerance);
    return;
  }
} // namespace

// ----------------------------------------------------------------------------

void gdstk_polygon_bind()
{
  class_<Polygon>("Polygon")
      .smart_ptr<std::shared_ptr<Polygon>>("Polygon_shared_ptr")
      .constructor(optional_override([](const val &points)
                                     {
        assert(points.isArray() ||
               points["constructor"]["name"].as<std::string>() ==
                   "PointsArray");
        auto points_array = utils::js_array2gdstk_arrayvec2(points);
        return make_polygon(*points_array); }))
      // overload constructor
      // REMEMBER: Embind now only supports overloading methods via parameter
      // count and NOT support default input parameter
      .constructor(optional_override(
          [](const val &points, uint32_t layer, uint32_t datatype)
          {
            assert(points.isArray() ||
                   points["constructor"]["name"].as<std::string>() ==
                       "PointsArray");
            auto points_array = utils::js_array2gdstk_arrayvec2(points);
            return make_polygon(*points_array, layer, datatype);
          }))
      .property("points", optional_override([](const Polygon &self)
                                            {
                  auto point_array = std::shared_ptr<Array<Vec2>>(
                      &const_cast<Polygon &>(self).point_array,
                      utils::nodelete());
                  return arrayref_to_js_proxy(point_array); }),

                optional_override([](Polygon &self, const val &new_points)
                                  {
                  auto points_array =
                      utils::js_array2gdstk_arrayvec2(new_points);
                  self.point_array.clear();
                  self.point_array.copy_from(*points_array); }))
      .property("layer", optional_override([](const Polygon &self)
                                           { return gdstk::get_layer(self.tag); }),
                optional_override([](Polygon &self, uint32_t layer)
                                  { return gdstk::set_layer(self.tag, layer); }))
      .property("datatype", optional_override([](const Polygon &self)
                                              { return gdstk::get_type(self.tag); }),
                optional_override([](Polygon &self, uint32_t datatype)
                                  { return gdstk::set_type(self.tag, datatype); }))
      .property("size", optional_override([](const Polygon &self)
                                          { return int(self.point_array.count); }))
      .property(
          "repetition", optional_override([](const Polygon &self)
                                          {
            auto new_repetition = std::shared_ptr<Repetition>(
                (Repetition *)gdstk::allocate_clear(sizeof(Repetition)),
                utils::RepetitionDeleter());
            new_repetition->copy_from(self.repetition);
            return new_repetition; }),
          optional_override([](Polygon &self, const val &repetition)
                            {
            if (repetition.isNull()) {
              self.repetition.clear();
            } else if (repetition["constructor"]["name"].as<std::string>() !=
                       "Repetition") {
              throw std::runtime_error("Value must be a Repetition object.");
            }
            self.repetition.clear();
            self.repetition.copy_from(repetition.as<Repetition>()); }))
      // .function("get_points", optional_override([](const Polygon &self)
      //                                           { return utils::gdstk_array2js_array_by_value(self.point_array); }))
      // TODO: export property "properties"
      .function("copy", optional_override([](const Polygon &self)
                                          {
                  auto p = std::shared_ptr<Polygon>(
                      (Polygon *)gdstk::allocate_clear(sizeof(Polygon)),
                      utils::PolygonDeleter());
                  p->copy_from(self);
                  return p; }))
      .function("area", &Polygon::area)
      .function("bounding_box", optional_override([](Polygon &self)
                                                  {
                  Vec2 min{0}, max{0};
                  self.bounding_box(min, max);
                  if (min.x > max.x) {
                      return val::null();
                  }
                  auto result = val::array();
                  result.call<void>("push", vec2_to_js_array(min));
                  result.call<void>("push", vec2_to_js_array(max));
                  return result; }))
      .function(
          "contain", optional_override([](Polygon &self, const val &points)
                                       {
            assert(points["length"].as<int>() > 0);
            if ((points.isArray() && points["length"].as<int>() == 2 &&
                 points[0].isNumber() && points[1].isNumber()) ||
                points["constructor"]["name"].as<std::string>() == "Point") {
              return val(self.contain(to_vec2(points)));
            } else if (points[0].isArray() ||
                       points["constructor"]["name"].as<std::string>() ==
                           "PointsArray") {
              auto length = points["length"].as<int>();
              auto result = val::array();
              for (size_t i = 0; i < length; i++) {
                result.call<void>("push", self.contain(to_vec2(points[i])));
              }
              return result;
            } else {
              throw std::runtime_error(
                  "Argument shoulde be point or array of points");
            } }))
      .function("contain_all",
                optional_override([](Polygon &self, const val &points)
                                  {
                  auto points_array = utils::js_array2gdstk_arrayvec2(points);
                  auto result = self.contain_all(*points_array);
                  return result; }))
      .function("contain_any",
                optional_override([](Polygon &self, const val &points)
                                  {
                  auto points_array = utils::js_array2gdstk_arrayvec2(points);
                  auto result = self.contain_any(*points_array);
                  return result; }))
      .function("translate",
                optional_override([](Polygon &self, double dx, double dy)
                                  { return self.translate({dx, dy}); }))
      .function("translate",
                optional_override([](Polygon &self, const val &dx)
                                  { return self.translate(to_vec2(dx)); }))
      .function("scale", optional_override([](Polygon &self, double sx,
                                              double sy, const val &center)
                                           { return self.scale(Vec2{sx, sy}, to_vec2(center)); }))
      .function("scale",
                optional_override([](Polygon &self, double sx, double sy)
                                  {
                  Vec2 center{0, 0};
                  return self.scale(Vec2{sx, sy}, center); }))
      .function("mirror", optional_override(
                              [](Polygon &self, const val &p0, const val &p1)
                              {
                                return self.mirror(to_vec2(p0), to_vec2(p1));
                              }))
      .function("mirror", optional_override([](Polygon &self, const val &p0)
                                            {
                  const Vec2 p1{0, 0};
                  return self.mirror(to_vec2(p0), p1); }))
      .function("rotate", optional_override([](Polygon &self, double angle,
                                               const val &center)
                                            { return self.rotate(angle, to_vec2(center)); }))
      .function("rotate", optional_override([](Polygon &self, double angle)
                                            {
                  Vec2 center{0, 0};
                  return self.rotate(angle, center); }))
      .function(
          "transform",
          optional_override([](Polygon &self, double magnification,
                               bool x_reflection, double rotation,
                               const val &translation, const val &matrix)
                            {
            Vec2 origin = {0, 0};
            if (!translation.isNull()) {
              origin = to_vec2(translation);
            }
            self.transform(magnification, x_reflection, rotation, origin);

            if (!matrix.isNull()) {
              auto rows = matrix["length"].as<int>();
              double m[] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
              const bool homogeneous = rows == 3;
              for (rows--; rows >= 0; rows--) {
                auto cols = matrix[rows]["length"].as<int>();
                for (cols--; cols >= 0; cols--) {
                  m[rows * 3 + cols] = matrix[rows][cols].as<double>();
                }
              }

              Array<Vec2> *point_array = &self.point_array;
              Vec2 *p = point_array->items;
              if (homogeneous) {
                for (uint64_t num = point_array->count; num > 0; num--, p++) {
                  double x = p->x;
                  double y = p->y;
                  double w = 1.0 / (m[6] * x + m[7] * y + m[8]);
                  p->x = (m[0] * x + m[1] * y + m[2]) * w;
                  p->y = (m[3] * x + m[4] * y + m[5]) * w;
                }
              } else {
                for (uint64_t num = point_array->count; num > 0; num--, p++) {
                  double x = p->x;
                  double y = p->y;
                  p->x = m[0] * x + m[1] * y + m[2];
                  p->y = m[3] * x + m[4] * y + m[5];
                }
              }
            } }))
      .function("transform", optional_override([](Polygon &self)
                                               {
                  double magnification = 1;
                  bool x_reflection = false;
                  double rotation = 0;
                  const Vec2 origin{0, 0};
                  return self.transform(magnification, x_reflection, rotation,
                                        origin); }))
      .function("fillet", optional_override([](Polygon &self, const val &radii,
                                               double tolerance)
                                            { return polygon_fillet(self, radii, tolerance); }))
      .function("fillet",
                optional_override([](Polygon &self, const val &radii)
                                  { return polygon_fillet(self, radii); }))
      .function("fracture",
                optional_override(
                    [](Polygon &self, uint64_t max_points, double precision)
                    {
                      if (precision <= 0)
                      {
                        throw std::runtime_error("Precision must be positive.");
                      }
                      Array<Polygon *> result{0};
                      self.fracture(max_points, precision, result);
                      auto r = utils::gdstk_array2js_array_by_ref(
                          result, utils::PolygonDeleter());
                      result.clear();
                      return r;
                    }))
      .function("fracture", optional_override([](Polygon &self)
                                              {
                  uint64_t max_points = 199;
                  double precision = 1e-3;
                  Array<Polygon *> result{0};
                  self.fracture(max_points, precision, result);
                  auto r = utils::gdstk_array2js_array_by_ref(
                      result, utils::PolygonDeleter());
                  result.clear();
                  return r; }))
      .function("apply_repetition", optional_override([](Polygon &self)
                                                      {
                  Array<Polygon *> result{0};
                  self.apply_repetition(result);
                  auto r = utils::gdstk_array2js_array_by_ref(
                      result, utils::PolygonDeleter());
                  result.clear();
                  return r; }))
      .function("set_gds_property",
                optional_override(
                    [](Polygon &self, unsigned int attr, const val &value)
                    {
                      gdstk::set_gds_property(self.properties, attr,
                                              value.as<std::string>().c_str());
                    }))
      .function("get_gds_property",
                optional_override([](Polygon &self, unsigned int attr)
                                  {
                  const PropertyValue *value =
                      gdstk::get_gds_property(self.properties, attr);
                  return val::u8string((char *)value->bytes); }))
      .function("delete_gds_property",
                optional_override([](Polygon &self, unsigned int attr)
                                  { gdstk::remove_gds_property(self.properties, attr); }))
  // TODO: export method "set_property", "get_property", "delete_property"
#ifndef NDEBUG
      .function("print", &Polygon::print)
#endif
      ;
}
