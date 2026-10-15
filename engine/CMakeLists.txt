cmake_minimum_required(VERSION 3.16)
project(astc CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra)

add_library(astcore
  src/ast.cpp
  src/lexer.cpp
  src/parser.cpp
  src/json.cpp
  src/comments.cpp
  src/printer.cpp
)
target_include_directories(astcore PUBLIC src)

add_executable(astc src/main.cpp)
target_link_libraries(astc astcore)

enable_testing()
# Every tests/test_*.cpp becomes its own test executable.
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/test_*.cpp)
foreach(test_src ${TEST_SOURCES})
  get_filename_component(test_name ${test_src} NAME_WE)
  add_executable(${test_name} ${test_src})
  target_link_libraries(${test_name} astcore)
  target_compile_definitions(${test_name} PRIVATE SAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../samples/engine")
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
