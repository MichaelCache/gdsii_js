#include "gdstk_base_bind.h"

#include "binding_utils.h"

extern "C" {

EM_JS(EM_VAL, make_arrayvec2_proxy, (EM_VAL array), {
  var proxy = new Proxy(Emval.toValue(array), {
set:
  function(target, property, value, receiver) {
    if (!isNaN(property)) {
      target.set(parseInt(property), value);
      return true;
    } 
  target[property] = value;
  return true;
}
, get : function(target, property, receiver) {
  if (!isNaN(property)) {
    return target.get(parseInt(property));
  }
  return target[property];
}
});

return Emval.toHandle(proxy);
});

EM_JS(EM_VAL, make_vec2_proxy, (EM_VAL array), {
  var proxy = new Proxy(Emval.toValue(array), {
 set : function(target, property, value, receiver) {
  if (!isNaN(property)) {
      let index = parseInt(property);
      if (index === 0) {
    target.x = value;
    return true;
}else if (index === 1) {
    target.y = value;
    return true;
}
else {
    throw "set index out of range";
}

}
target[property] = value;
return true;
}
, get : function(target, property, receiver) {
  if (!isNaN(property)) {
    let index = parseInt(property);
    if (index === 0) {
      return target.x;
    } else if (index === 1) {
      return target.y;
    } else {
      throw "get index out of range";
    }
  }
  return target[property];
}
});

return Emval.toHandle(proxy);
});
}

Vec2 to_vec2(const val& obj) {
  if (obj.isArray()) {
    return utils::js_array2vec2(obj);
  } else if (obj["constructor"]["name"].as<std::string>() == "Point") {
    return obj.as<Vec2>();
  } else {
    throw std::runtime_error("Can't convert js obje to gdstk::Vec2");
  }
}

std::shared_ptr<Array<Vec2>> to_array_vec2(const val& array) {
  if (array.isArray()) {
    auto points_array = utils::make_gdstk_array<Vec2>();
    auto length = array["length"].as<int>();
    for (size_t i = 0; i < length; i++) {
      points_array->append(to_vec2(array[i]));
    }
    return points_array;
  } else if (array["constructor"]["name"].as<std::string>() == "PointsArray") {
    return array.as<std::shared_ptr<Array<Vec2>>>();
  } else {
    throw std::runtime_error("Can't convert js obje to gdstk::Array<Vec2>");
  }
}

val vec2ref_to_js_proxy(std::shared_ptr<Vec2> vec) {
  return val::take_ownership(make_vec2_proxy(val(vec).as_handle()));
}

val arrayvec2ref_to_js_proxy(std::shared_ptr<Array<Vec2>> array) {
  return val::take_ownership(make_arrayvec2_proxy(val(array).as_handle()));
}

val vec2_to_js_array(const Vec2& vec){
    auto array = val::array();
    array.call<void>("push", vec.x);
    array.call<void>("push", vec.y);
    return array;
};

// ----------------------------------------------------------------------------
void gdstk_base_bind() {
  // value_array<Vec2>("Point").element(&Vec2::x).element(&Vec2::y);
  class_<Vec2>("Point")
      .smart_ptr<std::shared_ptr<Vec2>>("Point_shared_ptr")
      .constructor(optional_override([](double x, double y) {
        return std::shared_ptr<Vec2>(new Vec2{x, y});
      }))
      .constructor(optional_override([](const val& point) {
        if (point.isArray()) {
          return std::shared_ptr<Vec2>(
              new Vec2{point[0].as<double>(), point[1].as<double>()});
        } else if (point["constructor"]["name"].as<std::string>() == "Point") {
          return std::shared_ptr<Vec2>(
              new Vec2{point.as<Vec2>().x, point.as<Vec2>().y});
        } else {
          throw std::runtime_error("Constructing Point only accepte array");
        }
      }))
      .property("x", &Vec2::x)
      .property("y", &Vec2::y);

  class_<Array<Vec2>>("PointsArray")
      .smart_ptr<std::shared_ptr<Array<Vec2>>>("PointsArray_shared_ptr")
      .constructor(optional_override([]() {
        return std::shared_ptr<Array<Vec2>>(
            (Array<Vec2>*)gdstk::allocate_clear(sizeof(Array<Vec2>)),
            utils::ArrayDeleter());
      }))
      .constructor(optional_override([](const val& array) {
        assert(array.isArray() ||
               array["constructor"]["name"].as<std::string>() == "PointsArray");
        auto points_array = std::shared_ptr<Array<Vec2>>(
            (Array<Vec2>*)gdstk::allocate_clear(sizeof(Array<Vec2>)),
            utils::ArrayDeleter());
        auto length = array["length"].as<int>();
        for (size_t i = 0; i < length; i++) {
          points_array->append(to_vec2(array[i]));
        }
        return points_array;
      }))
      .property("length", optional_override([](const Array<Vec2>& self) {
                  return int(self.count);
                }))
      .function("push",
                optional_override([](Array<Vec2>& self, const val& point) {
                  self.append(to_vec2(point));
                }))
      .function("pop", optional_override([](Array<Vec2>& self) {
                  if (self.count > 0) {
                    Vec2 r = self[self.count - 1];
                    self.remove(self.count - 1);
                    return vec2_to_js_array(r);
                  }
                  return val::null();
                }))
      .function(
          "get", optional_override([](Array<Vec2>& self, int idx) {
            if (idx < 0 || idx >= self.count) {
              return val::null();
            }
            auto point =
                std::shared_ptr<Vec2>(&self.operator[](idx), utils::nodelete());
            return val::take_ownership(make_vec2_proxy(val(point).as_handle()));
          }),
          allow_raw_pointers())
      .function("set", optional_override([](Array<Vec2>& self, int idx,
                                            const val& point) {
                  if (idx < 0 || idx >= self.count) {
                    throw std::runtime_error("set point array out of range");
                  }
                  self.operator[](idx) = to_vec2(point);
                }))
#ifndef NDEBUG
      .function("print", &Array<Vec2>::print)
#endif
      ;
}