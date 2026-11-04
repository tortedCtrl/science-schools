#include "common/rate_limiter.hpp"

#include <algorithm>
#include <cstdint>
#include <thread>

namespace schools {

RateLimiter::RateLimiter(double requests_per_second) {
  if (requests_per_second > 0.0) {
    interval_ = std::chrono::nanoseconds(
        static_cast<std::int64_t>(1'000'000'000.0 / requests_per_second));
  }
}

void RateLimiter::Acquire() {
  if (interval_.count() <= 0) {
    return;
  }

  const auto now = std::chrono::steady_clock::now();
  std::chrono::steady_clock::time_point slot;

  {
    const std::lock_guard<std::mutex> lock(mutex_);
    slot = std::max(next_slot_, now);
    next_slot_ = slot + interval_;
  }

  if (slot > now) {
    std::this_thread::sleep_until(slot);
  }
}

}  // namespace schools
