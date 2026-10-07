.PHONY: all build clean run

BUILD_DIR = build

all: build

build:
	cmake -B $(BUILD_DIR) -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR)

run: build
	./$(BUILD_DIR)/apps/server/backend_server

clean:
	rm -rf $(BUILD_DIR)
