cmake_minimum_required(VERSION 3.30)

if(DEFINED ENV{VCPKG_ROOT})
    set(
        CMAKE_TOOLCHAIN_FILE
        "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING ""
    )
endif()

project(
    BackendProject
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(
    SOCI 4.1
    COMPONENTS Core PostgreSQL
    CONFIG REQUIRED
)

find_package(CURL REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(OpenMP REQUIRED)
find_package(ZLIB REQUIRED)

# GVE-Leiden is header-only and has no CMakeLists.txt of its own. FetchContent
# downloads the exact revision and the interface target exposes its headers.
include(FetchContent)
FetchContent_Declare(
    gve_leiden
    GIT_REPOSITORY https://github.com/puzzlef/leiden-communities-openmp.git
    GIT_TAG f741a40717a1b207d53289c5cdda36ce858477a7
)
FetchContent_MakeAvailable(gve_leiden)

set(SNAP_SOURCE_DIR "" CACHE PATH "Directory containing snap-core and snap-adv")
if(NOT SNAP_SOURCE_DIR)
    FetchContent_Declare(
        snap_cpp
        GIT_REPOSITORY https://github.com/snap-stanford/snap.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(snap_cpp)
    set(SNAP_SOURCE_DIR "${snap_cpp_SOURCE_DIR}")
endif()
if(NOT EXISTS "${SNAP_SOURCE_DIR}/snap-adv/agmfast.h")
    message(FATAL_ERROR "SNAP_SOURCE_DIR must point to a SNAP source checkout")
endif()
# Fix upstream SNAP write() bug
set(_snap_fl "${SNAP_SOURCE_DIR}/glib-core/fl.cpp")

file(READ "${_snap_fl}" _snap_fl_content)

string(REPLACE
    [=[write(output, '\0', 1);]=]
    [=[write(output, "\0", 1);]=]
    _snap_fl_fixed "${_snap_fl_content}"
)

if(NOT _snap_fl_fixed STREQUAL _snap_fl_content)
    file(WRITE "${_snap_fl}" "${_snap_fl_fixed}")
endif()

unset(_snap_fl)
unset(_snap_fl_content)
unset(_snap_fl_fixed)

add_library(snap_bigclam STATIC
    "${SNAP_SOURCE_DIR}/snap-core/Snap.cpp"
    "${SNAP_SOURCE_DIR}/snap-adv/agm.cpp"
    "${SNAP_SOURCE_DIR}/snap-adv/agmfit.cpp"
    "${SNAP_SOURCE_DIR}/snap-adv/agmfast.cpp"
)
target_include_directories(snap_bigclam SYSTEM PUBLIC
    "${SNAP_SOURCE_DIR}/glib-core"
    "${SNAP_SOURCE_DIR}/snap-core"
    "${SNAP_SOURCE_DIR}/snap-adv"
)

set_target_properties(snap_bigclam PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
target_compile_definitions(snap_bigclam PUBLIC USE_OPENMP)
target_link_libraries(snap_bigclam PUBLIC OpenMP::OpenMP_CXX ZLIB::ZLIB)

# Adapter between C++26 backend and C++17 SNAP
add_library(snap_bigclam_bridge STATIC
    src/graph_engine/snap_bigclam_bridge.cpp
)

target_include_directories(snap_bigclam_bridge PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(snap_bigclam_bridge PRIVATE snap_bigclam)

set_target_properties(snap_bigclam_bridge PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

add_library(gve_leiden INTERFACE)
target_include_directories(
    gve_leiden
    SYSTEM INTERFACE
    "${gve_leiden_SOURCE_DIR}/inc"
)
target_compile_definitions(gve_leiden INTERFACE OPENMP)
target_link_libraries(gve_leiden INTERFACE OpenMP::OpenMP_CXX)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
    )
endif()

add_subdirectory(src)
add_subdirectory(apps/server)