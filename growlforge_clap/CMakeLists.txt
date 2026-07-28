cmake_minimum_required(VERSION 3.20)
project(GrowlForge VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
FetchContent_Declare(
  clap
  GIT_REPOSITORY https://github.com/free-audio/clap.git
  GIT_TAG 1.2.7
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(clap)

add_library(GrowlForge MODULE src/GrowlForge.cpp)
target_include_directories(GrowlForge PRIVATE ${clap_SOURCE_DIR}/include)

if(MSVC)
  target_compile_options(GrowlForge PRIVATE /W4 /permissive-)
else()
  target_compile_options(GrowlForge PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(GrowlForge PROPERTIES
  PREFIX ""
  OUTPUT_NAME "GrowlForge"
  SUFFIX ".clap"
)

if(APPLE)
  set_target_properties(GrowlForge PROPERTIES BUNDLE TRUE BUNDLE_EXTENSION "clap")
endif()

install(TARGETS GrowlForge
  LIBRARY DESTINATION .
  RUNTIME DESTINATION .
  BUNDLE DESTINATION .
)
