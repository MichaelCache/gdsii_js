#include "binding_utils.h"
#include "gdstk_base_bind.h"

void gdstk_repetition_bind() {
  class_<Repetition>("Repetition")
      .smart_ptr<std::shared_ptr<Repetition>>("Repetition_shared_ptr")
      .constructor(optional_override([](const val& columns, const val& rows,
                                        const val& spacing, const val& v1,
                                        const val& v2, const val& offsets,
                                        const val& x_offsets,
                                        const val& y_offsets) {
        auto repetition = std::shared_ptr<Repetition>(
            (Repetition*)gdstk::allocate_clear(sizeof(Repetition)),
            utils::RepetitionDeleter());

        if (!columns.isNull() && columns.as<int>() > 0 && !rows.isNull() &&
            rows.as<int>() > 0 && !spacing.isNull()) {
          repetition->type = RepetitionType::Rectangular;
          repetition->columns = columns.as<int>();
          repetition->rows = rows.as<int>();
          repetition->spacing = to_vec2(spacing);
        } else if (!columns.isNull() && columns.as<int>() > 0 &&
                   !rows.isNull() && rows.as<int>() > 0 && !v1.isNull() &&
                   !v2.isNull()) {
          repetition->type = RepetitionType::Regular;
          repetition->columns = columns.as<int>();
          repetition->rows = rows.as<int>();
          repetition->v1 = to_vec2(v1);
          repetition->v2 = to_vec2(v2);
        } else if (!offsets.isNull()) {
          repetition->type = RepetitionType::Explicit;
          repetition->offsets = *utils::js_array2gdstk_arrayvec2(offsets);
        } else if (!x_offsets.isNull()) {
          repetition->type = RepetitionType::ExplicitX;
          repetition->coords = *utils::js_array2gdstk_array<double>(x_offsets);
        } else if (!y_offsets.isNull()) {
          repetition->type = RepetitionType::ExplicitY;
          repetition->coords = *utils::js_array2gdstk_array<double>(y_offsets);
        } else {
          throw std::runtime_error(
              "Repetition type undefined. Please define either "
              "columns + rows + spacing, columns + rows + v1 + "
              "v2, offsets, x_offsets, or y_offsets.");
        }

        return repetition;
      }))
      .property("size", optional_override([](const Repetition& self) {
                  return self.get_count();
                }))
      .property("columns", optional_override([](const Repetition& self) {
                  if (self.type == RepetitionType::Rectangular ||
                      self.type == RepetitionType::Regular) {
                    return val(self.columns);
                  }
                  return val::null();
                }))
      .property("rows", optional_override([](const Repetition& self) {
                  if (self.type == RepetitionType::Rectangular ||
                      self.type == RepetitionType::Regular) {
                    return val(self.rows);
                  }
                  return val::null();
                }))

      .property("spacing", optional_override([](const Repetition& self) {
                  if (self.type == RepetitionType::Rectangular) {
                    return vec2_to_js_array(self.spacing);
                  }
                  return val::null();
                }))
      .property("v1", optional_override([](const Repetition& self) {
                  if (self.type == RepetitionType::Regular) {
                    return vec2_to_js_array(self.v1);
                  }
                  return val::null();
                }))
      .property("v2", optional_override([](const Repetition& self) {
                  if (self.type == RepetitionType::Regular) {
                    return vec2_to_js_array(self.v2);
                  }
                  return val::null();
                }))
      .property("offsets", optional_override([](const Repetition& self) {
                  if (self.type == RepetitionType::Explicit) {
                    return utils::gdstk_array2js_array_by_value(self.offsets);
                  }
                  return val::null();
                }))
      .property("x_offsets", optional_override([](const Repetition& self) {
                  if (self.type == RepetitionType::ExplicitX) {
                    return utils::gdstk_array2js_array_by_value(self.coords);
                  }
                  return val::null();
                }))
      .property("y_offsets", optional_override([](const Repetition& self) {
                  if (self.type == RepetitionType::ExplicitX) {
                    return utils::gdstk_array2js_array_by_value(self.coords);
                  }
                  return val::null();
                }))
      .function("get_offsets", optional_override([](const Repetition& self) {
                  Array<Vec2> offsets = {0};
                  self.get_offsets(offsets);
                  auto reuslt = utils::gdstk_array2js_array_by_value(offsets);
                  offsets.clear();
                  return reuslt;
                }))
#ifndef NDEBUG
      .function("print", &Repetition::print)
#endif
      ;
}