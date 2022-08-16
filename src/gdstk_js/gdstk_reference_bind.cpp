#include <unordered_map>

#include "binding_utils.h"
#include "gdstk_base_bind.h"

namespace
{

  std::shared_ptr<Reference> make_reference(
      const val &cell, Vec2 origin = Vec2{0, 0}, double rotation = 0,
      double magnification = 1, bool x_reflection = false, double columns = 1,
      double rows = 1, const val &js_spacing = val::null())
  {
    auto reference = std::shared_ptr<Reference>(
        (Reference *)gdstk::allocate_clear(sizeof(Reference)),
        utils::ReferenceDeleter());

    auto cell_constr = cell["constructor"]["name"].as<std::string>();

    if (cell_constr == "Cell")
    {
      reference->type = ReferenceType::Cell;
      reference->cell = cell.as<Cell *>(allow_raw_pointers());
      utils::REF_KEEP_ALIVE_CELL.insert(
          {reference.get(), cell.as<std::shared_ptr<Cell>>()});
    }
    else if (cell_constr == "RawCell")
    {
      reference->type = ReferenceType::RawCell;
      reference->rawcell = cell.as<RawCell *>(allow_raw_pointers());
      utils::REF_KEEP_ALIVE_RAWCELL.insert(
          {reference.get(), cell.as<std::shared_ptr<RawCell>>()});
    }
    else if (cell.isString())
    {
      reference->type = ReferenceType::Name;
      auto name = cell.as<std::string>();
      auto len = name.size();
      reference->name = (char *)gdstk::allocate(++len);
      memcpy(reference->name, name.c_str(), len);
    }
    else
    {
      throw std::runtime_error(
          "Argument cell must be a Cell, RawCell, or string.");
    }

    if (!js_spacing.isNull() && columns > 0 && rows > 0)
    {
      Repetition *repetition = &reference->repetition;
      Vec2 spacing = to_vec2(js_spacing);
      // If any of these are zero, we won't be able to detect the AREF
      // construction in to_gds().
      if (columns == 1 && spacing.x == 0)
        spacing.x = 1;
      if (rows == 1 && spacing.y == 0)
        spacing.y = 1;
      repetition->type = RepetitionType::Rectangular;
      repetition->columns = columns;
      repetition->rows = rows;
      repetition->spacing = spacing;
      if (rotation != 0 || x_reflection)
      {
        repetition->transform(1, x_reflection > 0, rotation);
      }
    }

    reference->origin = origin;
    reference->rotation = rotation;
    reference->magnification = magnification;
    reference->x_reflection = x_reflection > 0;
    return reference;
  }
} // namespace

// ----------------------------------------------------------------------------

void gdstk_reference_bind()
{
  class_<Reference>("Reference")
      .smart_ptr<std::shared_ptr<Reference>>("Reference_shared_ptr")
      .constructor(optional_override([](const val &cell, const val &origin,
                                        double rotation, double magnification,
                                        bool x_reflection, double columns,
                                        double rows, const val &spacing)
                                     { return make_reference(cell, to_vec2(origin), rotation, magnification,
                                                             x_reflection, columns, rows, spacing); }))
      .constructor(optional_override(
          [](const val &cell)
          { return make_reference(cell); }))
      .property("cell", optional_override([](const Reference &self)
                                          {
                  switch (self.type) {
                    case ReferenceType::Cell:
                      return val(utils::REF_KEEP_ALIVE_CELL.at(
                          &const_cast<Reference&>(self)));
                    case ReferenceType::RawCell:
                      return val(utils::REF_KEEP_ALIVE_RAWCELL.at(
                          &const_cast<Reference&>(self)));
                    case ReferenceType::Name:
                      return val::u8string(self.name);
                  } }),
                optional_override([](Reference &self, const val &cell)
                                  {
                  ReferenceType new_type;
                  char* new_name = NULL;

                  if (cell["constructor"]["name"].as<std::string>() == "Cell") {
                    new_type = ReferenceType::Cell;
                    self.cell = cell.as<Cell*>(allow_raw_pointers());
                  } else if (cell["constructor"]["name"].as<std::string>() ==
                             "RawCell") {
                    new_type = ReferenceType::RawCell;
                    self.rawcell = cell.as<RawCell*>(allow_raw_pointers());
                  } else if (cell.isString()) {
                    new_type = ReferenceType::Name;
                    auto name = cell.as<std::string>();
                    auto len = name.size();
                    new_name = (char*)gdstk::allocate(++len);
                    memcpy(new_name, name.c_str(), len);
                    gdstk::free_allocation(self.name);
                    self.name = new_name;
                  } else {
                    throw std::runtime_error(
                        "Argument cell must be a Cell, RawCell, or string.");
                  } }))
      .property("origin",
                optional_override([](const Reference &self)
                                  { return vec2_to_js_array(self.origin); }),
                optional_override([](Reference &self, const val &origin)
                                  { self.origin = to_vec2(origin); 
                                  return vec2_to_js_array(self.origin); }))
      .property("rotation", &Reference::rotation)
      .property("magnification", &Reference::magnification)
      .property("x_reflection", &Reference::x_reflection)
      .property(
          "repetition", optional_override([](const Reference &self)
                                          {
            auto new_repetition = std::shared_ptr<Repetition>(
                (Repetition*)gdstk::allocate_clear(sizeof(Repetition)),
                utils::RepetitionDeleter());
            new_repetition->copy_from(self.repetition);
            return new_repetition; }),
          optional_override([](Reference &self, const val &repetition)
                            {
            if (repetition.isNull()) {
              self.repetition.clear();
            } else if (repetition["constructor"]["name"].as<std::string>() !=
                       "Repetition") {
              throw std::runtime_error("Value must be a Repetition object.");
            }
            self.repetition.clear();
            self.repetition.copy_from(repetition.as<Repetition>()); }))
      .function("copy", optional_override([](Reference &self)
                                          {
                  auto result = std::shared_ptr<Reference>(
                      (Reference*)gdstk::allocate_clear(sizeof(Reference)),
                      utils::ReferenceDeleter());
                  result->copy_from(self);
                  if (result->type == ReferenceType::Cell) {
                    utils::REF_KEEP_ALIVE_CELL[result.get()] =
                        utils::REF_KEEP_ALIVE_CELL[&self];
                  } else if (result->type == ReferenceType::RawCell) {
                    utils::REF_KEEP_ALIVE_RAWCELL[result.get()] =
                        utils::REF_KEEP_ALIVE_RAWCELL[&self];
                  }
                  return result; }))
      .function("bounding_box", optional_override([](Reference &self)
                                                  {
                  Vec2 min{0, 0};
                  Vec2 max{0, 0};
                  self.bounding_box(min, max);
                  if (min.x > max.x) {
                      return val::null();
                  }
                  val result = val::array();
                  result.call<void>("push", vec2_to_js_array(min));
                  result.call<void>("push", vec2_to_js_array(max));
                  return result; }))
      .function("convex_hull", optional_override([](Reference &self)
                                                 {
                  Array<Vec2> points = {0};
                  self.convex_hull(points);
                  auto result = utils::gdstk_array2js_array_by_value(points);
                  points.clear();
                  return result; }))
      .function("apply_repetition", optional_override([](Reference &self)
                                                      {
                  Array<Reference*> array = {0};
                  self.apply_repetition(array);
                  auto result = utils::gdstk_array2js_array_by_ref(
                      array, utils::ReferenceDeleter());
                  array.clear();
                  return result; }))
      .function("set_gds_property",
                optional_override(
                    [](Reference &self, unsigned int attr, const val &value)
                    {
                      gdstk::set_gds_property(self.properties, attr,
                                              value.as<std::string>().c_str());
                    }))
      .function("get_gds_property",
                optional_override([](Reference &self, unsigned int attr)
                                  {
                  const PropertyValue* value =
                      gdstk::get_gds_property(self.properties, attr);
                  return val::u8string((char*)value->bytes); }))
      .function("delete_gds_property",
                optional_override([](Reference &self, unsigned int attr)
                                  { gdstk::remove_gds_property(self.properties, attr); }))
  // TODO:set_property, get_property, delete_property
#ifndef NDEBUG
      .function("print", &Reference::print)
#endif
      ;
}