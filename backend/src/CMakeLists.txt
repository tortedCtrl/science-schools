add_library(backend_core STATIC)

# Добавляем новые .cpp файлы проекта в сборку
target_sources(backend_core PRIVATE 
    core.cpp
    graph_engine/leiden_wrapper.cpp
    database/db_client.cpp
)

target_include_directories(backend_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Подключаем папку алгоритма как системную, чтобы игнорировать её ворнинги
target_include_directories(backend_core SYSTEM PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/graph_engine/3rdparty/gve_leiden/inc
)

find_package(fmt CONFIG REQUIRED)
target_link_libraries(backend_core PUBLIC fmt::fmt)

# Поиск и подключение OpenMP для параллельных вычислений
find_package(OpenMP REQUIRED)
target_link_libraries(backend_core PRIVATE OpenMP::OpenMP_CXX)
