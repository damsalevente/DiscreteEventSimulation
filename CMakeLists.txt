cmake_minimum_required(VERSION 3.14)
project(DESFramework C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

add_subdirectory(framework)
add_subdirectory(apps/coffeesim)
add_subdirectory(apps/release_pipeline)
add_subdirectory(apps/config_runner)
add_subdirectory(apps/mdf_export)
add_subdirectory(apps/tui_editor)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
  add_subdirectory(tests)
endif()
