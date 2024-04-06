#include "html_parser.h"
#include "entity.h"
#include <assert.h>

// https://html.spec.whatwg.org/multipage/parsing.html

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
html_token_generator html_tokenize(std::string_view v) {
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
