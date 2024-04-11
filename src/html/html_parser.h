#pragma once
#include "html_token.h"
#include <string_view>
#include <coroutine>
#include <functional>
#include <assert.h>

// https://cpprefjp.github.io/lang/cpp20/coroutines.html

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
using SetHtmlTokenizerStateFunc =
    std::function<void(HtmlParserState::StateFunc)>;

/// 13.2.5.1 Data state
HtmlParserState::Result dataState(std::string_view src,
                                  HtmlParserState::Context &c);
/// 13.2.5.2 RCDATA state
HtmlParserState::Result rcdataState(std::string_view src,
                                    HtmlParserState::Context &c);
/// 13.2.5.3 RAWTEXT state
HtmlParserState::Result rawtextState(std::string_view src,
                                    HtmlParserState::Context &c);
/// 13.2.5.4 Script data state
HtmlParserState::Result scriptDataState(std::string_view src,
                                    HtmlParserState::Context &c);
/// 13.2.5.5 PLAINTEXT state
HtmlParserState::Result plaintextState(std::string_view src,
                                    HtmlParserState::Context &c);
/// 13.2.5.6 Tag open state
HtmlParserState::Result tagOpenState(std::string_view src,
                                     HtmlParserState::Context &c);
/// 13.2.5.7 End tag open state
HtmlParserState::Result endTagOpenState(std::string_view src,
                                        HtmlParserState::Context &c);
/// 13.2.5.8 Tag name state
HtmlParserState::Result tagNameState(std::string_view src,
                                     HtmlParserState::Context &c);

/// 13.2.5.32 Before attribute name state
HtmlParserState::Result beforeAttributeNameState(std::string_view src,
                                                 HtmlParserState::Context &c);
/// 13.2.5.33 Attribute name state
HtmlParserState::Result attributeNameState(std::string_view src,
                                           HtmlParserState::Context &c);
/// 13.2.5.34 After attribute name state
HtmlParserState::Result afterAttributeNameState(std::string_view src,
                                                HtmlParserState::Context &c);
/// 13.2.5.35 Before attribute value state
HtmlParserState::Result beforeAttributeValueState(std::string_view src,
                                                  HtmlParserState::Context &c);
/// 13.2.5.36 Attribute value (double-quoted) state
HtmlParserState::Result
attributeValueDoubleQuatedState(std::string_view src,
                                HtmlParserState::Context &c);
/// 13.2.5.37 Attribute value (single-quoted) state
HtmlParserState::Result
attributeValueSingleQuotedState(std::string_view src,
                                HtmlParserState::Context &c);
/// 13.2.5.38 Attribute value (unquoted) next_state
HtmlParserState::Result
attributeValueUnquotedState(std::string_view src, HtmlParserState::Context &c);
/// 13.2.5.39 After attribute value (quoted) state
HtmlParserState::Result
afterAttributeValueQuotedState(std::string_view src,
                               HtmlParserState::Context &c);
/// 13.2.5.40 Self-closing start tag state
HtmlParserState::Result SelfClosingStartTagState(std::string_view src,
                                                 HtmlParserState::Context &c);
/// 13.2.5.40 Self-closing start tag state
HtmlParserState::Result selfClosingStartTagState(std::string_view src,
                                                 HtmlParserState::Context &c);
/// 13.2.5.41 Bogus comment state
HtmlParserState::Result bogusCommentState(std::string_view src,
                                          HtmlParserState::Context &c);
/// 13.2.5.42 Markup declaration open state
HtmlParserState::Result markupDeclarationOpenState(std::string_view src,
                                                   HtmlParserState::Context &c);
/// 13.2.5.43 Comment start state
HtmlParserState::Result commentStartState(std::string_view src,
                                          HtmlParserState::Context &c);
/// 13.2.5.44 Comment start dash state
HtmlParserState::Result commentStartDashState(std::string_view src,
                                              HtmlParserState::Context &c);
/// 13.2.5.45 Comment state
HtmlParserState::Result commentState(std::string_view src,
                                     HtmlParserState::Context &c);
/// 13.2.5.46 Comment less-than sign state
/// 13.2.5.47 Comment less-than sign bang next_state
/// 13.2.5.48 Comment less-than sign bang dash state
/// 13.2.5.50 Comment end dash state
HtmlParserState::Result commentEndDashState(std::string_view src,
                                            HtmlParserState::Context &c);
/// 13.2.5.51 Comment end state
HtmlParserState::Result commentEndState(std::string_view src,
                                        HtmlParserState::Context &c);
/// 13.2.5.52 Comment end bang state
/// 13.2.5.53 DOCTYPE state
HtmlParserState::Result doctypeState(std::string_view src,
                                     HtmlParserState::Context &c);
/// 13.2.5.54 Before DOCTYPE name state
HtmlParserState::Result beforeDoctypeNameState(std::string_view src,
                                               HtmlParserState::Context &c);
/// 13.2.5.55 DOCTYPE name state
HtmlParserState::Result doctypeNameState(std::string_view src,
                                         HtmlParserState::Context &c);
/// 13.2.5.56 After DOCTYPE name state
/// 13.2.5.57 After DOCTYPE public keyword state
/// 13.2.5.58 Before DOCTYPE public identifier state
/// 13.2.5.59 DOCTYPE public identifier (double-quoted) state
/// 13.2.5.60 DOCTYPE public identifier (single-quoted) state
/// 13.2.5.61 After DOCTYPE public identifier state
/// 13.2.5.62 Between DOCTYPE public and system identifiers state
/// 13.2.5.63 After DOCTYPE system keyword state
/// 13.2.5.64 Before DOCTYPE system identifier state
/// 13.2.5.65 DOCTYPE system identifier (double-quoted) state
/// 13.2.5.66 DOCTYPE system identifier (single-quoted) state
/// 13.2.5.67 After DOCTYPE system identifier state
/// 13.2.5.68 Bogus DOCTYPE state

/// 13.2.5.72 Character reference state
HtmlParserState::Result characterReferenceState(std::string_view src,
                                                HtmlParserState::Context &c);
/// 13.2.5.73 Named character reference state
HtmlParserState::Result
namedCharacterReferenceState(std::string_view src, HtmlParserState::Context &c);

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

struct HtmlParser {
  HtmlParserState state{dataState};
  html_token_generator tokenize(std::string_view v);
};
