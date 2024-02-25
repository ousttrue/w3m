#include <iostream>
// #include "src/func.h"
#include <coroutine>
#include <functional>
#include <memory>
#include <utility>
#include <optional>
// #include <memory>

template <typename T> struct CoroutineState {
  using Disposer = std::function<void()>;
  std::optional<T> return_value;

private:
  Disposer _disposer;

public:
  explicit CoroutineState(const Disposer &disposer) : _disposer(disposer) {}
  CoroutineState(CoroutineState const &) = delete;
  ~CoroutineState() { _disposer(); }
};

template <> struct CoroutineState<void> {
  using Disposer = std::function<void()>;
  bool return_void = false;

private:
  Disposer _disposer;

public:
  explicit CoroutineState(const Disposer &disposer) : _disposer(disposer) {}
  CoroutineState(CoroutineState const &) = delete;
  ~CoroutineState() { _disposer(); }
};

template <typename T> struct CoroutinePromiseBase {
  using handle_t = std::coroutine_handle<CoroutineState<T>>;
  auto initial_suspend() { return std::suspend_never{}; }
  auto final_suspend() noexcept { return std::suspend_always{}; }
  void unhandled_exception() { std::terminate(); }
};

template <typename T> struct CoroutinePromise : CoroutinePromiseBase<T> {
  using handle_t = std::coroutine_handle<CoroutinePromise>;
  std::shared_ptr<CoroutineState<T>> _state;
  std::shared_ptr<CoroutineState<T>> get_return_object() {
    _state = std::make_shared<CoroutineState<T>>(
        [p = handle_t::from_promise(*this).address()]() {
          handle_t::from_address(p).destroy();
        });
    return _state;
  }
  void return_value(const T &value) { _state->return_value = value; }
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
  void return_void() { _state->return_void = true; }
};

template <typename T, typename... ArgTypes>
struct std::coroutine_traits<std::shared_ptr<CoroutineState<T>>, ArgTypes...> {
  using promise_type = CoroutinePromise<T>;
};

std::shared_ptr<CoroutineState<int>> coro() {
  std::cout << "coro#1" << std::endl;
  // co_yield {};
  std::cout << "coro#2" << std::endl;
  co_return 4;
}

int main() {
  auto t = coro();
  std::cout << t->return_value.value() << std::endl;
}
