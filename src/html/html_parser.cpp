#include "html_parser.h"
#include "entity.h"
#include "utf8.h"
#include <assert.h>
#include <cctype>
#include <algorithm>

// https://html.spec.whatwg.org/multipage/parsing.html

inline std::tuple<std::string_view, std::string_view>
consume(std::string_view src) {

  auto u = Utf8::from((char8_t *)src.data(), std::min(src.size(), (size_t)4));
  auto it = src.begin() + u.view().size();
  return {{src.begin(), it}, {it, src.end()}};
}

/// 13.2.5.1 Data state
HtmlParserState::Result dataState(std::string_view src,
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
/// 13.2.5.51 Comment end state
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

//
// impl
//

// 1
HtmlParserState::Result dataState(std::string_view src,
                                  HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src);
  auto ch = src.front();
  if (ch == '&') {
    c.return_state = &dataState;
    return {{}, cdr, {characterReferenceState}};
  } else if (ch == '<') {
    return {{}, cdr, {tagOpenState}};
  } else {
    return {car, cdr, {}};
  }
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
    return {src, {}, {}};
  } else {
    //  invalid-first-character-of-tag-name parse error
    return {"<", src, {dataState}};
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
    return {c.emitTag(car), cdr, {dataState}};
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
    return {src, {}, {}};
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
    return {c.emitTag(car), {cdr}, {dataState}};
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
    return {c.emitTag(car), cdr, {dataState}};
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
    c.return_state = attributeValueDoubleQuatedState;
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
    c.return_state = attributeValueSingleQuotedState;
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
    c.return_state = attributeValueUnquotedState;
    return {{}, cdr, {characterReferenceState}};
  } else if (ch == '>') {
    return {c.emitTag(car), cdr, {dataState}};
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
    return {c.emitTag(car), cdr, {dataState}};
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
    return {c.emitTag(car), {cdr}, {dataState}};
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
    return {c.emitComment(car), {cdr}, {dataState}};
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
    return {c.emitComment(car), cdr, {dataState}};
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
    return {src, {}, {}};
    // return {{}, cdr, {commentEndState}};
  } else if (ch == '>') {
    // abrupt-closing-of-empty-comment parse error.
    return {c.emitComment(car), cdr, {dataState}};
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
    return {src, {}, {}};
    // return {{}, cdr, {commentLessThanSignState}};
  } else if (ch == '-') {
    assert(false);
    return {src, {}, {}};
    // return {{}, cdr, {commentEndDashState}};
  } else {
    return {{}, cdr, {}};
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
    return {c.emitDocutype(car), cdr, {dataState}};
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
    return {c.emitDocutype(car), cdr, {dataState}};
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
    return {src, {}, {}};
  } else {
    assert(false);
    return {src, {}, {}};
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
    return {{begin, car.size() + 1}, cdr, {c.return_state}};
  } else {
    return {src, {}, {}};
  }
}

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
