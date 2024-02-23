#include <iostream>
#include <uv.h>
#include <coroutine>
#include <chrono>

struct RetObj {
  struct promise_type // 👈重要
  {
    uv_async_t *async = nullptr;
    using handle_t = std::coroutine_handle<promise_type>;

    auto get_return_object() {
      return RetObj{handle_t::from_promise(*this)}; // 👈重要
    }
    auto initial_suspend() {
      std::cout << "[promise_type] initial_suspend" << std::endl;

      std::cout << "create uv_async_t" << std::endl;
      async = new uv_async_t;
      // async->data = handle_t::from_promise(*this).address();
      uv_async_init(uv_default_loop(), async, [](auto a) {
        //
        // std::cout << "awake uv_async_t" << std::endl;
        // handle_t::from_address(a->data).resume();
      });

      return std::suspend_never{};
    }
    auto final_suspend() noexcept {
      std::cout << "[promise_type] final_suspend" << std::endl;

      uv_close((uv_handle_t *)async, [](auto a) {
        std::cout << "closed uv_async_t. delete uv_async_t" << std::endl;
        ;
        delete a;
      });

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
    using handle_t = std::coroutine_handle<RetObj::promise_type>;
    uv_timer_t *timer = nullptr;
    std::chrono::milliseconds duration;
    /* ... */
    awaiter(std::chrono::milliseconds d) : duration(d) {
      std::cout << "duration: " << duration.count() << std::endl;
    }
    bool await_ready() const { return duration.count() <= 0; }
    void await_resume() { std::cout << "await_resume" << std::endl; }
    void await_suspend(handle_t h) { /* ... */
      std::cout << "await_suspend" << std::endl;
      timer = new uv_timer_t;
      timer->data = h.address();
      uv_timer_init(uv_default_loop(), timer);
      uv_timer_start(
          timer,
          [](uv_timer_t *t) {
            //
            std::cout << "timer: " << std::endl;

            // auto a = (uv_async_t *)handle->data;
            // uv_async_send(a);
            handle_t::from_address(t->data).resume();
          },
          duration.count(), 0);
    }
  };
  return awaiter{d};
}

int main(int argc, char **argv) {

  std::cout << "start" << std::endl;

  using namespace std::literals::chrono_literals;
  auto task = [d = 500ms]() -> RetObj {
    std::cout << "[task] wait 500ms" << std::endl;
    co_await d;
    std::cout << "[task] done" << std::endl;
  };

  auto ret = task();

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  std::cout << "end" << std::endl;

  std::cout << ret.handle.done() << std::endl;

  return 0;
}
