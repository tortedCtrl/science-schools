add_library(backend_core STATIC)

# Указываем файл, который мы только что создали
target_sources(backend_core PRIVATE core.cpp)

target_include_directories(backend_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(fmt CONFIG REQUIRED)
target_link_libraries(backend_core PUBLIC fmt::fmt)
