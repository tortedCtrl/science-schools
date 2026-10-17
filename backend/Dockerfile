FROM debian:bookworm-slim AS builder

ENV DEBIAN_FRONTEND=noninteractive VCPKG_ROOT=/opt/vcpkg VCPKG_FORCE_SYSTEM_BINARIES=1

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        libgomp1 \
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
    && git clone --depth 1 https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT" \
    && "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Кэширование библиотек
COPY vcpkg.json .
RUN --mount=type=cache,target=/root/.cache/vcpkg \
    echo "cmake_minimum_required(VERSION 3.15)\nproject(dummy LANGUAGES C CXX)" > CMakeLists.txt \
    && cmake -G Ninja -B build -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"


# Сборка кода
COPY . .

RUN --mount=type=cache,target=/root/.ccache \
    cmake -G Ninja -B build -S . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    && cmake --build build

# Образ для исполнения    
FROM gcr.io/distroless/cc-debian12 AS runtime

COPY --from=builder /usr/lib/x86_64-linux-gnu/libgomp.so.1 /usr/lib/x86_64-linux-gnu/

USER nonroot

COPY --from=builder /src/build/apps/server/backend_server /usr/local/bin/backend_server

ENTRYPOINT ["/usr/local/bin/backend_server"]
