#pragma once
#include <string>
#include <optional>
#include <ostream>
#include <vector>
#include <array>

struct Token {
  bool is_tag;
  std::string str;
};

class html_feed_environ;
struct TableStatus;
struct Tokenizer {
  std::string line;
  Tokenizer(std::string_view html) {
    line = std::string{html.begin(), html.end()};
  }

  std::optional<Token> getToken(html_feed_environ *h_env, TableStatus &t,
                                bool internal);
};

// DOCTYPE, start tag, end tag, comment, character, end-of-file
enum HtmlTokenTypes {
  HtmlToken_Unknown,
  Doctype,
  Tag,
  Comment,
  Character,
  Eof,
};
inline const char *str(const HtmlTokenTypes t) {
  switch (t) {
  case HtmlToken_Unknown:
    return "HtmlToken_Unknown";
  case Doctype:
    return "Doctype";
  case Tag:
    return "Tag";
  case Comment:
    return "Comment";
  case Character:
    return "Character";
  case Eof:
    return "Eof";
  }

  return "invalid value";
}

struct HtmlToken {
  HtmlTokenTypes type;
  std::string_view view;

  bool operator==(const HtmlToken &rhs) const {
    return type == rhs.type && view == rhs.view;
  }

  bool isStartTag() const;
  bool isStartTag(std::string_view tag) const;

  bool isAnyStartTag(const char *tag) const { return isStartTag(tag); }
  template <typename... ARGS>
  bool isAnyStartTag(const char *tag, ARGS... args) const {
    if (isStartTag(tag)) {
      return true;
    }
    return isAnyStartTag(args...);
  }

  bool isEndTag() const;
  bool isEndTag(std::string_view tag) const;
  bool isAnyEndTag(const char *tag) const { return isEndTag(tag); }
  template <typename... ARGS>
  bool isAnyEndTag(const char *tag, ARGS... args) const {
    if (isEndTag(tag)) {
      return true;
    }
    return isAnyEndTag(args...);
  }

  std::string tag() const {
    if (type != Tag) {
      return {};
    }
    auto it = view.begin();
    if (*it != '<') {
      return {};
    }
    ++it;
    if (it == view.end()) {
      return {};
    }
    if (*it == '/') {
      ++it;
    }
    std::string tag;
    for (; std::isalpha(*it); ++it) {
      tag.push_back(std::tolower(*it));
    }
    return tag;
  }
};
inline std::ostream &operator<<(std::ostream &os, const HtmlToken &token) {
  os << "[" << str(token.type) << "] '" << token.view << "'";
  return os;
}
