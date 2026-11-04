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

# GVE-Leiden is header-only and has no CMakeLists.txt of its own. FetchContent
# downloads the exact revision and the interface target exposes its headers.
include(FetchContent)
FetchContent_Declare(
    gve_leiden
    GIT_REPOSITORY https://github.com/puzzlef/leiden-communities-openmp.git
    GIT_TAG f741a40717a1b207d53289c5cdda36ce858477a7
)
FetchContent_MakeAvailable(gve_leiden)

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
