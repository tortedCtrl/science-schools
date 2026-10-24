cmake_minimum_required(VERSION 3.28)
project(scientific_schools VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ENABLE_CLANG_TIDY "Run clang-tidy as part of the build" OFF)
option(WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# ---------------------------------------------------------------------------
# Dependencies (vcpkg manifest mode, see vcpkg.json)
# ---------------------------------------------------------------------------
# asio, beast and mysql are header-only, but mysql's text protocol uses the
# compiled Boost.Charconv for float/double parsing and formatting.
find_package(Boost REQUIRED COMPONENTS json charconv)
find_package(OpenSSL REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(OpenMP REQUIRED COMPONENTS CXX)
find_package(Threads REQUIRED)

# GVE-Leiden (puzzlef/leiden-communities-openmp) is a header-only, non-CMake
# project, so it is fetched but never configured. Pin LEIDEN_GIT_TAG to a commit
# hash for reproducible builds (then GIT_SHALLOW must be turned off).
include(FetchContent)
set(LEIDEN_GIT_TAG "main" CACHE STRING "Tag/branch of leiden-communities-openmp")
FetchContent_Declare(
  leiden_omp
  GIT_REPOSITORY https://github.com/puzzlef/leiden-communities-openmp.git
  GIT_TAG ${LEIDEN_GIT_TAG}
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR do-not-configure)
FetchContent_MakeAvailable(leiden_omp)

# ---------------------------------------------------------------------------
# clang-tidy (enabled with -DENABLE_CLANG_TIDY=ON)
# ---------------------------------------------------------------------------
if(ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy-20 clang-tidy REQUIRED)
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--extra-arg=-Wno-unknown-warning-option")
endif()

function(schools_set_warnings target)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
  if(WARNINGS_AS_ERRORS)
    target_compile_options(${target} PRIVATE -Werror)
  endif()
endfunction()

# ---------------------------------------------------------------------------
# Leiden wrapper: the only translation unit that sees third-party headers.
# Warnings and clang-tidy are disabled here on purpose.
# ---------------------------------------------------------------------------
add_library(schools_leiden STATIC src/graph/leiden_openmp.cpp)
target_include_directories(schools_leiden PUBLIC src)
target_include_directories(schools_leiden PRIVATE ${leiden_omp_SOURCE_DIR})
# error.hpp pulls in <fmt/format.h>; link fmt so this TU sees the same fmt
# version as the rest of the project (not the system one).
target_link_libraries(schools_leiden PUBLIC fmt::fmt OpenMP::OpenMP_CXX)
target_compile_options(schools_leiden PRIVATE -w)
set_target_properties(schools_leiden PROPERTIES CXX_CLANG_TIDY "")

# ---------------------------------------------------------------------------
# Core library (api / service / repository / openalex / common)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SCHOOLS_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER SCHOOLS_SOURCES EXCLUDE REGEX "src/main\\.cpp$|src/graph/leiden_openmp\\.cpp$")

add_library(schools_core STATIC ${SCHOOLS_SOURCES})
target_include_directories(schools_core PUBLIC src)
target_compile_definitions(schools_core PUBLIC BOOST_ASIO_NO_DEPRECATED)
target_link_libraries(
  schools_core
  PUBLIC Boost::headers
         Boost::json
         Boost::charconv
         OpenSSL::SSL
         OpenSSL::Crypto
         fmt::fmt
         spdlog::spdlog
         Threads::Threads
         schools_leiden)
schools_set_warnings(schools_core)

add_executable(scientific_schools src/main.cpp)
target_link_libraries(scientific_schools PRIVATE schools_core)
schools_set_warnings(scientific_schools)

install(TARGETS scientific_schools RUNTIME DESTINATION bin)
