#include "leiden_wrapper.hpp"
#include "Graph.hxx"     
#include "leiden.hxx"    
#include <algorithm>
#include <iostream>
#include <omp.h> 

std::vector<uint32_t> LeidenWrapper::run_leiden(const std::vector<CoauthorshipEdge>& input_edges, double gamma) {
    if (input_edges.empty()) return {};

    uint32_t max_vertex_id = 0;
    for (const auto& edge : input_edges) {
        max_vertex_id = std::max({max_vertex_id, edge.source_id, edge.target_id});
    }
    uint32_t num_vertices = max_vertex_id + 1;

    // 1. Создаем пустой граф, как это делает сам автор в main.cxx
    DiGraph<uint32_t, None, float> g;

    // 2. Инициализируем вершины строго по порядку от 0 до N-1.
    // Метод addVertex внутри библиотеки puzzlef проставит true в массив exists
    // и выделит соответствующий LazyBitset в массиве edges.
    for (uint32_t i = 0; i < num_vertices; ++i) {
        g.addVertex(i);
    }

    // 3. Добавляем ребра соавторства. 
    // Поскольку у автора используется структура LazyBitset, мы добавляем 
    // связи в обе стороны, чтобы алгоритм корректно считал граф ненаправленным.
    for (const auto& edge : input_edges) {
        g.addEdge(edge.source_id, edge.target_id, static_cast<float>(edge.weight));
        g.addEdge(edge.target_id, edge.source_id, static_cast<float>(edge.weight));
    }

    g.update();

    std::cout << "[LeidenCore] Проверка графа внутри движка:" << std::endl;
    std::cout << "  Вершин в графе (order): " << g.order() << std::endl;
    std::cout << "  Ребер в графе (size): " << g.size() << std::endl;
    // 4. Задаем опции Лейдена. 
    // Передаем gamma (разрешение CPM) в структуру опций, которую мы нашли в leiden.hxx
    LeidenOptions options;
    options.resolution = gamma;
    options.repeat = 1;

    // 5. Вызываем расчет через базовую функцию leidenStatic
    auto result = leidenStatic(g, options); 

    // 6. Собираем результаты членства в сообществах
    std::vector<uint32_t> communities(num_vertices);
    for (uint32_t i = 0; i < num_vertices; ++i) {
        communities[i] = result.membership[i]; 
    }

    return communities;
}
