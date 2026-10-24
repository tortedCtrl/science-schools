# Scientific Schools — backend

Бэкенд для построения графов научных школ российских учёных по данным
[OpenAlex](https://openalex.org/). Только каркас, фронтенд не входит.

Поток данных: **OpenAlex → MySQL → граф соавторств → Leiden → MySQL → HTTP API**.

## Архитектура

```
api/         Beast HTTP-сервер, роутер, контроллер (JSON, коды ошибок)
service/     ingest / clustering / query — сценарии использования
repository/  Boost.MySQL: схема, bulk-вставки, запросы
openalex/    HTTPS-клиент (Beast), парсер ответа, ретраи
graph/       обёртка над Leiden (puzzlef/leiden-communities-openmp)
common/      Result/Error, Config, rate limiter, async-хелперы
```

### Ошибки без исключений
Все функции возвращают `std::expected` (`Result<T>`, `Status`). Ошибки Boost
(Asio, Beast, MySQL, JSON) превращаются в `Error` на границе:
`as_tuple(use_awaitable)`, `error_code`+`diagnostics`, `json::parse(ec)`.
Единственный `try/catch` — в `graph/leiden_openmp.cpp`, вокруг стороннего
кода Leiden. API-слой мапит `Errc` → HTTP: 400 / 404 / 503, остальное 500.

### Почему нет mutex / condition_variable и синхронной БД
В первой версии был самописный пул синхронных соединений (mutex + condvar) и
блокирующий thread pool для HTTP-обработчиков. Теперь:
* БД — `boost::mysql::connection_pool`: асинхронный и потокобезопасный;
  корутина делает `co_await` на соединение и просто возобновляется, когда оно
  свободно. Потоки не блокируются.
* Rate limiter — атомарный CAS на «следующий слот», без мьютекса.
* Блокирующий `RunBlocking` нужен только для CPU-работы (Leiden) и идёт на
  отдельный `thread_pool(1)`, чтобы не занимать I/O-потоки.

### Загрузка данных
Курсорный поток нельзя распараллелить, поэтому диапазон лет режется на
месячные партиции; `INGEST_CONCURRENCY` корутин берут партиции и читают их по
своему keep-alive HTTPS-соединению. Общий token-bucket держит лимит запросов.
Страница сохраняется одной транзакцией: multi-row `INSERT ... ON DUPLICATE KEY
UPDATE` пачками по 2000 строк + курсор партиции. После падения загрузка
продолжается с сохранённого курсора. В граф попадают только авторы с RU
аффилиацией.

### Граф и сообщества
Вес ребра = число совместных работ (self-join по `authorships` внутри MySQL;
работы с более чем `MAX_RU_AUTHORS_PER_WORK` RU-авторов пропускаются).
Производные таблицы пересобираются через `CREATE TABLE ... LIKE` + атомарный
`RENAME TABLE`. Leiden (OpenMP) → сообщества нумеруются по размеру (0 — самое
большое) → запись в БД, `active_run` переключается атомарно, хранятся 2 последних
запуска.

## API

| Метод | Путь |
|---|---|
| GET | `/api/v1/health` |
| GET | `/api/v1/authors/{openalex_id}/community` |
| GET | `/api/v1/communities?limit=50` |
| GET | `/api/v1/communities/{id}?limit=100&offset=0` |
| GET | `/api/v1/graph?community_id=7&limit=500&min_weight=2` |

`/graph` отдаёт не более 2000 узлов (самые «связные»), поле `truncated` говорит,
что данные обрезаны.

## Локальная сборка

Нужно: **gcc 15**, CMake ≥ 3.28, Ninja, git, [vcpkg](https://github.com/microsoft/vcpkg),
`bison flex` (для сборки зависимостей), MySQL 8 / MariaDB для запуска.

```bash
git clone https://github.com/microsoft/vcpkg ~/vcpkg && ~/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=~/vcpkg
export CC=gcc-15 CXX=g++-15

cmake --preset dev          # ставит зависимости через vcpkg, создаёт build/dev
cmake --build --preset dev
./build/dev/scientific_schools --help
```

Release: `cmake --preset release && cmake --build --preset release`.
clang-tidy при сборке: `-DENABLE_CLANG_TIDY=ON`. Форматирование:
`find src -name '*.[ch]pp' | xargs clang-format -i`.

### Запуск

```bash
docker compose up -d db                      # MySQL (порт 3306)
export OPENALEX_MAILTO=you@example.com       # «вежливый» пул OpenAlex
B=./build/dev/scientific_schools
$B migrate
$B ingest --from-year=2020 --to-year=2025    # можно прерывать и запускать снова
$B cluster
$B serve                                     # http://localhost:8080/api/v1/health
```

Или всё в Docker: `docker compose up --build`, джобы —
`docker compose --profile jobs run --rm ingest --from-year=2020`.

### Переменные окружения
`DB_HOST DB_PORT DB_USER DB_PASSWORD DB_NAME DB_POOL_SIZE`, `LOG_LEVEL`,
`IO_THREADS`, `OPENALEX_HOST OPENALEX_MAILTO OPENALEX_API_KEY OPENALEX_PAGE_SIZE OPENALEX_RPS`,
`COUNTRY_CODE`, `INGEST_CONCURRENCY`, `MAX_RU_AUTHORS_PER_WORK`,
`LEIDEN_RESOLUTION LEIDEN_THREADS`, `HTTP_ADDRESS HTTP_PORT`.

## Zed / clangd («чтобы всё не горело красным»)

clangd нужен `compile_commands.json`; его генерирует CMake (включено в
`CMakePresets.json` и `CMakeLists.txt`). Шаги:

1. Один раз выполнить `cmake --preset dev` (после этого есть `build/dev/compile_commands.json`
   и скачаны зависимости vcpkg — без них заголовки Boost не найдутся).
2. Открыть папку проекта в Zed. Уже лежат готовые файлы:
   * `.clangd` — указывает `CompilationDatabase: build/dev`, вырезает GCC-only флаги;
   * `.zed/settings.json` — аргументы clangd: `--compile-commands-dir=build/dev`,
     `--query-driver=...g++*` (clangd берёт системные include-пути у g++-15,
     иначе не видит libstdc++ 15 с `std::expected`), clang-tidy, формат при сохранении.
3. Если g++-15 лежит не в `/usr/bin` или `/usr/local/bin`, поправьте `--query-driver`.
4. После добавления новых `.cpp` перезапустите `cmake --preset dev` и
   `Zed: restart language server`.

Нужен clangd ≥ 18 (Zed скачивает свежий сам). Если всё же красное — запустите
`clangd --check=src/main.cpp` из корня проекта: он покажет, чего не хватает.

## Не проверено

Код **не компилировался** в среде автора (нет gcc 15/vcpkg), возможны мелкие
ошибки сборки. Допущения, которые стоит проверить: API puzzlef/leiden
(`leidenStaticOmp`, `DiGraph`) в `leiden_openmp.cpp`; ветка `main` у Leiden —
лучше зафиксировать коммит (`LEIDEN_GIT_TAG`); формат полей `authorships`/`topics`
в ответе OpenAlex (`openalex/parser.cpp`); API Boost.MySQL ≥ 1.84.
