cmake_minimum_required(VERSION 3.22)

# ВАЖНО: Тулчейн vcpkg объявляется ДО вызова команды project()
set(CMAKE_TOOLCHAIN_FILE "/home/belozubova/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

project(BackendProject VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

add_subdirectory(src)
add_subdirectory(apps/server)
