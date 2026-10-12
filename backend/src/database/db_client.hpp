#pragma once

#include <vector>
#include <string>
#include <utility>
#include <cstdint>
#include "api/openalex_client.hpp"
#include <soci/soci.h>
#include <soci/postgresql/soci-postgresql.h>
#include "graph_engine/leiden_wrapper.hpp"
// Структура ребра для передачи в граф
// struct CoauthorshipEdge {
//     uint32_t source_id;
//     uint32_t target_id;
//     int weight;
// };

class DBClient {
private:
    std::string connection_string;
    soci::session sql_;

public:
    explicit DBClient(const std::string& conn_str);
    
    // 1. Выгрузка графа для алгоритма Лейдена
    std::vector<CoauthorshipEdge> get_coauthorship_edges();

    // 2. Запись результатов кластеризации обратно в БД
    void update_communities(const std::vector<uint32_t>& communities);

    // 3. Быстрый запрос для API (фронтенда) при клике на автора
    std::string get_author_card_json(uint32_t author_id);
    std::string get_all_authors_json();
    void clear_import_data();
    void insert_author(int id, const std::string& openalex_id, const std::string& name, const std::string& institution_name);
    void insert_article(const std::string& id, const std::string& title, int year);
    void insert_author_article(int author_id, const std::string& article_id);
    void insert_edge(int source, int target, int weight);
};
