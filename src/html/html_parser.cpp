#include "html_parser.h"
#include "entity.h"
#include <assert.h>
#include <cctype>

// https://html.spec.whatwg.org/multipage/parsing.html

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
  auto [car, cdr] = consume(src, 1);
  auto ch = src.front();
  if (ch == '&') {
    c.return_state = &dataState;
    return {{}, cdr, {characterReferenceState}};
  } else if (ch == '<') {
    return {{}, cdr, {tagOpenState}};
  } else if (ch == 0) {
    return {src, {}, {}};
  } else {
    return {car, cdr, {}};
  }
}

// 6
HtmlParserState::Result tagOpenState(std::string_view src,
                                     HtmlParserState::Context &c) {
  auto [car, cdr] = consume(src, 1);
  auto ch = src.front();
  if (ch == '!') {
    assert(false);
    return {src, {}, {}};
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
  auto [car, cdr] = consume(src, 1);
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
  auto [car, cdr] = consume(src, 1);
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
  auto [car, cdr] = consume(src, 1);
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
  auto [car, cdr] = consume(src, 1);
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
  auto [car, cdr] = consume(src, 1);
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
  auto [car, cdr] = consume(src, 1);
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
  auto [car, cdr] = consume(src, 1);
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
  auto [car, cdr] = consume(src, 1);
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
  auto [car, cdr] = consume(src, 1);
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
  auto [car, cdr] = consume(src, 1);
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
  auto [car, cdr] = consume(src, 1);
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
  auto [car, cdr] = consume(src, 1);
  auto ch = src.front();
  if (ch == '>') {
    return {c.emitComment(car), {cdr}, {dataState}};
  } else {
    c.newComment(src);
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
