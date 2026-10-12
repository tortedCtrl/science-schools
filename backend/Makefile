ifneq ($(wildcard ../.env),)
    include ../.env
    export $(shell sed 's/=.*//' ../.env)
endif

.PHONY: init build run clean

init:
	cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake

build:
	cmake --build build

run: build
	set -a; . ../.env; set +a; BACKEND_IN_DOCKER=false BACKEND_HOST=127.0.0.1 ./build/apps/server/backend_server

clean:
	rm -rf build/
