option(EXPORT_MODULE "" ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include_directories("./third_party/" 
                    "./third_party/gdstk" 
                    "./third_party/gdstk/clipperlib" 
                    "./third_party/gdstk/libqhull_r")

file(GLOB_RECURSE GDSTK_SRC ./third_party/gdstk/*.cpp 
                            ./third_party/gdstk/*.c )

add_library(gdstk_lib STATIC ${GDSTK_SRC})
                  
file(GLOB_RECURSE SRC ./gdstk_*.cpp
                      ./binding_utils.cpp)

set(CMAKE_EXECUTABLE_SUFFIX ".js")
add_executable(gdstk ${SRC})
target_link_libraries(gdstk gdstk_lib)
set_property(TARGET gdstk PROPERTY CXX_STANDARD 17)

set(EXPORT_MODULE_FLAG "")
if(EXPORT_MODULE)
  set(EXPORT_MODULE_FLAG "-sMODULARIZE=1 -sEXPORT_NAME='Gdstk'")
endif()

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  set_target_properties(gdstk_lib PROPERTIES COMPILE_FLAGS "-O0 -g -sUSE_ZLIB=1 --memoryprofiler -gsource-map")
  set_target_properties(gdstk PROPERTIES LINK_FLAGS "-sUSE_ZLIB=1")
  set_target_properties(gdstk PROPERTIES COMPILE_FLAGS "-O0 -g -sUSE_ZLIB=1 --memoryprofiler -gsource-map")
  set_target_properties(gdstk PROPERTIES LINK_FLAGS "-lembind -sUSE_ZLIB=1 -sDEMANGLE_SUPPORT=1 --memoryprofiler -gsource-map -sWASM_BIGINT -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=[FS] ${EXPORT_MODULE_FLAG}")
else()
  # if not Debug, will export gdstk as a js pacakge named Gdstk
  set_target_properties(gdstk_lib PROPERTIES COMPILE_FLAGS "-O2 -g -sUSE_ZLIB=1")
  set_target_properties(gdstk_lib PROPERTIES LINK_FLAGS "-sUSE_ZLIB=1")
  set_target_properties(gdstk PROPERTIES COMPILE_FLAGS "-O2 -g -sUSE_ZLIB=1")
  set_target_properties(gdstk PROPERTIES LINK_FLAGS "-lembind -sUSE_ZLIB=1 -sWASM_BIGINT -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=[FS] ${EXPORT_MODULE_FLAG}")
endif()


set(PACAKGE_OUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../packages)
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  add_custom_command(OUTPUT COPY_WASM
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/gdstk.js ${PACAKGE_OUT_PATH}/gdstk.js
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/gdstk.wasm ${PACAKGE_OUT_PATH}/gdstk.wasm
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/gdstk.wasm.map ${PACAKGE_OUT_PATH}/gdstk.wasm.map
  )
else()
  add_custom_command(OUTPUT COPY_WASM
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/gdstk.js ${PACAKGE_OUT_PATH}/gdstk.js
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/gdstk.wasm ${PACAKGE_OUT_PATH}/gdstk.wasm
  )
endif()

add_custom_target(CopyGdstk ALL DEPENDS COPY_WASM gdstk)