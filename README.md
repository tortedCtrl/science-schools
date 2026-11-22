# Научные школы — Вычислительный воркер (C++)

Backend для загрузки научных данных и выделения сообществ алгоритмами
GVE-Leiden и SNAP BigCLAM.

## Требования к окружению (WSL: Ubuntu)

Для локальной сборки необходимы компилятор C++20 и OpenMP, CMake, vcpkg, а также инструменты Git, Ninja, zip/unzip, tar, curl и pkg-config.

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build git zip unzip tar curl pkg-config libomp-dev
```

## Настройка локального окружения

Скопировать содержание `/.env.example` из корня проекта в `/.env`, заполнить пароли своими значениями.

Для сборки C++ отдельно используется `science-schools/.env` (**из корня репозитория**) с локальным путём к vcpkg:

```env
VCPKG_ROOT=/home/ВАШ_ПОЛЬЗОВАТЕЛЬ_WSL/vcpkg
```


## Использование и запуск (Команды Makefile)

Для локальной сборки укажите `VCPKG_ROOT` в `.env` в корне репозитория. Сборка выполняется из директории `/backend/`.

1. **Первоначальная инициализация сборки (выполняется один раз):**
   ```bash
   make init
   ```
   *Команда автоматически сконфигурирует CMake и скачает необходимые пакеты (например, fmt) через тулчейн vcpkg.*

2. **Запуск вычислительного воркера:**
   ```bash
   make run
   ```
   *Команда выполнит инкрементальную пересборку и запустит backend. При включённом
   `INGEST_ON_START` он загрузит данные и последовательно рассчитает Leiden и
   BigCLAM.*

3. **Очистка артефактов сборки:**
   ```bash
   make clean
   ```

## Запуск модулей

PostgreSQL и backend:

    docker compose up --build -d

Только PostreSQL:

    docker compose up postgres --build -d

backend локально (из директории backend):

    make clean && make init 
    make run

frontend (из директории frontend):

    npm install
    npm run dev

## REST API

`GET /graph?algorithm=leiden|bigclam` возвращает граф: авторов, рёбра и
принадлежности авторов к сообществам выбранного алгоритма. По умолчанию
используется `leiden`. В ответе нет внутреннего `run_id`; для узла поле
`communities` содержит все его принадлежности.

`GET /communities/leiden` и `GET /communities/bigclam` возвращают сводку по
сообществам выбранного алгоритма (`community_id` и `member_count`), без
повторной выдачи графа.

`GET /communities/{algorithm}/{community_id}/topics` возвращает до пяти тем
указанного сообщества, отсортированных по убыванию агрегированного веса.
`algorithm` принимает `leiden` или `bigclam`. Вес темы
складывается из OpenAlex score,
умноженного на произведение принадлежностей авторов статьи: `1` для авторов
сообщества и `0.01` для остальных. Поэтому статьи с авторами из нескольких
сообществ дают ненулевой вклад каждому из них. При ingest-е backend запускает
эту агрегацию отдельно для Leiden и BigCLAM; результаты обоих алгоритмов
хранятся отдельно. Для SNAP BigCLAM можно настроить `BIGCLAM_BINARY`
(по умолчанию `/usr/local/bin/bigclam`), `BIGCLAM_COMMUNITIES` (по умолчанию
`10`) и `BIGCLAM_THREADS` (по умолчанию `1`). `run_id`
используется только внутри backend и не является частью HTTP API.
