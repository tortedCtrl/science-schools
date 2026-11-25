#pragma once

#include <chrono>
#include <string>
#include <utility>
#include <spdlog/spdlog.h>

namespace schools {

class ScopedTimer {
public:
    explicit ScopedTimer(std::string name, bool enabled)
        : name_{std::move(name)},
          start_{std::chrono::high_resolution_clock::now()},
          enabled_{enabled} {}

    ~ScopedTimer() {
        if (!enabled_) {
            return;
        }
        auto end = std::chrono::high_resolution_clock::now();
        double seconds = std::chrono::duration<double>(end - start_).count();
        spdlog::info("[TIMER] {}: {:.3f} s", name_, seconds);
    }

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
    ScopedTimer(ScopedTimer&&) = delete;
    ScopedTimer& operator=(ScopedTimer&&) = delete;

private:
    std::string name_;
    std::chrono::high_resolution_clock::time_point start_;
    bool enabled_;
};

}  // namespace schools