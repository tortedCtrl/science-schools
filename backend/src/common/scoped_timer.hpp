#pragma once

#include <chrono>
#include <string>
#include <spdlog/spdlog.h>

namespace schools {

class ScopedTimer {
public:
    explicit ScopedTimer(std::string name)
        : name_{std::move(name)}, start_{std::chrono::high_resolution_clock::now()} {}

    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        double seconds = std::chrono::duration<double>(end - start_).count();
        spdlog::info("[TIMER] {}: {:.3f} s", name_, seconds);
    }

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
};

}  // namespace schools