add_library(backend_core STATIC)

target_sources(
    backend_core
    PRIVATE
    api/routes.cpp
    common/config.cpp
    common/rate_limiter.cpp
    common/url.cpp
    graph_engine/leiden_wrapper.cpp
    openalex/client.cpp
    openalex/parser.cpp
    repository/author_repository.cpp
    repository/community_repository.cpp
    repository/database.cpp
    repository/graph_repository.cpp
    repository/ingest_repository.cpp
    repository/schema.cpp
    service/ingest_service.cpp
    service/query_service.cpp
)

target_include_directories(
    backend_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    backend_core
    PUBLIC
        fmt::fmt
        spdlog::spdlog
        SOCI::SOCI
        httplib::httplib

    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
        gve_leiden
)
