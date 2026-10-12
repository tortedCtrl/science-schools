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
	set -a; . ../.env; set +a; \
	BACKEND_HOST=127.0.0.1 \
	ConnectionStrings__DefaultConnection="host=localhost port=$$POSTGRES_PUBLISHED_PORT dbname=$$POSTGRES_DB user=$$POSTGRES_USER password=$$POSTGRES_PASSWORD" \
	./build/apps/server/backend_server

clean:
	rm -rf build/
