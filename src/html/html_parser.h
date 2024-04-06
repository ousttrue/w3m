#pragma once
#include <string_view>
#include <ostream>
#include <functional>
#include <coroutine>

// https://cpprefjp.github.io/lang/cpp20/coroutines.html

// DOCTYPE, start tag, end tag, comment, character, end-of-file
enum HtmlTokenTypes {
  Unknown,
  Doctype,
  StartTag,
  EndTag,
  Comment,
  Character,
};
inline const char *str(const HtmlTokenTypes t) {
  switch (t) {
  case Unknown:
    return "Unknown";
  case Doctype:
    return "Doctype";
  case StartTag:
    return "StartTag";
  case EndTag:
    return "EndTag";
  case Comment:
    return "Comment";
  case Character:
    return "Character";
  }
}

struct HtmlToken {
  HtmlTokenTypes type;
  std::string_view view;

  bool operator==(const HtmlToken &rhs) const {
    return type == rhs.type && view.size() == rhs.view.size() &&
           view.begin() == rhs.view.begin();
  }
};
inline std::ostream &operator<<(std::ostream &os, const HtmlToken &token) {
  os << "[" << str(token.type) << "] '" << token.view << "'";
  return os;
}

struct HtmlParserState {
  struct Context;
  using Result =
      std::tuple<std::string_view, std::string_view, HtmlParserState>;
  using StateFunc = std::function<Result(std::string_view, Context &)>;
  struct Context {
    // std::string tmp;
    StateFunc return_state;
  };
  StateFunc parse;
};

struct html_token_generator {
  struct promise_type;
  using handle = std::coroutine_handle<promise_type>;

private:
  html_token_generator(handle h) : coro(h) {}
  handle coro;

public:
  struct promise_type {
    HtmlToken current_value;
    static auto get_return_object_on_allocation_failure() {
      return html_token_generator{nullptr};
    }
    auto get_return_object() {
      return html_token_generator{handle::from_promise(*this)};
    }
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
  html_token_generator(html_token_generator const &) = delete;
  html_token_generator(html_token_generator &&rhs) : coro(rhs.coro) {
    rhs.coro = nullptr;
  }
  ~html_token_generator() {
    if (coro)
      coro.destroy();
  }
};

inline std::tuple<std::string_view, std::string_view>
consume(std::string_view src, size_t n) {
  auto it = src.begin() + n;
  return {{src.begin(), it}, {it, src.end()}};
}

html_token_generator tokenize(std::string_view v);
