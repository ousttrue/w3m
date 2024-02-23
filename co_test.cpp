#include <iostream>
#include <uv.h>
#include <coroutine>
#include <chrono>
#include <optional>
#include <functional>

struct CoObj {
  struct promise_type // 👈重要
  {
    uv_async_t *async = nullptr;
    using handle_t = std::coroutine_handle<promise_type>;

    auto get_return_object() {
      return CoObj{handle_t::from_promise(*this)}; // 👈重要
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

struct VoidAwaiter {
  std::function<void(std::coroutine_handle<>)> onSuspend;
  bool await_ready() const { return false; }
  void await_resume() {}
  void await_suspend(std::coroutine_handle<> h) { /* ... */
    if (this->onSuspend) {
      this->onSuspend(h);
    }
  }
};

template <typename T> struct ValueAwaiter {
  std::optional<T> payload;
  void *handle_address;
  std::function<void(ValueAwaiter *, std::coroutine_handle<>)> onSuspend;
  bool await_ready() const { return this->payload.has_value(); }
  T await_resume() { return this->payload.value(); }
  void await_suspend(std::coroutine_handle<> h) { /* ... */
    if (this->onSuspend) {
      this->onSuspend(this, h);
    }
  }

  std::coroutine_handle<> handle() const {
    return std::coroutine_handle<>::from_address(this->handle_address);
  }
  void resume(const T &value) {
    this->payload = value;
    handle().resume();
  }
};

template <class Rep, class Period>
auto operator co_await(std::chrono::duration<Rep, Period> d) {
  using awaiter_t = ValueAwaiter<int>;
  awaiter_t awaiter;
  awaiter.onSuspend = [d](awaiter_t *self, auto h) {
    std::cout << "await_suspend" << std::endl;
    self->handle_address = h.address();
    auto timer = new uv_timer_t;
    timer->data = self;
    uv_timer_init(uv_default_loop(), timer);
    uv_timer_start(
        timer,
        [](uv_timer_t *t) {
          //
          std::cout << "timer: " << std::endl;

          auto a = (awaiter_t *)t->data;

          // auto a = (uv_async_t *)handle->data;
          // uv_async_send(a);
          a->resume(5);
        },
        d.count(), 0);
  };
  return awaiter;
}

int main(int argc, char **argv) {

  std::cout << "start" << std::endl;

  using namespace std::literals::chrono_literals;
  auto task = [d = 500ms]() -> CoObj {
    std::cout << "[task] wait 500ms" << std::endl;
    auto value = co_await d;
    std::cout << "[task] done: " << value << std::endl;
  };

  auto ret = task();

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  std::cout << "end" << std::endl;

  std::cout << ret.handle.done() << std::endl;

  return 0;
}
