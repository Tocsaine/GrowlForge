cmake_minimum_required(VERSION 3.24)
project(GrowlForge VERSION 1.0.0 LANGUAGES CXX)

add_library(GrowlForge MODULE src/GrowlForge.cpp)
target_compile_features(GrowlForge PRIVATE cxx_std_20)
target_include_directories(GrowlForge PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/clap/include")

if(MSVC)
  target_compile_options(GrowlForge PRIVATE /W4 /permissive- /EHsc /utf-8)
  target_compile_definitions(GrowlForge PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
  target_compile_options(GrowlForge PRIVATE -Wall -Wextra -Wpedantic -fvisibility=hidden)
endif()

set_target_properties(GrowlForge PROPERTIES
  PREFIX ""
  SUFFIX ".clap"
  OUTPUT_NAME "GrowlForge"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
)

install(TARGETS GrowlForge
  RUNTIME DESTINATION clap
  LIBRARY DESTINATION clap
)
