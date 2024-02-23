#pragma once
#include <iostream>
#include <uv.h>
#include <coroutine>
#include <chrono>
#include <functional>

struct FuncCoroutine {
  struct promise_type // 👈重要
  {
    using handle_t = std::coroutine_handle<promise_type>;

    auto get_return_object() {
      return FuncCoroutine{handle_t::from_promise(*this)}; // 👈重要
    }
    auto initial_suspend() {
      // std::cout << "[promise_type] initial_suspend" << std::endl;
      return std::suspend_never{};
    }
    auto final_suspend() noexcept {
      // std::cout << "[promise_type] final_suspend" << std::endl;
      return std::suspend_always{};
    }
    void unhandled_exception() { std::terminate(); }
    void return_void() {}
  };
  std::coroutine_handle<promise_type> handle; // 👈重要
};

template <class Rep, class Period>
auto operator co_await(std::chrono::duration<Rep, Period> d) {
  struct awaiter {
    using handle_t = std::coroutine_handle<>;
    uv_timer_t *timer = nullptr;
    std::chrono::milliseconds duration;
    /* ... */
    awaiter(std::chrono::milliseconds d) : duration(d) {
      // std::cout << "duration: " << duration.count() << std::endl;
    }
    bool await_ready() const { return duration.count() <= 0; }
    void await_resume() {
      // std::cout << "await_resume" << std::endl;
    }
    void await_suspend(handle_t h) { /* ... */
      // std::cout << "await_suspend" << std::endl;
      timer = new uv_timer_t;
      timer->data = h.address();
      uv_timer_init(uv_default_loop(), timer);
      uv_timer_start(
          timer,
          [](uv_timer_t *t) {
            //
            // std::cout << "timer: " << std::endl;
            // auto a = (uv_async_t *)handle->data;
            // uv_async_send(a);
            handle_t::from_address(t->data).resume();
          },
          duration.count(), 0);
    }
  };
  return awaiter{d};
}

// int main(int argc, char **argv) {
//
//   std::cout << "start" << std::endl;
//
//   auto ret = task();
//
//   uv_run(uv_default_loop(), UV_RUN_DEFAULT);
//
//   std::cout << "end" << std::endl;
//
//   std::cout << ret.handle.done() << std::endl;
//
//   return 0;
// }

using Func = std::function<FuncCoroutine()>;
