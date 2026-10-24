#pragma once

#include <atomic>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdint>

namespace schools {

// Global request pacing shared by all ingestion coroutines. Lock-free: each
// Acquire() atomically reserves the next time slot and sleeps until it comes.
class RateLimiter {
 public:
  explicit RateLimiter(double requests_per_second);

  boost::asio::awaitable<void> Acquire();

 private:
  using Clock = std::chrono::steady_clock;

  std::int64_t interval_ns_;
  std::atomic<std::int64_t> next_slot_ns_{0};
};

}  // namespace schools
