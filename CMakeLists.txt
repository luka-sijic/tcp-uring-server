cmake_minimum_required(VERSION 3.20)
project(app LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
  -Wall -Wextra -Wpedantic 
)

option(ENABLE_TRACE "Enable tracing" OFF)


add_executable(app
  main.cpp protocol.cpp trace.cpp
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED IMPORTED_TARGET liburing)

if (ENABLE_TRACE)
  target_compile_definitions(app PRIVATE ENABLE_TRACE=1)
endif()

target_link_libraries(app PRIVATE PkgConfig::LIBURING)


