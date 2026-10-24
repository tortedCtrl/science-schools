#pragma once

#include <boost/asio.hpp>
#include <type_traits>
#include <utility>

namespace schools {

// Completion token that delivers (error_code, results...) as a tuple instead
// of throwing: `auto [ec, value] = co_await op(..., kAsTuple);`
inline const auto kAsTuple{boost::asio::as_tuple(boost::asio::use_awaitable)};

namespace detail {

// Takes the callable by value so it lives in the coroutine frame.
template <typename F>
boost::asio::awaitable<std::invoke_result_t<F&>> InvokeCallable(F func) {
  co_return func();
}

}  // namespace detail

// Runs a CPU-bound callable (e.g. community detection) on `executor` and
// resumes the calling coroutine on its own executor with the result.
template <typename F>
boost::asio::awaitable<std::invoke_result_t<F&>> RunBlocking(
    boost::asio::any_io_executor executor, F func) {
  co_return co_await boost::asio::co_spawn(
      executor, detail::InvokeCallable(std::move(func)),
      boost::asio::use_awaitable);
}

}  // namespace schools
