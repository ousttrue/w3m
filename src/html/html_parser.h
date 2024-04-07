#pragma once
#include "html_token.h"
#include <string_view>
#include <coroutine>
#include <assert.h>

// https://cpprefjp.github.io/lang/cpp20/coroutines.html

/// 13.2.4.1 The insertion mode
enum HtmlDomInsertionMode {
  Initial,
  BeforeHtml,
  BeforeHead,
  InHead,
  InHeadNoscript,
  AfterHead,
  InBody,
  Text,
  InTable,
  InTableText,
  InCaption,
  InColumnGroup,
  InTableBody,
  InRow,
  InCell,
  InSelect,
  InSelectInTable,
  InTemplate,
  AfterBody,
  InFrameset,
  AfterFrameset,
  AfterAfterBody,
  AfterAfterFrameset,
};

struct HtmlParserState {
  class Context;
  using Result = std::tuple<HtmlToken, std::string_view, HtmlParserState>;
  using StateFunc = Result (*)(std::string_view, Context &);
  class Context {
    const char *_lastOpen = {};
    StateFunc _returnState = {};

  public:
    void setReturnState(StateFunc returnState) { _returnState = returnState; }
    StateFunc returnState() {
      auto p = _returnState;
      assert(p);
      _returnState = {};
      return p;
    }

    void open(std::string_view src) {
      _lastOpen = src.data();
      assert(*_lastOpen == '<');
    }
    void newTag(std::string_view src) { assert(*_lastOpen == '<'); }
    void newDocutype(std::string_view src) { assert(*_lastOpen == '<'); }
    void newComment(std::string_view src) { assert(*_lastOpen == '<'); }
    std::string_view emit(std::string_view car) {
      auto begin = _lastOpen;
      _lastOpen = {};
      return {begin, car.data() + car.size()};
    }
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

html_token_generator html_tokenize(std::string_view v);
