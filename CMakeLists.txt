cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_TESTS "Build unit tests for the region mapping" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

# Region-to-target mapping without OBS dependencies, shared by the plugin and the tests.
add_library(region-map STATIC src/region-map.c)
target_include_directories(region-map PUBLIC src)
target_link_libraries(region-map PUBLIC $<$<NOT:$<PLATFORM_ID:Windows>>:m>)
set_target_properties(region-map PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs region-map)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.c src/blur-region.c)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

if(ENABLE_TESTS)
  enable_testing()
  add_executable(region-map-tests tests/test-region-map.c)
  target_link_libraries(region-map-tests PRIVATE region-map)
  add_test(NAME region-map-tests COMMAND region-map-tests)
endif()
