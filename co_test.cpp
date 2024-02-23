#include <iostream>
#include <uv.h>
#include <coroutine>
#include <chrono>
#include <optional>
#include <functional>
#include <assert.h>

template <typename T> struct CoObj {
  struct promise_type_base {
    uv_async_t *async = nullptr;
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

      std::cout << "onFinal:" << (onFinal ? "has" : "empty") << std::endl;
      if (this->onFinal) {
        this->onFinal();
      }

      return std::suspend_always{};
    }
    void unhandled_exception() { std::terminate(); }

    template <class Rep, class Period>
    auto await_transform(std::chrono::duration<Rep, Period> d) {
      using awaiter_t = CoObj::ValueAwaiter<int>;
      awaiter_t awaiter;
      awaiter.onSuspend = [d](awaiter_t *self) {
        std::cout << "await_suspend" << std::endl;
        auto timer = new uv_timer_t;
        timer->data = self;
        uv_timer_init(uv_default_loop(), timer);
        uv_timer_start(
            timer,
            [](uv_timer_t *t) {
              //
              std::cout << "timer: end" << std::endl;

              auto a = (awaiter_t *)t->data;

              // auto a = (uv_async_t *)handle->data;
              // uv_async_send(a);
              a->resume(5);
            },
            d.count(), 0);
      };
      return awaiter;
    }

    template <typename S> auto await_transform(CoObj<S> nested) {
      using awaiter_t = CoObj::ValueAwaiter<S>;
      awaiter_t awaiter;
      awaiter.onSuspend = [p = nested.handle.address()](awaiter_t *self) {
        std::cout << "nest suspend" << std::endl;
        CoObj::handle_type::from_address(p).promise().onFinal = [p, self]() {
          // std::cout << "onFinal: resume" << std::endl;
          auto value = CoObj<S>::handle_type::from_address(p).promise().value;
          // resume outer awaiter use nested value
          self->resume(*value);
        };
      };
      return awaiter;
    }

    auto await_transform(CoObj<void> nested) {
      return CoObj::VoidAwaiter{[p = nested.handle.address()](auto outer) {
        std::cout << "nest suspend" << std::endl;
        CoObj::handle_type::from_address(p).promise().onFinal = [](auto self) {
          // std::cout << "onFinal: resume" << std::endl;
          // resume outer awaiter
          self->resume();
        };
      }};
    }
  };

  template <typename S> struct promise_type_t : public promise_type_base {
    using handle_t = std::coroutine_handle<promise_type_t>;
    auto get_return_object() { return CoObj{handle_t::from_promise(*this)}; }
    std::optional<T> value;
    void return_value(const T &value) { this->value = value; }
  };

  using promise_type = promise_type_t<T>;

  std::coroutine_handle<promise_type> handle; // 👈重要
  using handle_type = std::coroutine_handle<CoObj<T>::promise_type>;

  template <typename S> struct ValueAwaiter {
    std::optional<S> payload;
    void *handle_address;
    std::function<void(ValueAwaiter *)> onSuspend;
    bool await_ready() const { return this->payload.has_value(); }
    S await_resume() { return this->payload.value(); }
    void await_suspend(CoObj::handle_type h) { /* ... */
      this->handle_address = h.address();
      if (this->onSuspend) {
        this->onSuspend(this);
      }
    }
    CoObj::handle_type handle() const {
      return CoObj::handle_type::from_address(this->handle_address);
    }
    void resume(const S &value) {
      this->payload = value;
      assert(!handle().done());
      handle().resume();
    }
  };

  struct VoidAwaiter {
    std::function<void(CoObj::handle_type)> onSuspend;
    bool await_ready() const { return false; }
    void await_resume() {}
    void await_suspend(CoObj::handle_type h) { /* ... */
      this->handle_address = h.address();
      if (this->onSuspend) {
        this->onSuspend(this);
      }
    }
    CoObj::handle_type handle() const {
      return CoObj::handle_type::from_address(this->handle_address);
    }
    void resume() {
      assert(!handle().done());
      handle().resume();
    }
  };
};

template <>
template <>
struct CoObj<void>::promise_type_t<void> : public promise_type_base {
  using handle_t = std::coroutine_handle<promise_type_t>;
  auto get_return_object() { return CoObj{handle_t::from_promise(*this)}; }
  void return_void() {}
};

int main(int argc, char **argv) {

  std::cout << "start" << std::endl;

  using namespace std::literals::chrono_literals;
  auto task = [d = 500ms]() -> CoObj<int> {
    std::cout << "[task] wait 500ms" << std::endl;
    co_await d;
    std::cout << "[task] timer done" << std::endl;
    co_return 4;
  };

  auto outer = [task]() -> CoObj<int> { co_return co_await task(); };

  auto ret = outer();

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  std::cout << "handle.done: " << *ret.handle.promise().value << std::endl;

  return 0;
}
