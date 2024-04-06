#include <gtest/gtest.h>
#include <functional>
#include <coroutine>
#include "entity.h"

// https://html.spec.whatwg.org/multipage/parsing.html
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

enum HtmlParserStateType {
  DataStateType,
  CharacterReferenceStateType,
};

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

inline std::tuple<std::string_view, std::string_view>
consume(std::string_view src, size_t n) {
  auto it = src.begin() + n;
  return {{src.begin(), it}, {it, src.end()}};
}

// 13.2.5.73 Named character reference state
HtmlParserState::Result
namedCharacterReferenceState(std::string_view src,
                             HtmlParserState::Context &c) {
  auto [car, cdr] = matchNamedCharacterReference(src);
  if (car.size()) {
    auto begin = car.data() - 1;
    assert(*begin == '&');
    return {{begin, car.size() + 1}, cdr, {c.return_state}};
  } else {
    return {src, {}, {}};
  }
}

// 13.2.5.72 Character reference state
HtmlParserState::Result characterReferenceState(std::string_view src,
                                                HtmlParserState::Context &c) {
  auto ch = src.front();
  if (std::isalnum(ch)) {
    return {{}, src, {namedCharacterReferenceState}};
  } else if (ch == '#') {
    return {src, {}, {}};
  } else {
    return {src, {}, {}};
  }
}

// 13.2.5.1 Data state
HtmlParserState::Result dataState(std::string_view src,
                                  HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src, 1);
  auto ch = src.front();
  if (ch == '&') {
    c.return_state = &dataState;
    return {{}, cdr, {characterReferenceState}};
  } else if (ch == '<') {
    return {src, {}, {}};
  } else if (ch == 0) {
    return {src, {}, {}};
  } else {
    return {car, cdr, {}};
  }
}

//
// generator
//
generator tokenize(std::string_view v) {
  HtmlParserState state{dataState};
  HtmlParserState::Context c;
  while (v.size()) {
    auto [token, next, next_state] = state.parse(v, c);
    v = next;
    if (next_state.parse) {
      state = next_state;
    }
    if (token.size()) {
      // emit
      co_yield {Character, token};
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

TEST(HtmlTest, named) {
  {
    auto src = "&amp;";
    auto [found, remain] = matchNamedCharacterReference(src + 1);
    EXPECT_EQ("amp;", found);
    EXPECT_EQ("", remain);
  }
}

TEST(HtmlTest, reference) {
  {
    auto src = "&amp;";
    auto g = tokenize(src);
    EXPECT_TRUE(g.move_next());
    auto token = g.current_value();
    EXPECT_EQ("&amp;", token.view);
    EXPECT_FALSE(g.move_next());
  }
}
