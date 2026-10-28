#pragma once

#include <chrono>
#include <mutex>

namespace schools {

// Global request pacing shared by all OpenAlex workers. Each caller reserves
// the next free time slot and sleeps until it arrives. This keeps the total
// request rate under the limit even when many threads are fetching.
class RateLimiter {
 public:
  explicit RateLimiter(double requests_per_second);

  void Acquire();

 private:
  std::mutex mutex_;
  std::chrono::steady_clock::time_point next_slot_;
  std::chrono::nanoseconds interval_{};
};

}  // namespace schools
