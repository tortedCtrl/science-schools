ifneq ($(wildcard .env),)
    include .env
    export $(shell sed 's/=.*//' .env)
endif

.PHONY: init build run clean

init:
	cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake

build:
	cmake --build build

run: build
	./build/apps/server/backend_server

clean:
	rm -rf build/
