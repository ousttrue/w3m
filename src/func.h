#pragma once
#include <uv.h>
#include <coroutine>
#include <chrono>
#include <functional>

template <typename T> struct FuncCoroutine {
  struct promise_type_base {
    uv_async_t *async = nullptr;
    auto initial_suspend() {
      async = new uv_async_t;
      uv_async_init(uv_default_loop(), async, [](auto a) {});
      return std::suspend_never{};
    }

    // for resume outer coroutine when final
    std::function<void()> onFinal;
    auto final_suspend() noexcept {
      uv_close((uv_handle_t *)async, [](auto a) { delete a; });

      if (this->onFinal) {
        this->onFinal();
      }

      return std::suspend_always{};
    }
    void unhandled_exception() { std::terminate(); }

    template <class Rep, class Period>
    auto await_transform(std::chrono::duration<Rep, Period> d) {
      using awaiter_t = FuncCoroutine::ValueAwaiter<int>;
      awaiter_t awaiter;
      awaiter.onSuspend = [d](awaiter_t *self) {
        auto timer = new uv_timer_t;
        timer->data = self;
        uv_timer_init(uv_default_loop(), timer);
        uv_timer_start(
            timer,
            [](uv_timer_t *t) {
              auto a = (awaiter_t *)t->data;

              // auto a = (uv_async_t *)handle->data;
              // uv_async_send(a);
              a->resume(5);
            },
            d.count(), 0);
      };
      return awaiter;
    }

    template <typename S> auto await_transform(FuncCoroutine<S> nested) {
      using awaiter_t = FuncCoroutine::ValueAwaiter<S>;
      awaiter_t awaiter;
      awaiter.onSuspend = [p = nested.handle.address()](awaiter_t *self) {
        FuncCoroutine::handle_type::from_address(p)
            .promise()
            .onFinal = [p, self]() {
          // std::cout << "onFinal: resume" << std::endl;
          auto value =
              FuncCoroutine<S>::handle_type::from_address(p).promise().value;
          // resume outer awaiter use nested value
          self->resume(*value);
        };
      };
      return awaiter;
    }

    auto await_transform(FuncCoroutine<void> nested) {
      return FuncCoroutine::VoidAwaiter{
          [p = nested.handle.address()](auto outer) {
            FuncCoroutine::handle_type::from_address(p).promise().onFinal =
                [](auto self) {
                  // std::cout << "onFinal: resume" << std::endl;
                  // resume outer awaiter
                  self->resume();
                };
          }};
    }
  };

  template <typename S> struct promise_type_t : public promise_type_base {
    using handle_t = std::coroutine_handle<promise_type_t>;
    auto get_return_object() {
      return FuncCoroutine{handle_t::from_promise(*this)};
    }
    std::optional<T> value;
    void return_value(const T &value) { this->value = value; }
  };

  using promise_type = promise_type_t<T>;

  std::coroutine_handle<promise_type> handle; // 👈重要
  using handle_type = std::coroutine_handle<FuncCoroutine<T>::promise_type>;

  template <typename S> struct ValueAwaiter {
    std::optional<S> payload;
    void *handle_address;
    std::function<void(ValueAwaiter *)> onSuspend;
    bool await_ready() const { return this->payload.has_value(); }
    S await_resume() { return this->payload.value(); }
    void await_suspend(FuncCoroutine::handle_type h) { /* ... */
      this->handle_address = h.address();
      if (this->onSuspend) {
        this->onSuspend(this);
      }
    }
    FuncCoroutine::handle_type handle() const {
      return FuncCoroutine::handle_type::from_address(this->handle_address);
    }
    void resume(const S &value) {
      this->payload = value;
      assert(!handle().done());
      handle().resume();
    }
  };

  struct VoidAwaiter {
    std::function<void(FuncCoroutine::handle_type)> onSuspend;
    bool await_ready() const { return false; }
    void await_resume() {}
    void await_suspend(FuncCoroutine::handle_type h) { /* ... */
      this->handle_address = h.address();
      if (this->onSuspend) {
        this->onSuspend(this);
      }
    }
    FuncCoroutine::handle_type handle() const {
      return FuncCoroutine::handle_type::from_address(this->handle_address);
    }
    void resume() {
      assert(!handle().done());
      handle().resume();
    }
  };
};

template <>
template <>
struct FuncCoroutine<void>::promise_type_t<void> : public promise_type_base {
  using handle_t = std::coroutine_handle<promise_type_t>;
  auto get_return_object() {
    return FuncCoroutine{handle_t::from_promise(*this)};
  }
  void return_void() {}
};

using Func = std::function<FuncCoroutine<void>()>;
