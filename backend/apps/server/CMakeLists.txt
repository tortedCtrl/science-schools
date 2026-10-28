add_executable(backend_server main.cpp)
target_link_libraries(backend_server PRIVATE backend_core)

option(STATIC_RUNTIME "Link libstdc++ and libgcc statically" OFF)
if(STATIC_RUNTIME AND NOT MSVC)
    target_link_options(backend_server PRIVATE -static-libstdc++ -static-libgcc)
endif()
