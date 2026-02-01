cmake_minimum_required(VERSION 3.20)
project(app LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_TRACE   "Enable tracing" OFF)
option(ENABLE_ASAN    "Enable AddressSanitizer/UBSan (Debug-ish builds)" OFF)
option(ENABLE_LTO     "Enable link-time optimization (Release-ish builds)" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED IMPORTED_TARGET liburing)


add_executable(app
  src/main.cpp
  src/net/server.cpp
  src/handlers.cpp
  src/net/uring_driver.cpp
)

target_include_directories(app PRIVATE include)

add_compile_options(
  -Wall -Wextra -Wpedantic 
)

target_link_libraries(app PRIVATE PkgConfig::LIBURING)

if (ENABLE_ASAN)
  target_compile_options(app PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(app PRIVATE -fsanitize=address,undefined)
endif()

if (ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ipo_ok OUTPUT ipo_err)
  if (ipo_ok)
    set_property(TARGET app PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(WARNING "IPO/LTO not supported: ${ipo_err}")
  endif()
endif()

