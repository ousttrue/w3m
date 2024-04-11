#include "html_parser.h"
#include "entity.h"
#include "utf8.h"
#include "cmp.h"
#include <assert.h>
#include <cctype>
#include <algorithm>
#include <regex>

// https://html.spec.whatwg.org/multipage/parsing.html

inline std::tuple<std::string_view, std::string_view>
consume(std::string_view src) {

  auto u = Utf8::from((char8_t *)src.data(), std::min(src.size(), (size_t)4));
  auto it = src.begin() + u.view().size();
  return {{src.begin(), it}, {it, src.end()}};
}

//
// impl
//

// 1
HtmlParserState::Result dataState(std::string_view src,
                                  HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '&') {
    c.setReturnState(&dataState);
    return {{}, cdr, {characterReferenceState}};
  } else if (ch == '<') {
    c.open(src);
    return {{}, cdr, {tagOpenState}};
  } else {
    return {{Character, car}, cdr, {}};
  }
}

// 2
HtmlParserState::Result rcdataState(std::string_view src,
                                    HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '&') {
    c.setReturnState(&rcdataState);
    return {{}, cdr, {characterReferenceState}};
  } else if (ch == '<') {
    // TODO:
    assert(false);
    return {{}, cdr, {}};
  } else {
    return {{Character, car}, cdr, {}};
  }
}

/// 13.2.5.3 RAWTEXT state
HtmlParserState::Result rawtextState(std::string_view src,
                                     HtmlParserState::Context &c) {
  // TODO:
  assert(false);
  return {};
}

/// 13.2.5.4 Script data state
HtmlParserState::Result scriptDataState(std::string_view src,
                                        HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '<') {
    // TODO:
    assert(false);
    return {};
  } else {
    //  invalid-first-character-of-tag-name parse error
    return {{Character, c.emit(car)}, src, {dataState}};
  }
}

/// 13.2.5.5 PLAINTEXT state
HtmlParserState::Result plaintextState(std::string_view src,
                                       HtmlParserState::Context &c) {
  // TODO:
  assert(false);
  return {};
}

// 6
HtmlParserState::Result tagOpenState(std::string_view src,
                                     HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '!') {
    return {{}, cdr, {markupDeclarationOpenState}};
  } else if (ch == '/') {
    return {{}, cdr, {endTagOpenState}};
  } else if (std::isalpha(ch)) {
    c.newTag(src);
    return {{}, src, {tagNameState}};
  } else if (ch == '?') {
    assert(false);
    return {{Character, src}, {}, {}};
  } else {
    //  invalid-first-character-of-tag-name parse error
    return {{Character, c.emit(car)}, src, {dataState}};
  }
}

// 7
HtmlParserState::Result endTagOpenState(std::string_view src,
                                        HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (std::isalpha(ch)) {
    c.newTag(src);
    return {{}, src, {tagNameState}};
  } else if (ch == '>') {
    // missing-end-tag-name parse error.
    return {{}, cdr, {dataState}};
  } else {
    // invalid-first-character-of-tag-name parse error.
    return {{}, src, {bogusCommentState}};
  }
}

// 8
HtmlParserState::Result tagNameState(std::string_view src,
                                     HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
    return {{}, cdr, {beforeAttributeNameState}};
  } else if (ch == '/') {
    return {{}, cdr, {selfClosingStartTagState}};
  } else if (ch == '>') {
    return {{Tag, c.emit(car)}, cdr, {dataState}};
  } else {
    return {{}, cdr, {}};
  }
}

// 32
HtmlParserState::Result beforeAttributeNameState(std::string_view src,
                                                 HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
    return {{}, cdr, {}};
  } else if (ch == '/' || ch == '>') {
    return {{}, src, {afterAttributeNameState}};
  } else if (ch == '=') {
    // unexpected-equals-sign-before-attribute-name parse error
    return {{}, cdr, {attributeNameState}};
  } else {
    return {{}, cdr, {attributeNameState}};
  }
}

/// 33
HtmlParserState::Result attributeNameState(std::string_view src,
                                           HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20 || ch == '/' ||
      ch == '>') {
    return {{}, src, {attributeNameState}};
  } else if (ch == '=') {
    return {{}, cdr, {beforeAttributeValueState}};
  } else if (ch == '"' || ch == '\'' || ch == '<') {
    // unexpected-null-character parse error.
    assert(false);
    return {{{}, src}, {}, {}};
  } else {
    // push attribute name
    return {{}, cdr, {}};
  }
}

/// 34
HtmlParserState::Result afterAttributeNameState(std::string_view src,
                                                HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
    return {{}, cdr, {}};
  } else if (ch == '/') {
    return {{}, cdr, {selfClosingStartTagState}};
  } else if (ch == '=') {
    return {{}, cdr, {beforeAttributeValueState}};
  } else if (ch == '>') {
    return {{Tag, c.emit(car)}, {cdr}, {dataState}};
  } else {
    return {{}, src, {attributeNameState}};
  }
}

/// 35
HtmlParserState::Result beforeAttributeValueState(std::string_view src,
                                                  HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
    return {{}, cdr, {}};
  } else if (ch == '"') {
    return {{}, cdr, {attributeValueDoubleQuatedState}};
  } else if (ch == '\'') {
    return {{}, cdr, {attributeValueSingleQuotedState}};
  } else if (ch == '>') {
    return {{Tag, c.emit(car)}, cdr, {dataState}};
  } else {
    return {{}, src, {attributeValueUnquotedState}};
  }
}

/// 36
HtmlParserState::Result
attributeValueDoubleQuatedState(std::string_view src,
                                HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '"') {
    return {{}, cdr, {afterAttributeValueQuotedState}};
  } else if (ch == '&') {
    c.setReturnState(attributeValueDoubleQuatedState);
    return {{}, cdr, {characterReferenceState}};
  } else {
    return {{}, cdr, {}};
  }
}

// 37
HtmlParserState::Result
attributeValueSingleQuotedState(std::string_view src,
                                HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '\'') {
    return {{}, cdr, {afterAttributeValueQuotedState}};
  } else if (ch == '&') {
    c.setReturnState(attributeValueSingleQuotedState);
    return {{}, cdr, {characterReferenceState}};
  } else {
    return {{}, cdr, {}};
  }
}

/// 38
HtmlParserState::Result
attributeValueUnquotedState(std::string_view src, HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
    return {{}, cdr, {beforeAttributeNameState}};
  } else if (ch == '&') {
    c.setReturnState(attributeValueUnquotedState);
    return {{}, cdr, {characterReferenceState}};
  } else if (ch == '>') {
    return {{Tag, c.emit(car)}, cdr, {dataState}};
  } else if (ch == '"' || ch == '\'' || ch == '<' || ch == '=' || ch == '`') {
    // unexpected-character-in-unquoted-attribute-value parse error.
    return {{}, cdr, {}};
  } else {
    return {{}, cdr, {}};
  }
}

/// 39
HtmlParserState::Result
afterAttributeValueQuotedState(std::string_view src,
                               HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
    return {{}, cdr, {beforeAttributeNameState}};
  } else if (ch == '/') {
    return {{}, cdr, {selfClosingStartTagState}};
  } else if (ch == '>') {
    return {{Tag, c.emit(car)}, cdr, {dataState}};
  } else {
    // missing-whitespace-between-attributes parse error
    return {{}, src, {beforeAttributeNameState}};
  }
}

// 40
HtmlParserState::Result selfClosingStartTagState(std::string_view src,

                                                 HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '>') {
    // c.selfClosing=true;
    return {{Tag, c.emit(car)}, {cdr}, {dataState}};
  } else {
    // unexpected-solidus-in-tag parse error.
    return {{}, src, {beforeAttributeNameState}};
  }
}

// 41
HtmlParserState::Result bogusCommentState(std::string_view src,
                                          HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '>') {
    return {{Comment, c.emit(car)}, {cdr}, {dataState}};
  } else {
    c.newComment(src);
    return {{}, cdr, {}};
  }
}

// 42
HtmlParserState::Result
markupDeclarationOpenState(std::string_view src, HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '-' && cdr.starts_with("-")) {
    c.newComment(src);
    return {{}, cdr.substr(1), {commentStartState}};
  } else if (src.starts_with("DOCTYPE")) {
    return {{}, src.substr(7), {doctypeState}};
  } else if (src.starts_with("[CDATA[")) {
    assert(false);
    return {{}, {src}, {}};
  } else {
    //  incorrectly-opened-comment parse error
    c.newComment(src);
    return {{}, cdr, {bogusCommentState}};
  }
}

// 43
HtmlParserState::Result commentStartState(std::string_view src,
                                          HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '-') {
    return {{}, cdr, {commentStartDashState}};
  } else if (ch == '>') {
    // abrupt-closing-of-empty-comment parse error.
    return {{Comment, c.emit(car)}, cdr, {dataState}};
  } else {
    return {{}, src, {commentState}};
  }
}

// 44
HtmlParserState::Result commentStartDashState(std::string_view src,
                                              HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '-') {
    assert(false);
    return {{{}, src}, {}, {}};
    // return {{}, cdr, {commentEndState}};
  } else if (ch == '>') {
    // abrupt-closing-of-empty-comment parse error.
    return {{Comment, c.emit(car)}, cdr, {dataState}};
  } else {
    return {{}, src, {commentState}};
  }
}

// 45
HtmlParserState::Result commentState(std::string_view src,
                                     HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '<') {
    assert(false);
    return {{{}, src}, {}, {}};
    // return {{}, cdr, {commentLessThanSignState}};
  } else if (ch == '-') {
    return {{}, cdr, {commentEndDashState}};
  } else {
    return {{}, cdr, {}};
  }
}

// 50
HtmlParserState::Result commentEndDashState(std::string_view src,
                                            HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '-') {
    return {{}, cdr, {commentEndState}};
  } else {
    return {{}, src, {commentState}};
  }
}

// 51
HtmlParserState::Result commentEndState(std::string_view src,
                                        HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '>') {
    return {{Comment, c.emit(car)}, cdr, {dataState}};
  } else if (ch == '!') {
    assert(false);
    return {{{}, src}, {}, {}};
  } else if (ch == '-') {
    return {{}, cdr, {}};
  } else {
    return {{}, src, {commentState}};
  }
}

// 53
HtmlParserState::Result doctypeState(std::string_view src,
                                     HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
    return {{}, cdr, {beforeDoctypeNameState}};
  } else if (ch == '>') {
    assert(false);
    return {{}, {src}, {}};
    // return {{}, src, {beforeDoctypeNameState}};
  } else {
    // missing-whitespace-before-doctype-name parse error
    assert(false);
    return {{}, {src}, {}};
    // return {{}, src, {beforeDoctypeNameState}};
  }
}

// 54 Before DOCTYPE name state
HtmlParserState::Result beforeDoctypeNameState(std::string_view src,
                                               HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
    return {{}, cdr, {}};
  } else if (ch == '>') {
    // missing-doctype-name parse error.
    c.newDocutype(src);
    return {{Doctype, c.emit(car)}, cdr, {dataState}};
  } else {
    c.newDocutype(src);
    return {{}, cdr, {doctypeNameState}};
  }
}

// 55
HtmlParserState::Result doctypeNameState(std::string_view src,
                                         HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == 0x09 || ch == 0x0a || ch == 0x0c || ch == 0x20) {
    assert(false);
    return {{}, {src}, {}};
    // return {{}, cdr, {afterDoctypeNameState}};
  } else if (ch == '>') {
    return {{Doctype, c.emit(car)}, cdr, {dataState}};
  } else {
    return {{}, cdr, {}};
  }
}

// 72
HtmlParserState::Result characterReferenceState(std::string_view src,
                                                HtmlParserState::Context &c) {
  auto ch = src.front();
  if (std::isalnum(ch)) {
    return {{}, src, {namedCharacterReferenceState}};
  } else if (ch == '#') {
    assert(false);
    return {{{}, src}, {}, {}};
  } else {
    assert(false);
    return {{{}, src}, {}, {}};
  }
}

// 73
HtmlParserState::Result
namedCharacterReferenceState(std::string_view src,
                             HtmlParserState::Context &c) {
  auto [car, cdr] = matchNamedCharacterReference(src);
  if (car.size()) {
    auto begin = car.data() - 1;
    assert(*begin == '&');
    return {{Character, {begin, car.size() + 1}}, cdr, {c.returnState()}};
  }

  return {{Character, car}, cdr, {ambiguousAmpersandState}};
}

// 74
HtmlParserState::Result ambiguousAmpersandState(std::string_view src,
                                                HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (std::isalnum(ch)) {
    return {{Character, car}, cdr, {}};
  }

  if (ch == ';') {
  }

  return {{}, src, {c.returnState()}};
}

html_token_generator HtmlParser::tokenize(std::string_view v) {
  HtmlParserState::Context c;
  while (v.size()) {
    auto [token, next, next_state] = state.parse(v, c);
    assert(token.view.empty() || token.type != HtmlToken_Unknown);
    v = next;
    if (next_state.parse) {
      state = next_state;
    }
    if (token.view.size()) {
      // emit
      co_yield token;

      // skip script
      // if (token.isStartTag("script")) {
      //   // search "</script>";
      //   std::regex re(R"(</script>)", std::regex_constants::icase);
      //   // std::match_results<std::list<char>::const_iterator> m;
      //   std::cmatch m;
      //   if (std::regex_search(v.data(), v.data() + v.size(), m, re)) {
      //     std::string_view data{v.data(), m[0].first};
      //     if (data.size()) {
      //       co_yield {Character, data};
      //     }
      //     v = {m[0].first, v.data() + v.size()};
      //   } else {
      //     assert(false);
      //   }
      // }
    }
  }
}
