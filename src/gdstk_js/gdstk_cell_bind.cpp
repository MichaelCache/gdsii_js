#include <set>
#include <unordered_map>

#include "binding_utils.h"
#include "gdstk_base_bind.h"

namespace {
std::string make_key(Tag tag) {
  auto layer = gdstk::get_layer(tag);
  auto type = gdstk::get_type(tag);
  return std::to_string(layer) + std::string(",") + std::to_string(type);
}

static bool filter_check(int8_t operation, bool a, bool b) {
  switch (operation) {
    case 0:
      return a && b;
    case 1:
      return a || b;
    case 2:
      return a != b;
    case 3:
      return !(a && b);
    case 4:
      return !(a || b);
    case 5:
      return a == b;
    default:
      return false;
  }
}
// ----------------------------------------------------------------------------

val cell_area(Cell &self, bool by_spec = false) {
  Array<Polygon *> array = {0};
  self.get_polygons(true, true, -1, false, 0, array);

  val result = val::null();
  if (by_spec) {
    result = val::object();
    for (uint64_t i = 0; i < array.count; i++) {
      Polygon *poly = array[i];
      double area = poly->area();
      auto key = make_key(poly->tag);
      // has no key
      if (result[key.c_str()].isUndefined()) {
        result.set(key.c_str(), area);
      }
      // key exist
      else {
        result.set(key.c_str(), result[key.c_str()].as<double>() + area);
      }
    }
  } else {
    double area = 0;
    for (uint64_t i = 0; i < array.count; i++) {
      Polygon *poly = array[i];
      area += poly->area();
    }
    result = val(area);
  }

  for (uint64_t i = 0; i < array.count; i++) {
    array[i]->clear();
    free_allocation(array[i]);
  }
  array.clear();
  return result;
}

val cell_get_polygons(Cell &self, bool apply_repetitions = true,
                      bool include_paths = true,
                      const val &js_depth = val::null(),
                      const val &js_layer = val::null(),
                      const val &js_datatype = val::null()) {
  int64_t depth = -1;
  if (!js_depth.isNull()) {
    depth = js_depth.as<int>();
  }

  uint32_t layer = 0;
  uint32_t datatype = 0;
  bool filter = (!js_layer.isNull()) && (!js_datatype.isNull());
  if (filter) {
    layer = js_layer.as<uint32_t>();
    datatype = js_datatype.as<uint32_t>();
  }

  Array<Polygon *> array = {0};
  self.get_polygons(apply_repetitions, include_paths, depth, filter,
                    gdstk::make_tag(layer, datatype), array);

  auto r = utils::gdstk_array2js_array_by_ref(array, utils::PolygonDeleter());

  array.clear();
  return r;
}

val cell_get_paths(Cell &self, bool apply_repetitions = true,
                   const val &js_depth = val::null(),
                   const val &js_layer = val::null(),
                   const val &js_datatype = val::null()) {
  int64_t depth = -1;
  if (!js_depth.isNull()) {
    depth = js_depth.as<int>();
  }

  uint32_t layer = 0;
  uint32_t datatype = 0;
  bool filter = (!js_layer.isNull()) && (!js_datatype.isNull());
  if (filter) {
    layer = js_layer.as<uint32_t>();
    datatype = js_datatype.as<uint32_t>();
  }

  Array<FlexPath *> fp_array = {0};
  self.get_flexpaths(apply_repetitions > 0, depth, filter,
                     gdstk::make_tag(layer, datatype), fp_array);

  Array<RobustPath *> rp_array = {0};
  self.get_robustpaths(apply_repetitions > 0, depth, filter,
                       gdstk::make_tag(layer, datatype), rp_array);

  auto fp =
      utils::gdstk_array2js_array_by_ref(fp_array, utils::FlexPathDeleter());
  auto rp =
      utils::gdstk_array2js_array_by_ref(rp_array, utils::RobustPathDeleter());

  val result = val::array();
  result.call<void>("push", fp);
  result.call<void>("push", rp);
  result = result.call<val>("flat");

  fp_array.clear();
  rp_array.clear();
  return result;
}

val cell_get_labels(Cell &self, bool apply_repetitions = true,
                    const val &js_depth = val::null(),
                    const val &js_layer = val::null(),
                    const val &js_texttype = val::null()) {
  int64_t depth = -1;
  if (!js_depth.isNull()) {
    depth = js_depth.as<int>();
  }

  uint32_t layer = 0;
  uint32_t texttype = 0;
  bool filter = (!js_layer.isNull()) && (!js_texttype.isNull());
  if (filter) {
    layer = js_layer.as<uint32_t>();
    texttype = js_texttype.as<uint32_t>();
  }

  Array<Label *> array = {0};
  self.get_labels(apply_repetitions > 0, depth, filter,
                  gdstk::make_tag(layer, texttype), array);

  auto result =
      utils::gdstk_array2js_array_by_ref(array, utils::LabelDeleter());

  array.clear();
  return result;
}

std::shared_ptr<Cell> cell_copy(Cell &self, const val &js_name,
                                const Vec2 &translation = {0, 0},
                                double rotation = 0, double magnification = 1,
                                bool x_reflection = false,
                                bool deep_copy = true) {
  auto name = js_name.as<std::string>();

  bool transform = (translation.x != 0 || translation.y != 0 || rotation != 0 ||
                    magnification != 1 || x_reflection > 0);
  if (transform) deep_copy = 1;

  auto cell = std::shared_ptr<Cell>((Cell *)gdstk::allocate_clear(sizeof(Cell)),
                                    utils::CellDeleter());
  cell->copy_from(self, name.c_str(), deep_copy > 0);

  Array<Polygon *> *polygon_array = &cell->polygon_array;
  if (deep_copy) {
    for (uint64_t i = 0; i < polygon_array->count; i++) {
      Polygon *polygon = (*polygon_array)[i];
      if (transform) {
        polygon->transform(magnification, x_reflection > 0, rotation,
                           translation);
        polygon->repetition.transform(magnification, x_reflection > 0,
                                      rotation);
      }
      utils::CELL_KEEP_ALIVE_GEOM[cell.get()].polygons.insert(
          {polygon, utils::CELL_KEEP_ALIVE_GEOM[&self].polygons.at(polygon)});
    }
  }

  Array<Reference *> *reference_array = &cell->reference_array;
  if (deep_copy) {
    for (uint64_t i = 0; i < reference_array->count; i++) {
      Reference *reference = (*reference_array)[i];
      if (transform) {
        reference->transform(magnification, x_reflection > 0, rotation,
                             translation);
        reference->repetition.transform(magnification, x_reflection > 0,
                                        rotation);
      }
      utils::CELL_KEEP_ALIVE_GEOM[cell.get()].references.insert(
          {reference,
           utils::CELL_KEEP_ALIVE_GEOM[&self].references.at(reference)});
    }
  }

  Array<FlexPath *> *flexpath_array = &cell->flexpath_array;
  if (deep_copy) {
    for (uint64_t i = 0; i < flexpath_array->count; i++) {
      FlexPath *path = (*flexpath_array)[i];
      if (transform) {
        path->transform(magnification, x_reflection > 0, rotation, translation);
        path->repetition.transform(magnification, x_reflection > 0, rotation);
      }
      utils::CELL_KEEP_ALIVE_GEOM[cell.get()].flexpaths.insert(
          {path, utils::CELL_KEEP_ALIVE_GEOM[&self].flexpaths.at(path)});
    }
  }

  Array<RobustPath *> *robustpath_array = &cell->robustpath_array;
  if (deep_copy) {
    for (uint64_t i = 0; i < robustpath_array->count; i++) {
      RobustPath *path = (*robustpath_array)[i];
      if (transform) {
        path->transform(magnification, x_reflection > 0, rotation, translation);
        path->repetition.transform(magnification, x_reflection > 0, rotation);
      }
      utils::CELL_KEEP_ALIVE_GEOM[cell.get()].robustpaths.insert(
          {path, utils::CELL_KEEP_ALIVE_GEOM[&self].robustpaths.at(path)});
    }
  }

  Array<Label *> *label_array = &cell->label_array;
  if (deep_copy) {
    for (uint64_t i = 0; i < label_array->count; i++) {
      Label *label = (*label_array)[i];
      if (transform) {
        label->transform(magnification, x_reflection > 0, rotation,
                         translation);
        label->repetition.transform(magnification, x_reflection > 0, rotation);
      }
      utils::CELL_KEEP_ALIVE_GEOM[cell.get()].labels.insert(
          {label, utils::CELL_KEEP_ALIVE_GEOM[&self].labels.at(label)});
    }
  }

  return cell;
}

void cell_filter(Cell &self, const val &js_layers, const val &js_types,
                 const val &js_operation, bool polygons = true,
                 bool paths = true, bool labels = true) {
  if (!js_layers.isArray()) {
    throw std::runtime_error("Argument layers must be a sequence.");
  }
  if (!js_types.isArray()) {
    throw std::runtime_error("Argument types must be a sequence.");
  }

  auto layers = utils::js_array2gdstk_array<uint32_t>(js_layers);
  auto types = utils::js_array2gdstk_array<uint32_t>(js_types);

  int8_t op = -1;
  auto str = js_operation.as<std::string>();
  const char *operation = str.c_str();
  if (strcmp(operation, "and") == 0) {
    op = 0;
  } else if (strcmp(operation, "or") == 0) {
    op = 1;
  } else if (strcmp(operation, "xor") == 0) {
    op = 2;
  } else if (strcmp(operation, "nand") == 0) {
    op = 3;
  } else if (strcmp(operation, "nor") == 0) {
    op = 4;
  } else if (strcmp(operation, "nxor") == 0) {
    op = 5;
  } else {
    throw std::runtime_error(
        "Operation must be one of 'and', 'or', 'xor', 'nand', 'or', 'nxor'.");
  }

  if (polygons > 0) {
    uint64_t i = 0;
    while (i < self.polygon_array.count) {
      Polygon *poly = self.polygon_array[i];
      if (filter_check(op, layers->contains(gdstk::get_layer(poly->tag)),
                       types->contains(gdstk::get_type(poly->tag)))) {
        self.polygon_array.remove_unordered(i);
        utils::CELL_KEEP_ALIVE_GEOM.at(&self).polygons.erase(poly);
      } else {
        ++i;
      }
    }
  }

  if (paths > 0) {
    uint64_t i = 0;
    while (i < self.flexpath_array.count) {
      FlexPath *path = self.flexpath_array[i];
      uint64_t remove = 0;
      uint64_t j = 0;
      while (j < path->num_elements) {
        FlexPathElement *el = path->elements + j++;
        if (filter_check(op, layers->contains(gdstk::get_layer(el->tag)),
                         types->contains(gdstk::get_type(el->tag))))
          remove++;
      }
      if (remove == path->num_elements) {
        utils::CELL_KEEP_ALIVE_GEOM.at(&self).flexpaths.erase(
            self.flexpath_array[i]);
        self.flexpath_array.remove_unordered(i);
      } else {
        if (remove > 0) {
          j = 0;
          while (j < path->num_elements) {
            FlexPathElement *el = path->elements + j;
            if (filter_check(op, layers->contains(gdstk::get_layer(el->tag)),
                             types->contains(gdstk::get_type(el->tag)))) {
              el->half_width_and_offset.clear();
              path->elements[j] = path->elements[--path->num_elements];
            } else {
              ++j;
            }
          }
        }
        ++i;
      }
    }

    i = 0;
    while (i < self.robustpath_array.count) {
      RobustPath *path = self.robustpath_array[i];
      uint64_t remove = 0;
      uint64_t j = 0;
      while (j < path->num_elements) {
        RobustPathElement *el = path->elements + j++;
        if (filter_check(op, layers->contains(gdstk::get_layer(el->tag)),
                         types->contains(gdstk::get_type(el->tag))))
          remove++;
      }
      if (remove == path->num_elements) {
        utils::CELL_KEEP_ALIVE_GEOM.at(&self).robustpaths.erase(
            self.robustpath_array[i]);
        self.robustpath_array.remove_unordered(i);
      } else {
        if (remove > 0) {
          j = 0;
          while (j < path->num_elements) {
            RobustPathElement *el = path->elements + j;
            if (filter_check(op, layers->contains(gdstk::get_layer(el->tag)),
                             types->contains(gdstk::get_type(el->tag)))) {
              el->width_array.clear();
              el->offset_array.clear();
              path->elements[j] = path->elements[--path->num_elements];
            } else {
              ++j;
            }
          }
        }
        ++i;
      }
    }
  }

  if (labels > 0) {
    uint64_t i = 0;
    while (i < self.label_array.count) {
      Label *label = self.label_array[i];
      if (filter_check(op, layers->contains(gdstk::get_layer(label->tag)),
                       types->contains(gdstk::get_type(label->tag)))) {
        self.label_array.remove_unordered(i);
        utils::CELL_KEEP_ALIVE_GEOM.at(&self).labels.erase(label);
      } else {
        ++i;
      }
    }
  }
}

}  // namespace

// ----------------------------------------------------------------------------

void gdstk_cell_bind() {
  class_<Cell>("Cell")
      .smart_ptr<std::shared_ptr<Cell>>("Cell_shared_ptr")
      .constructor(optional_override([](const val &name) {
        assert(name.isString());
        auto cell = std::shared_ptr<Cell>(
            (Cell *)gdstk::allocate_clear(sizeof(Cell)), utils::CellDeleter());
        uint64_t len;
        cell->name = gdstk::copy_string(name.as<std::string>().c_str(), &len);
        if (len <= 1) {
          throw std::runtime_error("Empty cell name");
        }
        utils::CELL_KEEP_ALIVE_GEOM[cell.get()];
        return cell;
      }))
      .property("name", optional_override([](const Cell &self) {
                  return val::u8string(self.name);
                }),
                optional_override([](Cell &self, val new_name) {
                  assert(new_name.isString());
                  auto name = new_name.as<std::string>();
                  int len = name.size();  // without null char
                  if (self.name) gdstk::free_allocation(self.name);
                  self.name = (char *)gdstk::allocate(++len);
                  memcpy(self.name, new_name.as<std::string>().c_str(), len);
                }))
      .property("polygons", optional_override([](const Cell &self) {
                  auto js_array = val::array();
                  auto &poly_array =
                      utils::CELL_KEEP_ALIVE_GEOM.at(&const_cast<Cell &>(self))
                          .polygons;
                  for (size_t i = 0; i < self.polygon_array.count; i++)
                  {
                    auto polygon = self.polygon_array[i];
                    // convert raw pointer to shared_ptr
                    js_array.call<void>("push", val(poly_array.at(polygon)));
                  }
                  return js_array;
                }))
      .property("references", optional_override([](const Cell &self) {
                  auto js_array = val::array();
                  auto &ref_array =
                      utils::CELL_KEEP_ALIVE_GEOM.at(&const_cast<Cell &>(self))
                          .references;
                  for (size_t i = 0; i < self.reference_array.count; i++)
                  {
                    auto ref = self.reference_array[i];
                    // convert raw pointer to shared_ptr
                    js_array.call<void>("push", val(ref_array.at(ref)));
                  }
                  return js_array;
                }))
      .property("paths", optional_override([](const Cell &self) {
                  auto val_flexpath = val::array();
                  auto &flex_array =
                      utils::CELL_KEEP_ALIVE_GEOM.at(&const_cast<Cell &>(self))
                          .flexpaths;
                  for (size_t i = 0; i < self.flexpath_array.count; i++)
                  {
                    auto path = self.flexpath_array[i];
                    // convert raw pointer to shared_ptr
                    val_flexpath.call<void>("push", val(flex_array.at(path)));
                  }
                  
                  auto val_robustpath = val::array();
                  auto &robust_array =
                      utils::CELL_KEEP_ALIVE_GEOM.at(&const_cast<Cell &>(self))
                          .robustpaths;
                  for (size_t i = 0; i < self.robustpath_array.count; i++)
                  {
                    auto path = self.robustpath_array[i];
                    // convert raw pointer to shared_ptr
                    val_flexpath.call<void>("push", val(robust_array.at(path)));
                  }
                  
                  val result = val::array();
                  result.call<void>("push", val_flexpath);
                  result.call<void>("push", val_robustpath);
                  result = result.call<val>("flat");
                  return result;
                }))
      .property("labels", optional_override([](const Cell &self) {
                  auto js_array = val::array();
                  auto &label_array =
                      utils::CELL_KEEP_ALIVE_GEOM.at(&const_cast<Cell &>(self))
                          .labels;
                  for (size_t i = 0; i < self.label_array.count; i++)
                  {
                    auto label = self.label_array[i];
                    // convert raw pointer to shared_ptr
                    js_array.call<void>("push", val(label_array.at(label)));
                  }
                  return js_array;
                }))
      // TODO:properties
      .function("add", optional_override([](Cell &self, const val &elements) {
                  std::string cons_name =
                      elements["constructor"]["name"].as<std::string>();
                  if (cons_name == "Polygon") {
                    self.polygon_array.append(
                        elements.as<Polygon *>(allow_raw_pointers()));
                    utils::CELL_KEEP_ALIVE_GEOM[&self].polygons.insert(
                        {elements.as<Polygon *>(allow_raw_pointers()),
                         elements.as<std::shared_ptr<Polygon>>()});
                  } else if (cons_name == "Reference") {
                    self.reference_array.append(
                        elements.as<Reference *>(allow_raw_pointers()));
                    utils::CELL_KEEP_ALIVE_GEOM[&self].references.insert(
                        {elements.as<Reference *>(allow_raw_pointers()),
                         elements.as<std::shared_ptr<Reference>>()});
                  } else if (cons_name == "FlexPath") {
                    self.flexpath_array.append(
                        elements.as<FlexPath *>(allow_raw_pointers()));
                    utils::CELL_KEEP_ALIVE_GEOM[&self].flexpaths.insert(
                        {elements.as<FlexPath *>(allow_raw_pointers()),
                         elements.as<std::shared_ptr<FlexPath>>()});
                  } else if (cons_name == "RobustPath") {
                    self.robustpath_array.append(
                        elements.as<RobustPath *>(allow_raw_pointers()));
                    utils::CELL_KEEP_ALIVE_GEOM[&self].robustpaths.insert(
                        {elements.as<RobustPath *>(allow_raw_pointers()),
                         elements.as<std::shared_ptr<RobustPath>>()});
                  } else if (cons_name == "Label") {
                    self.label_array.append(
                        elements.as<Label *>(allow_raw_pointers()));
                    utils::CELL_KEEP_ALIVE_GEOM[&self].labels.insert(
                        {elements.as<Label *>(allow_raw_pointers()),
                         elements.as<std::shared_ptr<Label>>()});
                  } else if (elements.isArray()) {
                    int length = elements["length"].as<int>();
                    for (int i = 0; i < length; i++) {
                      auto item = elements[i];
                      std::string cons =
                          item["constructor"]["name"].as<std::string>();
                      if (cons == "Polygon") {
                        self.polygon_array.append(
                            item.as<Polygon *>(allow_raw_pointers()));
                        utils::CELL_KEEP_ALIVE_GEOM[&self].polygons.insert(
                            {item.as<Polygon *>(allow_raw_pointers()),
                             item.as<std::shared_ptr<Polygon>>()});
                      } else if (cons == "Reference") {
                        self.reference_array.append(
                            item.as<Reference *>(allow_raw_pointers()));
                        utils::CELL_KEEP_ALIVE_GEOM[&self].references.insert(
                            {item.as<Reference *>(allow_raw_pointers()),
                             item.as<std::shared_ptr<Reference>>()});
                      } else if (cons == "FlexPath") {
                        self.flexpath_array.append(
                            item.as<FlexPath *>(allow_raw_pointers()));
                        utils::CELL_KEEP_ALIVE_GEOM[&self].flexpaths.insert(
                            {item.as<FlexPath *>(allow_raw_pointers()),
                             item.as<std::shared_ptr<FlexPath>>()});
                      } else if (cons == "RobustPath") {
                        self.robustpath_array.append(
                            item.as<RobustPath *>(allow_raw_pointers()));
                        utils::CELL_KEEP_ALIVE_GEOM[&self].robustpaths.insert(
                            {item.as<RobustPath *>(allow_raw_pointers()),
                             item.as<std::shared_ptr<RobustPath>>()});
                      } else if (cons == "Label") {
                        self.label_array.append(
                            item.as<Label *>(allow_raw_pointers()));
                        utils::CELL_KEEP_ALIVE_GEOM[&self].labels.insert(
                            {item.as<Label *>(allow_raw_pointers()),
                             item.as<std::shared_ptr<Label>>()});
                      } else {
                        throw std::runtime_error(
                            "Arguments must be Polygon, FlexPath, "
                            "RobustPath, Label or Reference.");
                      }
                    }
                  } else {
                    throw std::runtime_error(
                        "Arguments must be Polygon, FlexPath, "
                        "RobustPath, Label or Reference.");
                  }
                }))
      .function("area", optional_override([](Cell &self, bool by_spec) {
                  return cell_area(self, by_spec);
                }))
      .function("area",
                optional_override([](Cell &self) { return cell_area(self); }))
      .function("bounding_box", optional_override([](Cell &self) {
                  Vec2 min, max;
                  self.bounding_box(min, max);
                  if (min.x > max.x) {
                    return val::null();
                  }
                  val result = val::array();
                  result.call<void>("push", vec2_to_js_array(min));
                  result.call<void>("push", vec2_to_js_array(max));
                  return result;
                }))
      .function("convex_hull", optional_override([](Cell &self) {
                  Array<Vec2> points = {0};
                  self.convex_hull(points);
                  auto r = utils::gdstk_array2js_array_by_value(points);
                  points.clear();
                  return r;
                }))
      .function("get_polygons",
                optional_override([](Cell &self, bool apply_repetitions,
                                     bool include_paths, const val &depth,
                                     const val &layer, const val &datatype) {
                  return cell_get_polygons(self, apply_repetitions,
                                           include_paths, depth, layer,
                                           datatype);
                }))
      .function("get_polygons", optional_override([](Cell &self) {
                  return cell_get_polygons(self);
                }))
      .function("get_paths",
                optional_override([](Cell &self, bool apply_repetitions,
                                     const val &depth, const val &layer,
                                     const val &datatype) {
                  return cell_get_paths(self, apply_repetitions, depth, layer,
                                        datatype);
                }))
      .function("get_paths", optional_override([](Cell &self) {
                  return cell_get_paths(self);
                }))
      .function("get_labels",
                optional_override([](Cell &self, bool apply_repetitions,
                                     const val &depth, const val &layer,
                                     const val &texttype) {
                  return cell_get_labels(self, apply_repetitions, depth, layer,
                                         texttype);
                }))
      .function("get_labels", optional_override([](Cell &self) {
                  return cell_get_labels(self);
                }))
      .function("flatten",
                optional_override([](Cell &self, bool apply_repetitions) {
                  Array<Reference *> removed_reference = {0};
                  self.flatten(apply_repetitions, removed_reference);
                  for (size_t i = 0; i < removed_reference.count; i++) {
                    utils::CELL_KEEP_ALIVE_GEOM[&self].references.erase(
                        removed_reference[i]);
                  }
                  removed_reference.clear();
                }))
      .function("flatten", optional_override([](Cell &self) {
                  bool apply_repetitions = true;
                  Array<Reference *> removed_reference = {0};
                  self.flatten(apply_repetitions, removed_reference);
                  for (size_t i = 0; i < removed_reference.count; i++) {
                    utils::CELL_KEEP_ALIVE_GEOM[&self].references.erase(
                        removed_reference[i]);
                  }
                  removed_reference.clear();
                }))

      .function("copy",
                optional_override([](Cell &self, const val &name,
                                     const val &translation, double rotation,
                                     double magnification, bool x_reflection,
                                     bool deep_copy) {
                  return cell_copy(self, name, to_vec2(translation), rotation,
                                   magnification, x_reflection, deep_copy);
                }))
      .function("copy", optional_override([](Cell &self, const val &name) {
                  return cell_copy(self, name);
                }))
      // TODO: .function("write_svg")
      .function(
          "remove", optional_override([](Cell &self, const val &elements) {
            auto contr = elements["constructor"]["name"].as<std::string>();
            if (contr == "Polygon") {
              Polygon *polygon = elements.as<Polygon *>(allow_raw_pointers());
              Array<Polygon *> *array = &self.polygon_array;
              array->remove_item(polygon);
              utils::CELL_KEEP_ALIVE_GEOM.at(&self).polygons.erase(polygon);
            } else if (contr == "Reference") {
              Reference *reference =
                  elements.as<Reference *>(allow_raw_pointers());
              Array<Reference *> *array = &self.reference_array;
              array->remove_item(reference);
              utils::CELL_KEEP_ALIVE_GEOM.at(&self).references.erase(reference);
            } else if (contr == "FlexPath") {
              FlexPath *flexpath =
                  elements.as<FlexPath *>(allow_raw_pointers());
              Array<FlexPath *> *array = &self.flexpath_array;
              array->remove_item(flexpath);
              utils::CELL_KEEP_ALIVE_GEOM.at(&self).flexpaths.erase(flexpath);
            } else if (contr == "RobustPath") {
              RobustPath *robustpath =
                  elements.as<RobustPath *>(allow_raw_pointers());
              Array<RobustPath *> *array = &self.robustpath_array;
              array->remove_item(robustpath);
              utils::CELL_KEEP_ALIVE_GEOM.at(&self).robustpaths.erase(
                  robustpath);
            } else if (contr == "Label") {
              Label *label = elements.as<Label *>(allow_raw_pointers());
              Array<Label *> *array = &self.label_array;
              array->remove_item(label);
              utils::CELL_KEEP_ALIVE_GEOM.at(&self).labels.erase(label);
            } else if (elements.isArray()) {
              auto len = elements["length"].as<uint32_t>();
              for (size_t i = 0; i < len; i++) {
                auto item = elements[i];
                auto cons = item["constructor"]["name"].as<std::string>();
                if (cons == "Polygon") {
                  Polygon *polygon = item.as<Polygon *>(allow_raw_pointers());
                  Array<Polygon *> *array = &self.polygon_array;
                  array->remove_item(polygon);
                  utils::CELL_KEEP_ALIVE_GEOM.at(&self).polygons.erase(polygon);
                } else if (cons == "Reference") {
                  Reference *reference =
                      item.as<Reference *>(allow_raw_pointers());
                  Array<Reference *> *array = &self.reference_array;
                  array->remove_item(reference);
                  utils::CELL_KEEP_ALIVE_GEOM.at(&self).references.erase(
                      reference);
                } else if (cons == "FlexPath") {
                  FlexPath *flexpath =
                      item.as<FlexPath *>(allow_raw_pointers());
                  Array<FlexPath *> *array = &self.flexpath_array;
                  array->remove_item(flexpath);
                  utils::CELL_KEEP_ALIVE_GEOM.at(&self).flexpaths.erase(
                      flexpath);
                } else if (cons == "RobustPath") {
                  RobustPath *robustpath =
                      item.as<RobustPath *>(allow_raw_pointers());
                  Array<RobustPath *> *array = &self.robustpath_array;
                  array->remove_item(robustpath);
                  utils::CELL_KEEP_ALIVE_GEOM.at(&self).robustpaths.erase(
                      robustpath);
                } else if (cons == "Label") {
                  Label *label = item.as<Label *>(allow_raw_pointers());
                  Array<Label *> *array = &self.label_array;
                  array->remove_item(label);
                  utils::CELL_KEEP_ALIVE_GEOM.at(&self).labels.erase(label);
                } else {
                  throw std::runtime_error(
                      "Arguments must be Polygon, FlexPath, "
                      "RobustPath, Label or Reference.");
                }
              }
            } else {
              throw std::runtime_error(
                  "Arguments must be Polygon, FlexPath, "
                  "RobustPath, Label or Reference.");
            }
          }))
      .function("filter",
                optional_override([](Cell &self, const val &layers,
                                     const val &types, const val &operation,
                                     bool polygons, bool paths, bool labels) {
                  return cell_filter(self, layers, types, operation, polygons,
                                     paths, labels);
                }))
      .function("filter",
                optional_override([](Cell &self, const val &layers,
                                     const val &types, const val &operation) {
                  return cell_filter(self, layers, types, operation);
                }))
      .function(
          "dependencies", optional_override([](Cell &self, bool recursive) {
            gdstk::Map<Cell *> cell_map = {0};
            gdstk::Map<RawCell *> rawcell_map = {0};
            self.get_dependencies(recursive > 0, cell_map);
            self.get_raw_dependencies(recursive > 0, rawcell_map);
            val result = val::array();
            for (gdstk::MapItem<Cell *> *item = cell_map.next(NULL);
                 item != NULL; item = cell_map.next(item)) {
              auto cell = std::find_if(
                  utils::REF_KEEP_ALIVE_CELL.begin(),
                  utils::REF_KEEP_ALIVE_CELL.end(),
                  [&item](
                      std::pair<Reference *const, std::shared_ptr<Cell>> &ele) {
                    if (item->value == ele.second.get()) {
                      return true;
                    }
                    return false;
                  });
              if (cell != utils::REF_KEEP_ALIVE_CELL.end()) {
                result.call<void>("push", val(cell->second));
              } else {
                cell_map.clear();
                rawcell_map.clear();
                throw std::runtime_error("No valid Cell found for Reference");
              }
            }
            cell_map.clear();
            for (gdstk::MapItem<RawCell *> *item = rawcell_map.next(NULL);
                 item != NULL; item = rawcell_map.next(item)) {
              auto cell = std::find_if(
                  utils::REF_KEEP_ALIVE_RAWCELL.begin(),
                  utils::REF_KEEP_ALIVE_RAWCELL.end(),
                  [&item](std::pair<Reference *const, std::shared_ptr<RawCell>>
                              &ele) {
                    if (item->value == ele.second.get()) {
                      return true;
                    }
                    return false;
                  });
              if (cell != utils::REF_KEEP_ALIVE_RAWCELL.end()) {
                result.call<void>("push", val(cell->second));
              } else {
                cell_map.clear();
                rawcell_map.clear();
                throw std::runtime_error(
                    "No valid RawCell found for Reference");
              }
            }
            rawcell_map.clear();
            return result;
          }))
// TODO:
//   .function("set_property")
//   .function("get_property")
//   .function("delete_property")
#ifndef NDEBUG
      .function("print", &Cell::print)
#endif
      ;
}
