#pragma once
#include "linein.h"
#include <coroutine>
#include <functional>
#include <assert.h>
#include <optional>

template <typename T> class CoroutineState {
  using Disposer = std::function<void()>;
  Disposer _disposer;
  std::optional<T> _return;

public:
  std::function<void(const T &)> onReturn;
  explicit CoroutineState(const Disposer &disposer) : _disposer(disposer) {}
  CoroutineState(CoroutineState const &) = delete;
  ~CoroutineState() { _disposer(); }
  const std::optional<T> &return_value() const { return _return; }
  void return_value(const T &value) {
    _return = value;
    if (onReturn) {
      onReturn(value);
    }
  }
};

template <> class CoroutineState<void> {
  using Disposer = std::function<void()>;
  Disposer _disposer;
  bool _return = false;

public:
  std::function<void()> onReturn;
  explicit CoroutineState(const Disposer &disposer) : _disposer(disposer) {}
  CoroutineState(CoroutineState const &) = delete;
  ~CoroutineState() { _disposer(); }
  bool isDone() const { return _return; }
  void setDone() { _return = true; }
};

template <typename T> struct CoroutinePromiseBase {
  using handle_t = std::coroutine_handle<T>;
  auto initial_suspend() {
    // hot-start
    return std::suspend_never{};
  }
  auto final_suspend() noexcept {
    // keep coroutine state
    // https://devblogs.microsoft.com/oldnewthing/20210331-00/?p=105028
    return std::suspend_always{};
  }
  void unhandled_exception() { std::terminate(); }

  template <typename S> struct ValueAwaiter {
    std::optional<S> payload;
    void *handle_address;
    std::function<void(ValueAwaiter *)> onSuspend;
    bool await_ready() const { return this->payload.has_value(); }
    S await_resume() { return this->payload.value(); }
    void await_suspend(handle_t t) { /* ... */
      this->handle_address = t.address();
      if (this->onSuspend) {
        this->onSuspend(this);
      }
    }
    void resume(const S &value) {
      auto handle = handle_t::from_address(this->handle_address);
      this->payload = value;
      assert(!handle.done());
      handle.resume();
    }
  };

  struct VoidAwaiter {
    bool ready = false;
    void *handle_address;
    std::function<void(VoidAwaiter *)> onSuspend;
    bool await_ready() const { return this->ready; }
    void await_resume() { ; }
    void await_suspend(handle_t t) { /* ... */
      this->handle_address = t.address();
      if (this->onSuspend) {
        this->onSuspend(this);
      }
    }
    void resume() {
      auto handle = handle_t::from_address(this->handle_address);
      assert(!handle.done());
      handle.resume();
    }
  };

  //
  template <typename S>
  ValueAwaiter<S>
  await_transform(const std::shared_ptr<CoroutineState<S>> &state) {
    ValueAwaiter<S> awaiter;
    if (auto value = state->return_value()) {
      awaiter.payload = *value;
    } else {
      awaiter.onSuspend = [state](auto a) {
        state->onReturn = [a](const S &value) { a->resume(value); };
      };
    }
    return awaiter;
  }

  VoidAwaiter
  await_transform(const std::shared_ptr<CoroutineState<void>> &state) {
    VoidAwaiter awaiter;
    if (state->isDone()) {
      awaiter.ready = true;
    } else {
      awaiter.onSuspend = [state](auto a) {
        state->onReturn = [a]() { a->resume(); };
      };
    }
    return awaiter;
  }

  //
  ValueAwaiter<std::string>
  await_transform(const std::shared_ptr<LineInput> &input) {
    ValueAwaiter<std::string> awaiter;

    awaiter.onSuspend = [input](auto a) {
      input->onInput = [a](const std::string &p) { a->resume(p); };
    };

    return awaiter;
  }
};

template <typename T>
struct CoroutinePromise : CoroutinePromiseBase<CoroutinePromise<T>> {
  using handle_t = std::coroutine_handle<CoroutinePromise>;
  std::shared_ptr<CoroutineState<T>> _state;
  std::shared_ptr<CoroutineState<T>> get_return_object() {
    _state = std::make_shared<CoroutineState<T>>(
        [p = handle_t::from_promise(*this).address()]() {
          handle_t::from_address(p).destroy();
        });
    return _state;
  }
  void return_value(const T &value) { _state->return_value(value); }
};

template <> struct CoroutinePromise<void> : CoroutinePromiseBase<void> {
  using handle_t = std::coroutine_handle<CoroutinePromise>;
  std::shared_ptr<CoroutineState<void>> _state;
  std::shared_ptr<CoroutineState<void>> get_return_object() {
    _state = std::make_shared<CoroutineState<void>>(
        [p = handle_t::from_promise(*this).address()]() {
          handle_t::from_address(p).destroy();
        });
    return _state;
  }
  void return_void() { _state->setDone(); }
};

template <typename T, typename... ArgTypes>
struct std::coroutine_traits<std::shared_ptr<CoroutineState<T>>, ArgTypes...> {
  using promise_type = CoroutinePromise<T>;
};

struct FuncContext {
  std::shared_ptr<struct TabBuffer> curretTab;
};

using Func = std::function<std::shared_ptr<CoroutineState<void>>(
    const FuncContext &ctx)>;
