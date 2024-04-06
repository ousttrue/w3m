#include <gtest/gtest.h>
#include <optional>
#include <functional>
#include <coroutine>

// https://html.spec.whatwg.org/multipage/parsing.html
// https://cpprefjp.github.io/lang/cpp20/coroutines.html

enum HtmlTokenTypes {
  Unknown,
  Character,
  Reference,
};

struct HtmlToken {
  HtmlTokenTypes type;
  std::string_view view;

  bool operator==(const HtmlToken &rhs) const {
    return type == rhs.type && view.size() == rhs.view.size() &&
           view.begin() == rhs.view.begin();
  }
};

inline std::ostream &operator<<(std::ostream &os, const HtmlToken &token) {
  os << "[";
  switch (token.type) {
  case Unknown:
    os << "Unknown";
    break;
  case Character:
    os << "Character";
    break;
  case Reference:
    os << "Reference";
    break;
  }
  os << "] '" << token.view << "'";
  return os;
}

enum HtmlParserStateType {
  Data,
};

using HtmlParserResult =
    std::tuple<std::optional<HtmlParserStateType>, std::optional<HtmlToken>>;

using HtmlParseStateFunc =
    std::function<std::tuple<std::string_view::iterator, HtmlParserResult>(
        std::string_view::iterator)>;

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
  bool move_next() {
    if (!coro) {
      return false;
    }
    if (coro.done()) {
      return false;
    }
    coro.resume();
    if (coro.done()) {
      return false;
    }
    return true;
  }
  HtmlToken current_value() { return coro.promise().current_value; }
  generator(generator const &) = delete;
  generator(generator &&rhs) : coro(rhs.coro) { rhs.coro = nullptr; }
  ~generator() {
    if (coro)
      coro.destroy();
  }
};

struct DataState {
  std::tuple<std::string_view::iterator, HtmlParserResult>
  operator()(std::string_view::iterator it) {
    auto next = it + 1;
    std::string_view view = {it, next};
    return {next, {Data, HtmlToken{.type = Character, .view = view}}};
  }
};

generator tokenize(std::string_view v) {
  HtmlParseStateFunc state = DataState();
  for (auto it = v.begin(); it != v.end();) {
    auto [next, result] = state(it);
    it = next;
    auto [next_state, value] = result;
    if (next_state) {
      // _state = next_state;
    }
    if (value) {
      co_yield *value;
    }
  }
}

TEST(HtmlTest, tokenizer) {
  {
    auto src = " ";
    auto g = tokenize(src);
    EXPECT_TRUE(g.move_next());
    auto token = g.current_value();
    EXPECT_EQ(HtmlToken(Character, src), token);
    EXPECT_FALSE(g.move_next());
  }
  {
    auto src = "a b";
    auto g = tokenize(src);
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ("a", token.view);
    }
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ(" ", token.view);
    }
    {
      EXPECT_TRUE(g.move_next());
      auto token = g.current_value();
      EXPECT_EQ("b", token.view);
    }
    EXPECT_FALSE(g.move_next());
  }
}

TEST(HtmlTest, reference) {
  {
    auto src = "&amp;";
    auto g = tokenize(src);
    EXPECT_TRUE(g.move_next());
    auto token = g.current_value();
    EXPECT_EQ(HtmlToken(Reference, src), token);
    EXPECT_FALSE(g.move_next());
  }
}
