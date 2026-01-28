cmake_minimum_required(VERSION 3.20)
project(app LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(
  -Wall -Wextra -Wpedantic 
)

option(ENABLE_TRACE "Enable tracing" OFF)

add_executable(app
  src/main.cpp src/core/protocol.cpp src/core/trace.cpp src/net/server.cpp
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED IMPORTED_TARGET liburing)

if (ENABLE_TRACE)
  target_compile_definitions(app PRIVATE ENABLE_TRACE=1)
endif()

target_include_directories(app PRIVATE include)
target_link_libraries(app PRIVATE PkgConfig::LIBURING)


