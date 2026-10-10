add_library(backend_core STATIC)

target_sources(
    backend_core
    PRIVATE
    core.cpp
    graph_engine/leiden_wrapper.cpp
    database/db_client.cpp
    api/openalex_client.cpp
)

target_include_directories(
    backend_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(
    backend_core
    SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/graph_engine/3rdparty/gve_leiden/inc
)

find_package(fmt CONFIG REQUIRED)
find_package(OpenMP REQUIRED)

target_link_libraries(
    backend_core
    PUBLIC
        fmt::fmt
        SOCI::SOCI

    PRIVATE
        OpenMP::OpenMP_CXX
        CURL::libcurl
        nlohmann_json::nlohmann_json
)