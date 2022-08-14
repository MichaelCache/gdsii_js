#include "binding_utils.h"
#include "gdstk_base_bind.h"

namespace {

std::shared_ptr<Label> make_label(const val &text, const val &origin,
                                  const val &anchor = val::u8string("o"),
                                  double rotation = 0, double magnification = 1,
                                  bool x_reflection = false, int layer = 0,
                                  int texttype = 0) {
  auto label = std::shared_ptr<Label>(
      (Label *)gdstk::allocate_clear(sizeof(Label)), utils::LabelDeleter());
  label->tag = gdstk::make_tag(layer, texttype);
  label->origin = to_vec2(origin);
  if (anchor.isNull()) {
    label->anchor = Anchor::O;
  } else {
    if (!anchor.isString()) {
      throw std::runtime_error(
          "Argument anchor must be one of 'n', 's', 'e', 'w', 'o', 'ne', 'nw', "
          "'se', 'sw'.");
    }
    auto anchor_str = anchor.as<std::string>();
    int len = anchor_str.size();
    if (len == 1) {
      switch (anchor_str[0]) {
        case 'o':
          label->anchor = Anchor::O;
          break;
        case 'n':
          label->anchor = Anchor::N;
          break;
        case 's':
          label->anchor = Anchor::S;
          break;
        case 'w':
          label->anchor = Anchor::W;
          break;
        case 'e':
          label->anchor = Anchor::E;
          break;
        default:
          throw std::runtime_error(
              "Argument anchor must be one of 'n', 's', 'e', 'w', 'o', 'ne', "
              "'nw', "
              "'se', 'sw'.");
      }
    } else if (len == 2) {
      switch (anchor_str[0]) {
        case 'n': {
          switch (anchor_str[1]) {
            case 'w':
              label->anchor = Anchor::NW;
              break;
            case 'e':
              label->anchor = Anchor::NE;
              break;
            default:
              throw std::runtime_error(
                  "Argument anchor must be one of 'n', 's', 'e', 'w', 'o', "
                  "'ne', 'nw', "
                  "'se', 'sw'.");
          }
        } break;
        case 's': {
          switch (anchor_str[1]) {
            case 'w':
              label->anchor = Anchor::SW;
              break;
            case 'e':
              label->anchor = Anchor::SE;
              break;
            default:
              throw std::runtime_error(
                  "Argument anchor must be one of 'n', 's', 'e', 'w', 'o', "
                  "'ne', 'nw', "
                  "'se', 'sw'.");
          }
        } break;
        default:
          throw std::runtime_error(
              "Argument anchor must be one of 'n', 's', 'e', 'w', 'o', 'ne', "
              "'nw', "
              "'se', 'sw'.");
      }
    }
  }

  label->rotation = rotation;
  label->magnification = magnification;
  label->x_reflection = x_reflection;
  label->text = gdstk::copy_string(text.as<std::string>().c_str(), NULL);
  return label;
}

}  // namespace

void gdstk_label_bind() {
  class_<Label>("Label")
      .smart_ptr<std::shared_ptr<Label>>("Label_shared_ptr")
      .constructor(optional_override([](const val &text, const val &origin,
                                        const val &anchor, double rotation,
                                        double magnification, bool x_reflection,
                                        int layer, int texttype) {
        return make_label(text, origin, anchor, rotation, magnification,
                          x_reflection, layer, texttype);
      }))
      .constructor(optional_override([](const val &text, const val &origin) {
        return make_label(text, origin);
      }))
      .property("text", optional_override([](const Label &self) {
                  return val::u8string(self.text);
                }),
                optional_override([](Label &self, const val &text) {
                  auto text_str = text.as<std::string>();
                  int len = text_str.size();
                  self.text = (char *)gdstk::reallocate(self.text, ++len);
                  memcpy(self.text, text_str.c_str(), len);
                }))
      .property("origin", optional_override([](const Label &self) {
                  auto points = std::shared_ptr<Vec2>(
                      &const_cast<Label &>(self).origin, utils::nodelete());
                  return vec2ref_to_js_proxy(points);
                }))
      .property("anchor", optional_override([](const Label &self) {
                  auto result = val::null();
                  switch (self.anchor) {
                    case Anchor::NW:
                      result = val::u8string("nw");
                      break;
                    case Anchor::N:
                      result = val::u8string("n");
                      break;
                    case Anchor::NE:
                      result = val::u8string("ne");
                      break;
                    case Anchor::W:
                      result = val::u8string("w");
                      break;
                    case Anchor::O:
                      result = val::u8string("o");
                      break;
                    case Anchor::E:
                      result = val::u8string("e");
                      break;
                    case Anchor::SW:
                      result = val::u8string("sw");
                      break;
                    case Anchor::S:
                      result = val::u8string("s");
                      break;
                    case Anchor::SE:
                      result = val::u8string("se");
                      break;
                  }
                  return result;
                }),
                optional_override([](Label &self, const val &anchor) {
                  if (!anchor.isString()) {
                    throw std::runtime_error(
                        "Argument anchor must be one of 'n', 's', 'e', 'w', "
                        "'o', 'ne', 'nw', "
                        "'se', 'sw'.");
                  }
                  auto anchor_str = anchor.as<std::string>();
                  int len = anchor_str.size();
                  if (len == 1) {
                    switch (anchor_str[0]) {
                      case 'o':
                        self.anchor = Anchor::O;
                        break;
                      case 'n':
                        self.anchor = Anchor::N;
                        break;
                      case 's':
                        self.anchor = Anchor::S;
                        break;
                      case 'w':
                        self.anchor = Anchor::W;
                        break;
                      case 'e':
                        self.anchor = Anchor::E;
                        break;
                      default:
                        throw std::runtime_error(
                            "Argument anchor must be one of 'n', 's', 'e', "
                            "'w', 'o', 'ne', "
                            "'nw', "
                            "'se', 'sw'.");
                    }
                  } else if (len == 2) {
                    switch (anchor_str[0]) {
                      case 'n': {
                        switch (anchor_str[1]) {
                          case 'w':
                            self.anchor = Anchor::NW;
                            break;
                          case 'e':
                            self.anchor = Anchor::NE;
                            break;
                          default:
                            throw std::runtime_error(
                                "Argument anchor must be one of 'n', 's', 'e', "
                                "'w', 'o', "
                                "'ne', 'nw', "
                                "'se', 'sw'.");
                        }
                      } break;
                      case 's': {
                        switch (anchor_str[1]) {
                          case 'w':
                            self.anchor = Anchor::SW;
                            break;
                          case 'e':
                            self.anchor = Anchor::SE;
                            break;
                          default:
                            throw std::runtime_error(
                                "Argument anchor must be one of 'n', 's', 'e', "
                                "'w', 'o', "
                                "'ne', 'nw', "
                                "'se', 'sw'.");
                        }
                      } break;
                      default:
                        throw std::runtime_error(
                            "Argument anchor must be one of 'n', 's', 'e', "
                            "'w', 'o', 'ne', "
                            "'nw', "
                            "'se', 'sw'.");
                    }
                  }
                }))
      .property("rotation", &Label::rotation)
      .property("magnification", &Label::magnification)
      .property("x_reflection", &Label::x_reflection)
      .property("layer", optional_override([](const Label &self) {
                  return gdstk::get_layer(self.tag);
                }),
                optional_override([](Label &self, int layer) {
                  gdstk::set_layer(self.tag, layer);
                }))
      .property("texttype", optional_override([](const Label &self) {
                  return gdstk::get_type(self.tag);
                }),
                optional_override([](Label &self, int type) {
                  gdstk::set_type(self.tag, type);
                }))
      .property(
          "repetition", optional_override([](const Label &self) {
            auto new_repetition = std::shared_ptr<Repetition>(
                (Repetition *)gdstk::allocate_clear(sizeof(Repetition)),
                utils::RepetitionDeleter());
            new_repetition->copy_from(self.repetition);
            return new_repetition;
          }),
          optional_override([](Label &self, const val &repetition) {
            if (repetition.isNull()) {
              self.repetition.clear();
            } else if (repetition["constructor"]["name"].as<std::string>() !=
                       "Repetition") {
              throw std::runtime_error("Value must be a Repetition object.");
            }
            self.repetition.clear();
            self.repetition.copy_from(repetition.as<Repetition>());
          }))
      .function("copy", optional_override([](Label &self) {
                  auto label = std::shared_ptr<Label>(
                      (Label *)gdstk::allocate_clear(sizeof(Label)),
                      utils::LabelDeleter());
                  label->copy_from(self);
                  return label;
                }))
      .function("apply_repetition", optional_override([](Label &self) {
                  Array<Label *> array = {0};
                  self.apply_repetition(array);
                  auto result = utils::gdstk_array2js_array_by_ref(
                      array, utils::LabelDeleter());
                  array.clear();
                  return result;
                }))
      .function("set_gds_property",
                optional_override([](Label &self, int attr, const val &value) {
                  gdstk::set_gds_property(self.properties, attr,
                                          value.as<std::string>().c_str());
                }))
      .function("get_gds_property",
                optional_override([](Label &self, int attr) {
                  const PropertyValue *value =
                      gdstk::get_gds_property(self.properties, attr);
                  return val::u8string((char *)value->bytes);
                }))
      .function("delete_gds_property",
                optional_override([](Label &self, int attr) {
                  gdstk::remove_gds_property(self.properties, attr);
                }))
#ifndef NDEBUG
      .function("print", &Label::print)
#endif
      ;
}