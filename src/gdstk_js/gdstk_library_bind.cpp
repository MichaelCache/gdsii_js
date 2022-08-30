#include <ctime>
#include <unordered_map>
#include <unordered_set>

#include "binding_utils.h"

// js function for download gds file
EM_JS(void, download_file, (const char* name), {
  mime = "application/octet-stream";
  let filename = name;
  let content = FS.readFile(filename);

  var download_link = document.getElementById('download_gds');
  if (download_link == null) {
    download_link = document.createElement('a');
    download_link.id = 'download_gds';
    download_link.style.display = 'none';
    document.body.appendChild(download_link);
  }
  download_link.download = filename;
  download_link.href = URL.createObjectURL(new Blob([content], {
    type:
      mime
  }));
  download_link.click();
  console.log(`Downloading "${filename}" ...`);
});

namespace {

// containors keep library contains cell/rawcell object alive

static void library_replace_cell(Library* library, Cell* cell) {
  Array<Cell*>* cell_array = &library->cell_array;
  for (uint64_t i = 0; i < cell_array->count; i++) {
    Cell* c = cell_array->items[i];
    if (strcmp(cell->name, c->name) == 0) {
      cell_array->remove_unordered(i--);
    } else {
      Reference** ref_pp = c->reference_array.items;
      for (uint64_t j = c->reference_array.count; j > 0; j--, ref_pp++) {
        Reference* reference = *ref_pp;
        if (reference->type == ReferenceType::Cell) {
          if (cell != reference->cell &&
              strcmp(cell->name, reference->cell->name) == 0) {
            reference->cell = cell;
          }
        } else if (reference->type == ReferenceType::RawCell) {
          if (strcmp(cell->name, reference->rawcell->name) == 0) {
            reference->type = ReferenceType::Cell;
            reference->cell = cell;
          }
        }
      }
    }
  }
  Array<RawCell*>* rawcell_array = &library->rawcell_array;
  for (uint64_t i = 0; i < rawcell_array->count; i++) {
    RawCell* c = rawcell_array->items[i];
    if (strcmp(cell->name, c->name) == 0) {
      rawcell_array->remove_unordered(i--);
    }
  }
  library->cell_array.append(cell);

  auto old_cell = std::find_if(utils::LIB_KEEP_ALIVE_CELL.at(library).begin(),
                               utils::LIB_KEEP_ALIVE_CELL.at(library).end(),
                               [&cell](std::shared_ptr<Cell> p) {
                                 if (strcmp(p->name, cell->name)) {
                                   return true;
                                 }
                                 return false;
                               });
  if (old_cell != utils::LIB_KEEP_ALIVE_CELL.at(library).end())
    utils::LIB_KEEP_ALIVE_CELL.at(library).erase(*old_cell);
}

static void library_replace_rawcell(Library* library, RawCell* rawcell) {
  Array<Cell*>* cell_array = &library->cell_array;
  for (uint64_t i = 0; i < cell_array->count; i++) {
    Cell* c = cell_array->items[i];
    if (strcmp(rawcell->name, c->name) == 0) {
      cell_array->remove_unordered(i--);
    } else {
      Reference** ref_pp = c->reference_array.items;
      for (uint64_t j = c->reference_array.count; j > 0; j--, ref_pp++) {
        Reference* reference = *ref_pp;
        if (reference->type == ReferenceType::Cell) {
          if (strcmp(rawcell->name, reference->cell->name) == 0) {
            reference->rawcell = rawcell;
            reference->type = ReferenceType::RawCell;
          }
        } else if (reference->type == ReferenceType::RawCell) {
          if (rawcell != reference->rawcell &&
              strcmp(rawcell->name, reference->rawcell->name) == 0) {
            reference->rawcell = rawcell;
          }
        }
      }
    }
  }
  Array<RawCell*>* rawcell_array = &library->rawcell_array;
  for (uint64_t i = 0; i < rawcell_array->count; i++) {
    RawCell* c = rawcell_array->items[i];
    if (strcmp(rawcell->name, c->name) == 0) {
      rawcell_array->remove_unordered(i--);
    }
  }
  library->rawcell_array.append(rawcell);

  auto old_cell =
      std::find_if(utils::LIB_KEEP_ALIVE_RAWCELL.at(library).begin(),
                   utils::LIB_KEEP_ALIVE_RAWCELL.at(library).end(),
                   [&rawcell](std::shared_ptr<RawCell> p) {
                     if (strcmp(p->name, rawcell->name)) {
                       return true;
                     }
                     return false;
                   });
  if (old_cell != utils::LIB_KEEP_ALIVE_RAWCELL.at(library).end())
    utils::LIB_KEEP_ALIVE_RAWCELL.at(library).erase(*old_cell);
}

static val build_tag_set(const gdstk::Set<Tag>& tags) {
  val result = val::array();
  for (gdstk::SetItem<Tag>* item = tags.next(NULL); item;
       item = tags.next(item)) {
    val tag_pair = val::array();
    tag_pair.call<void>("push", gdstk::get_layer(item->value));
    tag_pair.call<void>("push", gdstk::get_type(item->value));
    result.call<void>("push", tag_pair);
  }
  return result;
}
}  // namespace

void gdstk_library_bind() {
  class_<Library>("Library")
      .smart_ptr<std::shared_ptr<Library>>("Library_shared_ptr")
      .constructor(
          optional_override([](const val& name, double unit, double precision) {
            assert(name.isString());
            if (unit <= 0) {
              throw std::runtime_error("Unit must be positive.");
            }

            if (precision <= 0) {
              throw std::runtime_error("Precision must be positive.");
            }

            auto library = std::shared_ptr<Library>(
                (Library*)gdstk::allocate_clear(sizeof(Library)),
                utils::LibraryDeleter());
            library->name =
                gdstk::copy_string(name.as<std::string>().c_str(), NULL);
            library->unit = unit;
            library->precision = precision;
            utils::LIB_KEEP_ALIVE_CELL[library.get()];
            utils::LIB_KEEP_ALIVE_RAWCELL[library.get()];
            return library;
          }))
      .constructor(optional_override([]() {
        std::string name = "library";
        double unit = 1e-6;
        double precision = 1e-9;
        auto library = std::shared_ptr<Library>(
            (Library*)gdstk::allocate_clear(sizeof(Library)),
            utils::LibraryDeleter());
        library->name = gdstk::copy_string(name.c_str(), NULL);
        library->unit = unit;
        library->precision = precision;
        utils::LIB_KEEP_ALIVE_CELL[library.get()];
        utils::LIB_KEEP_ALIVE_RAWCELL[library.get()];
        return library;
      }))
      .property("name", optional_override([](const Library& self) {
                  return val::u8string(self.name);
                }),
                optional_override([](Library& self, const val& name) {
                  assert(name.isString());
                  auto new_name = name.as<std::string>();
                  auto len = new_name.size();
                  self.name = (char*)gdstk::reallocate(self.name, ++len);
                  memcpy(self.name, new_name.c_str(), len);
                }))
      .property("unit", &Library::unit)
      .property("precision", &Library::precision)
      .property(
          "cells", optional_override([](const Library& self) {
            auto result = val::array();
            auto& cell_array =
                utils::LIB_KEEP_ALIVE_CELL.at(&const_cast<Library&>(self));
            for (auto& cell : cell_array) {
              result.call<void>("push", val(cell));
            }

            auto& rawcell_array =
                utils::LIB_KEEP_ALIVE_RAWCELL.at(&const_cast<Library&>(self));
            for (auto& cell : rawcell_array) {
              result.call<void>("push", val(cell));
            }

            return result;
          }))
      .function(
          "add", optional_override([](Library& self, const val& cells) {
            auto constr = cells["constructor"]["name"].as<std::string>();
            if (constr == "Cell") {
              self.cell_array.append(cells.as<Cell*>(allow_raw_pointers()));
              utils::LIB_KEEP_ALIVE_CELL[&self].insert(
                  cells.as<std::shared_ptr<Cell>>());
            } else if (constr == "RawCell") {
              self.rawcell_array.append(
                  cells.as<RawCell*>(allow_raw_pointers()));
              utils::LIB_KEEP_ALIVE_RAWCELL[&self].insert(
                  cells.as<std::shared_ptr<RawCell>>());
            } else if (cells.isArray()) {
              auto len = cells["length"].as<int>();
              for (size_t i = 0; i < len; i++) {
                auto cons = cells[i]["constructor"]["name"].as<std::string>();
                if (cons == "Cell") {
                  self.cell_array.append(
                      cells[i].as<Cell*>(allow_raw_pointers()));
                  utils::LIB_KEEP_ALIVE_CELL[&self].insert(
                      cells[i].as<std::shared_ptr<Cell>>());
                } else if (cons == "RawCell") {
                  self.rawcell_array.append(
                      cells[i].as<RawCell*>(allow_raw_pointers()));
                  utils::LIB_KEEP_ALIVE_RAWCELL[&self].insert(
                      cells[i].as<std::shared_ptr<RawCell>>());
                } else {
                  throw std::runtime_error(
                      "Arguments must be of type Cell or RawCell.");
                }
              }

            } else {
              throw std::runtime_error(
                  "Arguments must be of type Cell or RawCell.");
            }
          }))
      .function(
          "remove", optional_override([](Library& self, const val& cells) {
            auto constr = cells["constructor"]["name"].as<std::string>();
            if (constr == "Cell") {
              self.cell_array.remove_item(
                  cells.as<Cell*>(allow_raw_pointers()));
              utils::LIB_KEEP_ALIVE_CELL[&self].erase(
                  cells.as<std::shared_ptr<Cell>>());
            } else if (constr == "RawCell") {
              self.rawcell_array.remove_item(
                  cells.as<RawCell*>(allow_raw_pointers()));
              utils::LIB_KEEP_ALIVE_RAWCELL[&self].erase(
                  cells.as<std::shared_ptr<RawCell>>());
            }
            assert(cells.isArray());
            auto len = cells["length"].as<int>();
            for (size_t i = 0; i < len; i++) {
              auto cons = cells[i]["constructor"]["name"].as<std::string>();
              if (cons == "Cell") {
                self.cell_array.remove_item(
                    cells[i].as<Cell*>(allow_raw_pointers()));
                utils::LIB_KEEP_ALIVE_CELL[&self].erase(
                    cells[i].as<std::shared_ptr<Cell>>());
              } else if (cons == "RawCell") {
                self.rawcell_array.remove_item(
                    cells[i].as<RawCell*>(allow_raw_pointers()));
                utils::LIB_KEEP_ALIVE_RAWCELL[&self].erase(
                    cells[i].as<std::shared_ptr<RawCell>>());
              } else {
                throw std::runtime_error(
                    "Arguments must be Polygon, FlexPath, RobustPath, "
                    "Label or Reference.");
              }
            }
          }))
      .function(
          "rename_cell",
          optional_override(
              [](Library& self, const val& old_name, const val& new_name) {
                if (old_name.isString()) {
                  self.rename_cell(old_name.as<std::string>().c_str(),
                                   new_name.as<std::string>().c_str());
                } else if (old_name["constructor"]["name"].as<std::string>() ==
                           "Cell") {
                  self.rename_cell(old_name.as<Cell*>(allow_raw_pointers()),
                                   new_name.as<std::string>().c_str());
                } else {
                  throw std::runtime_error("old_name must be str or Cell");
                }
              }))
      .function(
          "replace", optional_override([](Library& self, const val& cells) {
            auto cons = cells["constructor"]["name"].as<std::string>();
            if (cons == "Cell") {
              library_replace_cell(&self,
                                   cells.as<Cell*>(allow_raw_pointers()));
              utils::LIB_KEEP_ALIVE_CELL[&self].insert(
                  cells.as<std::shared_ptr<Cell>>());
            } else if (cons == "RawCell") {
              library_replace_rawcell(&self,
                                      cells.as<RawCell*>(allow_raw_pointers()));
              utils::LIB_KEEP_ALIVE_RAWCELL[&self].insert(
                  cells.as<std::shared_ptr<RawCell>>());
            } else if (cells.isArray()) {
              auto length = cells["length"].as<int>();
              for (size_t i = 0; i < length; i++) {
                auto cons = cells[i]["constructor"]["name"].as<std::string>();
                if (cons == "Cell") {
                  library_replace_cell(
                      &self, cells[i].as<Cell*>(allow_raw_pointers()));
                  utils::LIB_KEEP_ALIVE_CELL[&self].insert(
                      cells[i].as<std::shared_ptr<Cell>>());
                } else if (cons == "RawCell") {
                  library_replace_rawcell(
                      &self, cells[i].as<RawCell*>(allow_raw_pointers()));
                  utils::LIB_KEEP_ALIVE_RAWCELL[&self].insert(
                      cells[i].as<std::shared_ptr<RawCell>>());
                } else {
                  throw std::runtime_error(
                      "Arguments must be of type Cell or RawCell.");
                }
              }
            } else {
              throw std::runtime_error(
                  "Arguments must be of type Cell or RawCell.");
            }
          }))
      .function("new_cell",
                optional_override([](Library& self, const val& name) {
                  assert(name.isString());
                  auto cell = std::shared_ptr<Cell>(
                      (Cell*)gdstk::allocate_clear(sizeof(Cell)),
                      utils::CellDeleter());
                  cell->name =
                      gdstk::copy_string(name.as<std::string>().c_str(), NULL);
                  self.cell_array.append(cell.get());
                  utils::LIB_KEEP_ALIVE_CELL[&self].insert(cell);
                  return cell;
                }))
      .function(
          "top_level", optional_override([](Library& self) {
            auto top_cells = utils::make_gdstk_array<Cell*>();
            auto top_rawcells = utils::make_gdstk_array<RawCell*>();
            self.top_level(*top_cells, *top_rawcells);

            val result = val::array();
            auto& cell_array =
                utils::LIB_KEEP_ALIVE_CELL.at(&const_cast<Library&>(self));
            for (size_t i = 0; i < top_cells->count; i++) {
              auto cell_ptr = (*top_cells)[i];
              auto cell =
                  std::find_if(cell_array.begin(), cell_array.end(),
                               [&cell_ptr](const std::shared_ptr<Cell>& c) {
                                 if (cell_ptr == c.get()) {
                                   return true;
                                 }
                                 return false;
                               });
              if (cell != cell_array.end()) {
                result.call<void>("push", *cell);
              } else {
                throw std::runtime_error("No valid Cell in Library");
              }
            }

            auto& rawcell_array =
                utils::LIB_KEEP_ALIVE_RAWCELL.at(&const_cast<Library&>(self));
            for (size_t i = 0; i < top_rawcells->count; i++) {
              auto cell_ptr = (*top_rawcells)[i];
              auto cell =
                  std::find_if(rawcell_array.begin(), rawcell_array.end(),
                               [&cell_ptr](const std::shared_ptr<RawCell>& c) {
                                 if (cell_ptr == c.get()) {
                                   return true;
                                 }
                                 return false;
                               });
              if (cell != rawcell_array.end()) {
                result.call<void>("push", *cell);
              } else {
                throw std::runtime_error("No valid RawCell in Library");
              }
            }

            return result;
          }))
      .function("layers_and_datatypes", optional_override([](Library& self) {
                  gdstk::Set<Tag> tags = {0};
                  self.get_shape_tags(tags);
                  auto r = build_tag_set(tags);
                  tags.clear();
                  return r;
                }))
      .function("layers_and_texttypes", optional_override([](Library& self) {
                  gdstk::Set<Tag> tags = {0};
                  self.get_label_tags(tags);
                  auto r = build_tag_set(tags);
                  tags.clear();
                  return r;
                }))
      .function("write_gds",
                optional_override([](Library& self, const val& outfile,
                                     int max_points, tm timestamp) {
                  auto fn = outfile.as<std::string>();
                  self.write_gds(fn.c_str(), max_points, &timestamp);
                  // download_file(fn.c_str());
                }))
      .function("write_gds",
                optional_override([](Library& self, const val& outfile) {
                  int max_points = 199;
                  auto time = std::time(nullptr);
                  auto timestamp = std::localtime(&time);
                  auto fn = outfile.as<std::string>();
                  self.write_gds(fn.c_str(), max_points, timestamp);
                  // download_file(fn.c_str());
                }))
      .function("write_oas",
                optional_override(
                    [](Library& self, const val& outfile, int compression_level,
                       bool detect_rectangles, bool detect_trapezoids,
                       double circletolerance, bool standard_properties,
                       const val& validation) {
                      uint8_t config_flags = 0;
                      if (detect_rectangles == 1)
                        config_flags |= OASIS_CONFIG_DETECT_RECTANGLES;
                      if (detect_trapezoids == 1)
                        config_flags |= OASIS_CONFIG_DETECT_TRAPEZOIDS;
                      if (standard_properties == 1)
                        config_flags |= OASIS_CONFIG_STANDARD_PROPERTIES;

                      if (!validation.isNull()) {
                        auto str = validation.as<std::string>();
                        if (str == "crc32") {
                          config_flags |= OASIS_CONFIG_INCLUDE_CRC32;
                        } else if (str == "checksum32") {
                          config_flags |= OASIS_CONFIG_INCLUDE_CHECKSUM32;
                        } else {
                          throw std::runtime_error(
                              "Argument validation must be \"crc32\", "
                              "\"checksum32\", or None.");
                        }
                      }

                      auto filename = outfile.as<std::string>();
                      self.write_oas(filename.c_str(), circletolerance,
                                     compression_level, config_flags);

                      download_file(filename.c_str());
                    }))
      .function("write_oas",
                optional_override([](Library& self, const val& outfile) {
                  int compression_level = 6;
                  bool detect_rectangles = true;
                  bool detect_trapezoids = true;
                  double circletolerance = 0;
                  bool standard_properties = false;
                  uint8_t config_flags = 0;
                  if (detect_rectangles == 1)
                    config_flags |= OASIS_CONFIG_DETECT_RECTANGLES;
                  if (detect_trapezoids == 1)
                    config_flags |= OASIS_CONFIG_DETECT_TRAPEZOIDS;
                  if (standard_properties == 1)
                    config_flags |= OASIS_CONFIG_STANDARD_PROPERTIES;

                  auto filename = outfile.as<std::string>();
                  self.write_oas(filename.c_str(), circletolerance,
                                 compression_level, config_flags);

                  download_file(filename.c_str());
                }));

  // TODO:  write_oas, set_property, get_property, delete_property

  value_object<tm>("tm")
      .field("tm_sec", &tm::tm_sec)
      .field("tm_min", &tm::tm_min)
      .field("tm_hour", &tm::tm_hour)
      .field("tm_mday", &tm::tm_mday)
      .field("tm_mon", &tm::tm_mon)
      .field("tm_year", &tm::tm_year)
      .field("tm_wday", &tm::tm_wday)
      .field("tm_yday", &tm::tm_yday)
      .field("tm_isdst", &tm::tm_isdst);
}
