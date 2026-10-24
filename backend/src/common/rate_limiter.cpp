#include "common/rate_limiter.hpp"

#include <algorithm>

#include "common/async.hpp"

namespace schools {

RateLimiter::RateLimiter(double requests_per_second)
    : interval_ns_{static_cast<std::int64_t>(
          1e9 / std::max(requests_per_second, 0.001))} {}

boost::asio::awaitable<void> RateLimiter::Acquire() {
  const std::int64_t now{std::chrono::duration_cast<std::chrono::nanoseconds>(
                             Clock::now().time_since_epoch())
                             .count()};

  std::int64_t current{next_slot_ns_.load()};
  std::int64_t slot{0};
  do {
    slot = std::max(current, now);
  } while (!next_slot_ns_.compare_exchange_weak(current, slot + interval_ns_));

  if (slot > now) {
    boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);
    timer.expires_after(std::chrono::nanoseconds(slot - now));
    // Cancellation is the only possible error and means we are shutting down.
    const auto [ec] = co_await timer.async_wait(kAsTuple);
    (void)ec;
  }
}

}  // namespace schools
