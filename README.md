# gdstk_js

## Intro
Javascript wasm wrapper for [gdstk](https://github.com/heitzmann/gdstk). We use [emscritpen sdk](https://github.com/emscripten-core/emsdk) wrapper gdstk as a Javascript pacakage, so that we can use gdstk functions in web.

## How to Build
Both Linux and windows is ok, emsdk and cmake is required
```shell
mkdir build
cd build
emcmake cmake ..
cmake --build .
```
You will get wasm with it's binding js in `packages` dir

Valid cmake configuration are:

```cmake
CMAKE_BUILD_TYPE // default is "Release", use -DCMAKE_BUILD_TYPE=Debug when you want to get Debug packages
EXPORT_MODULE // default is "ON", you will get pacakges named "Gdstk" in directory `packages`, if "OFF", pacakges will be named "Module"
```

## How to use


API is very similar like gdstk python interface but in Js flavor, see [gdstk documentation](https://heitzmann.github.io/gdstk/reference_python.html) for details

If you work in pure JS enviroment, you must cmake this gdstk_js project with option "-DEXPORT_MODULE=OFF", then use `<script>xxx/gdstk.js</script>` label import gdstk as `Module` object. See `examples`

If you work in node.js enviroment, you must cmake this gdstk_js project with option "-DEXPORT_MODULE=ON"(it's default value), then use `require` to import gdstk as `Gdstk` object.

## Pitfalls

- If you want set one of default value of interface, you must give all value for default parameter
- In web broswer, it's hard to operate local file directly. You can use [`FS`](https://emscripten.org/docs/api_reference/Filesystem-API.html) object offered by emscripten to achieve that. Or just use `upload_file` and `download_file` js function of gdstk_js package. Notic: if use `FS`, should wrapper with `Module.FS` or `Gdstk.FS` base on you build type.
- `Polygon.points` is a js array like proxy object. Support modify points value in place as js array ways, but not support iter yet.
- When access properties like `Polygon.get_points`(in form of js array) in iteration, it's better save proerties as a variable, then iter it. Otherwise may cause huge useage of memory and CPU performance.
- `FlexPath.commands` accept array of commands instead of variable length of parameters. same of `Curve.commands`
- Some method like `Plygon.get_properties` not implemented yet.
- Some Class like `RawCell, GdsWriter` not implemented yet.