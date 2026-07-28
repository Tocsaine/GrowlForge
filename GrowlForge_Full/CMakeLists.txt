cmake_minimum_required(VERSION 3.24)
project(GrowlForge VERSION 1.1.0 LANGUAGES CXX)

if(WIN32 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR
    "GrowlForge must be built for x64. "
    "Delete the out directory and use a windows-x64 preset.")
endif()

message(STATUS "GrowlForge generator: ${CMAKE_GENERATOR}")
message(STATUS "GrowlForge generator platform: ${CMAKE_GENERATOR_PLATFORM}")
message(STATUS "GrowlForge pointer size: ${CMAKE_SIZEOF_VOID_P}")
message(STATUS "GrowlForge C++ compiler: ${CMAKE_CXX_COMPILER}")

add_library(GrowlForge MODULE src/GrowlForge.cpp)
target_compile_features(GrowlForge PRIVATE cxx_std_20)
target_include_directories(GrowlForge PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/external/clap/include"
)

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

foreach(config Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${config}" config_upper)
  set_target_properties(GrowlForge PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/plugins"
    LIBRARY_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/plugins"
  )
endforeach()

install(TARGETS GrowlForge
  RUNTIME DESTINATION clap
  LIBRARY DESTINATION clap
)
