add_executable(backend_server main.cpp routes.cpp)
target_link_libraries(backend_server PRIVATE backend_core)
