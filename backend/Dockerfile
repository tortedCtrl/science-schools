FROM debian:bookworm AS builder

ENV DEBIAN_FRONTEND=noninteractive VCPKG_ROOT=/opt/vcpkg VCPKG_FORCE_SYSTEM_BINARIES=1

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        git \
        ninja-build \
        pkg-config \
        tar \
        unzip \
        zip \
        python3 \
        bison \
        flex \
        ccache \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT" \
    && "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics

WORKDIR /src

# Кэширование библиотек
COPY vcpkg.json .

RUN echo "cmake_minimum_required(VERSION 3.15)" > ./CMakeLists.txt && \
    echo "project(dummy LANGUAGES C CXX)" >> ./CMakeLists.txt

RUN --mount=type=cache,target=/root/.cache/vcpkg \
    cmake -G Ninja -B build -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
        -DVCPKG_MANIFEST_EARLY_ERRORS=OFF

# Сборка кода
COPY . .

RUN --mount=type=cache,target=/root/.ccache cmake -G Ninja -S . -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    && cmake --build build --parallel 4

# Образ для исполнения    
FROM debian:bookworm-slim AS runtime

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        libgcc-s1 \
        libgomp1 \
        libstdc++6 \
        libssl3 \
        curl \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m -u 10001 appuser

USER appuser

COPY --from=builder /src/build/apps/server/backend_server /usr/local/bin/backend_server

ENTRYPOINT ["/usr/local/bin/backend_server"]
