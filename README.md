# Граф российских учёных и научных школ

Go-программа загружает статьи из OpenAlex, строит взвешенный граф соавторства, выделяет сообщества алгоритмом Leiden и сохраняет результат в Neo4j.

## Данные и модель

По умолчанию выбирается **СПбГУ** — St Petersburg University, OpenAlex `I172901346`. Для другого вуза используйте `-university` или переменную `UNIVERSITY` в `.env`. Программа загружает статьи с аффилиацией выбранного университета и включает только тех авторов, у которых **на данной статье** указан именно этот вуз с кодом страны `RU`. Это проверяемое определение по аффилиации, а не гражданство или текущее место работы. Зарубежные и другие российские соавторы этой статьи не образуют вершины в графе выбранного университета.

В Neo4j создаются:

- `(:Author {id,name,firstName,lastName,orcid,country})`;
- `(:Work {id,title,doi,year})` и `(:Author)-[:AUTHORED_AT {universityId}]->(:Work)`;
- `(:Institution {id,name,countryCode,type})` и `(:Author)-[:AFFILIATED_WITH]->(:Institution)`;
- `(:Author)-[:COAUTHORED {universityId,weight}]->(:Author)`, где `weight` — число общих статей в выбранном вузе;
- `(:School {id,universityId,method,country})` и `(:Author)-[:MEMBER_OF {universityId}]->(:School)`.

`universityId` разделяет графы разных вузов в одной базе. Ранее загруженный граф по всей России остаётся в базе, но не попадает в расчёт выбранного университета.

Одна публикация создаёт не более одной связи для каждой пары авторов, даже если в данных повторилась авторская запись. Идентификаторы вершин берутся из OpenAlex. `firstName` и `lastName` выделяются эвристически из `display_name`; точное исходное имя всегда находится в `name`. ORCID и учреждение могут отсутствовать. OpenAlex ограничивает `authorships` первыми 100 авторами статьи, поэтому связи в особенно больших коллаборациях могут быть неполными.

## Зависимости

- Go 1.24+ с включённым cgo;
- [igraph C 1.0+](https://github.com/igraph/igraph) с файлом `igraph.pc`, доступным через `pkg-config` (`brew install igraph` на macOS);
- Neo4j 5.26+ с [Query API v2](https://neo4j.com/docs/query-api/current/query/) (HTTP-порт 7474);
- ключ OpenAlex для массового сбора из [настроек API](https://openalex.org/settings/api).

Пример локального Neo4j через Docker:

```sh
docker run -d --name science-neo4j \
  -p 7474:7474 -p 7687:7687 \
  -e NEO4J_AUTH=neo4j/secret123 \
  -v science-neo4j-data:/data neo4j:5
```

## Запуск по шагам

Файл `.env` читается автоматически при запуске и исключён из Git. В нём уже указаны адрес и логин локального Neo4j, демонстрационный пароль `secret123` из примера Docker выше и `UNIVERSITY=spbu`. Если у вашей базы другой пароль, измените `NEO4J_PASSWORD`. Для массовой загрузки добавьте свой ключ в `OPENALEX_API_KEY`; переменные окружения процесса имеют приоритет над `.env`.

```sh
# Быстрая проверка на первых 100 статьях.
go run . -step=ingest -max-pages=1
go run . -step=project
go run . -step=cluster
go run . -step=stats
```

Другой университет можно выбрать по OpenAlex ID либо по названию; неоднозначный поиск выдаст список ID для уточнения:

```sh
go run . -step=all -university=I173089394 -max-pages=1
go run . -step=stats -university="ITMO University"
```

Для полного набора выбранного вуза запускайте `go run . -step=all` без `-max-pages`. `-max-pages=0` означает чтение до конца. Для каждого вуза используется отдельный файл курсора `data/openalex-<ID>-cursor.json`; повторный запуск продолжает сбор. Если меняете фильтр (`-since-year`), укажите новый `-cursor-file`. Опция `-resolution` задаёт разрешение Leiden (по умолчанию 1), `-seed` фиксирует случайность.

Полный сбор может занять долгое время и упереться в дневной бюджет OpenAlex; для очень большого объёма OpenAlex рекомендует [снимок данных](https://help.openalex.org/api/paging/). Публикации и связи сохраняются поэтапно, но шаг Leiden требует, чтобы весь граф соавторства выбранного вуза поместился в оперативную память. Статья, добавленная после завершённого сбора, потребует нового курсора или отдельного процесса обновления.

## Проверка в Neo4j

Информация об учёных СПбГУ:

```cypher
MATCH (a:Author)-[:AUTHORED_AT {universityId:'I172901346'}]->(:Work)
RETURN DISTINCT a.name, a.firstName, a.lastName, a.orcid
LIMIT 20;
```

Для графического вида в Neo4j Browser возвращайте сами вершины и связь:

```cypher
MATCH (a:Author)-[r:COAUTHORED {universityId:'I172901346'}]->(b:Author)
RETURN a, r, b LIMIT 20;
```

Если запрос пуст, выполните `go run . -step=stats`. `authorships > 0` при `coauthor edges = 0` означает, что шаг `project` ещё не запускался либо среди загруженных статей нет пары авторов выбранного вуза. Если `stats` показывает общую статью, запустите `go run . -step=project`, затем `go run . -step=cluster`.

```cypher
MATCH (s:School {universityId:'I172901346'})<-[:MEMBER_OF]-(a:Author)
RETURN s.id, count(a) AS scientists, collect(a.name)[0..10] AS examples
ORDER BY scientists DESC LIMIT 20;
```

Сообщество Leiden здесь — **кандидат на научную школу**, а не подтверждённая историческая школа с руководителем и направлением. Эти сведения из соавторства автоматически не следуют.

## Источники и выбор алгоритма

Взята готовая реализация Leiden из [igraph C](https://github.com/igraph/igraph), вызываемая через cgo. На 30 сентября 2026 года у репозитория igraph около 2 тыс. звёзд, у [vtraag/leidenalg](https://github.com/vtraag/leidenalg) около 800, у [vtraag/libleidenalg](https://github.com/vtraag/libleidenalg) около 35. Для неориентированного взвешенного графа используется `igraph_community_leiden_simple` с функцией качества modularity, разрешением 1, `beta=0.01` и двумя итерациями. API OpenAlex и параметры курсора: [документация авторств](https://help.openalex.org/data/authorships/), [пагинация](https://help.openalex.org/api/paging/), [ключ и лимиты](https://help.openalex.org/api/authentication/).
