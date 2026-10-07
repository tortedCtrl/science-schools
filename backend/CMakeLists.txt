cmake_minimum_required(VERSION 3.22)

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

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(
    SOCI 4.1
    COMPONENTS Core PostgreSQL
    CONFIG REQUIRED
)

find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

add_subdirectory(src)
add_subdirectory(apps/server)