#pragma once
#include <vector>
#include <string>
#include <utility>
#include <cstdint>

// Структура ребра для передачи в граф
struct CoauthorshipEdge {
    uint32_t source_id;
    uint32_t target_id;
    int weight;
};

class DBClient {
private:
    std::string connection_string;
    // Здесь будет объект подключения (например, pqxx::connection для PostgreSQL)

public:
    explicit DBClient(const std::string& conn_str);
    
    // 1. Выгрузка графа для алгоритма Лейдена
    std::vector<CoauthorshipEdge> get_coauthorship_graph();

    // 2. Запись результатов кластеризации обратно в БД
    void update_communities(const std::vector<uint32_t>& communities);

    // 3. Быстрый запрос для API (фронтенда) при клике на автора
    std::string get_author_card_json(uint32_t author_id);
};
