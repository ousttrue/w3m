#include <gtest/gtest.h>
#include <optional>
#include <functional>
#include <coroutine>

// https://html.spec.whatwg.org/multipage/parsing.html

enum HtmlTokenTypes {
  Character,
};

enum HtmlParserStateType {};

struct HtmlToken {
  HtmlTokenTypes type;
  std::string_view view;
};

using HtmlParserResult =
    std::tuple<std::optional<HtmlParserStateType>, std::optional<HtmlToken>>;

using HtmlParseStateFunc = std::function<HtmlParserResult(char8_t)>;

struct generator {
  struct promise_type;
  using handle = std::coroutine_handle<promise_type>;

private:
  generator(handle h) : coro(h) {}
  handle coro;

public:
  struct promise_type {
    HtmlToken current_value;
    static auto get_return_object_on_allocation_failure() {
      return generator{nullptr};
    }
    auto get_return_object() { return generator{handle::from_promise(*this)}; }
    auto initial_suspend() { return std::suspend_always{}; }
    auto final_suspend() noexcept { return std::suspend_always{}; }
    void unhandled_exception() { std::terminate(); }
    void return_void() {}
    auto yield_value(const HtmlToken &value) {
      current_value = value;
      return std::suspend_always{};
    }
  };
  bool move_next() { return coro ? (coro.resume(), !coro.done()) : false; }
  HtmlToken current_value() { return coro.promise().current_value; }
  generator(generator const &) = delete;
  generator(generator &&rhs) : coro(rhs.coro) { rhs.coro = nullptr; }
  ~generator() {
    if (coro)
      coro.destroy();
  }
};

struct DataState {
  HtmlParserResult operator()(char8_t c) { return {}; }
};

class HtmlTokenizer {
  std::string_view _v;
  std::string_view::iterator _current;
  std::string_view::iterator _start;
  HtmlParseStateFunc _state;

public:
  HtmlTokenizer(std::string_view v)
      : _v(v), _current(_v.begin()), _start(_current) {
    this->_state = DataState{};
  }

  generator next() {
    while (_current != _v.end()) {
      auto [next_state, value] = _state(*(_current++));
      if (next_state) {
        // _state = next_state;
      }
      if (value) {
        co_yield *value;
      }
    }
  }
};

TEST(HtmlTest, tokenizer) {
  HtmlTokenizer t(" ");
  auto g = t.next();
  g.move_next();
  auto v = g.current_value().view;
  EXPECT_EQ(" ", v);
}
