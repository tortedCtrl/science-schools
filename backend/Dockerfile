# syntax=docker/dockerfile:1

# ---------------------------------------------------------------------------
# Build stage: gcc 15 + CMake + vcpkg
# ---------------------------------------------------------------------------
FROM gcc:15 AS build

ARG CMAKE_VERSION=3.31.6

RUN apt-get update && apt-get install -y --no-install-recommends \
      ninja-build git curl zip unzip tar pkg-config ca-certificates \
      make perl bison flex \
    && rm -rf /var/lib/apt/lists/*

# Distro CMake may be older than the 3.28 required by the project.
RUN ARCH="$(uname -m)" \
    && curl -fsSL "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-${ARCH}.tar.gz" \
       | tar -xz -C /opt \
    && ln -s /opt/cmake-${CMAKE_VERSION}-linux-${ARCH}/bin/* /usr/local/bin/

ENV VCPKG_ROOT=/opt/vcpkg \
    VCPKG_FORCE_SYSTEM_BINARIES=1 \
    VCPKG_DISABLE_METRICS=1
RUN git clone --depth 1 https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh -disableMetrics

WORKDIR /src

# Dependencies first: this layer is cached until vcpkg.json changes.
COPY vcpkg.json ./
RUN ${VCPKG_ROOT}/vcpkg install \
      --x-manifest-root=/src --x-install-root=/opt/vcpkg_installed

COPY CMakeLists.txt ./
COPY src ./src
RUN cmake -S . -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
      -DVCPKG_INSTALLED_DIR=/opt/vcpkg_installed \
      -DVCPKG_MANIFEST_INSTALL=OFF \
    && cmake --build build --target scientific_schools

# ---------------------------------------------------------------------------
# Runtime stage
# ---------------------------------------------------------------------------
FROM debian:trixie-slim

RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --uid 10001 --no-create-home app

# The binary needs the libstdc++ / libgomp from gcc 15, which are newer than
# the ones shipped by Debian.
COPY --from=build /usr/local/lib64/libstdc++.so.6* /usr/local/lib64/
COPY --from=build /usr/local/lib64/libgomp.so.1* /usr/local/lib64/
COPY --from=build /usr/local/lib64/libgcc_s.so.1 /usr/local/lib64/
ENV LD_LIBRARY_PATH=/usr/local/lib64

COPY --from=build /src/build/scientific_schools /usr/local/bin/scientific_schools

USER app
EXPOSE 8080
ENTRYPOINT ["scientific_schools"]
CMD ["serve"]
