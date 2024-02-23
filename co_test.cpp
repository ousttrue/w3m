#include <iostream>
#include <uv.h>
#include <coroutine>
#include <chrono>
#include <optional>
#include <functional>

struct CoObj {
  struct promise_type {
    uv_async_t *async = nullptr;
    using handle_t = std::coroutine_handle<promise_type>;
    auto get_return_object() { return CoObj{handle_t::from_promise(*this)}; }
    auto initial_suspend() {
      std::cout << "[promise_type] initial_suspend" << std::endl;
      std::cout << "create uv_async_t" << std::endl;
      async = new uv_async_t;
      uv_async_init(uv_default_loop(), async, [](auto a) {});
      return std::suspend_never{};
    }

    // for resume outer coroutine when final
    std::function<void()> onFinal;
    auto final_suspend() noexcept {
      std::cout << "[promise_type] final_suspend" << std::endl;

      uv_close((uv_handle_t *)async, [](auto a) {
        std::cout << "closed uv_async_t. delete uv_async_t" << std::endl;
        ;
        delete a;
      });

      if (this->onFinal) {
        std::cout << "onFinal" << std::endl;
        this->onFinal();
      }

      return std::suspend_always{};
    }
    void unhandled_exception() { std::terminate(); }
    void return_void() {}
  };
  std::coroutine_handle<promise_type> handle; // 👈重要
};
using promise_handle_t = std::coroutine_handle<CoObj::promise_type>;

struct VoidAwaiter {
  std::function<void(promise_handle_t)> onSuspend;
  bool await_ready() const { return false; }
  void await_resume() {}
  void await_suspend(promise_handle_t h) { /* ... */
    if (this->onSuspend) {
      this->onSuspend(h);
    }
  }
};

template <typename T> struct ValueAwaiter {
  std::optional<T> payload;
  void *handle_address;
  std::function<void(ValueAwaiter *, promise_handle_t)> onSuspend;
  bool await_ready() const { return this->payload.has_value(); }
  T await_resume() { return this->payload.value(); }
  void await_suspend(promise_handle_t h) { /* ... */
    if (this->onSuspend) {
      this->onSuspend(this, h);
    }
  }

  promise_handle_t handle() const {
    return promise_handle_t::from_address(this->handle_address);
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

auto operator co_await(CoObj inner) {
  return VoidAwaiter{[p = inner.handle.address()](promise_handle_t outer_h) {
    std::cout << "nest suspend" << std::endl;
    promise_handle_t::from_address(p).promise().onFinal = [p]() {
      // resume inner
      std::cout << "onFinal: resume" << std::endl;
      promise_handle_t::from_address(p).resume();
    };
  }};
}

int main(int argc, char **argv) {

  std::cout << "start" << std::endl;

  using namespace std::literals::chrono_literals;
  auto task = [d = 500ms]() -> CoObj {
    std::cout << "[task] wait 500ms" << std::endl;
    auto value = co_await d;
    std::cout << "[task] done: " << value << std::endl;
  };

  auto nest = [task]() -> CoObj { co_await task(); };

  auto ret = nest();

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  std::cout << "end" << std::endl;

  std::cout << ret.handle.done() << std::endl;

  return 0;
}
