#pragma once
#include <string>
#include <optional>
#include <ostream>

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
  Unknown,
  Doctype,
  Tag,
  Comment,
  Character,
};
inline const char *str(const HtmlTokenTypes t) {
  switch (t) {
  case Unknown:
    return "Unknown";
  case Doctype:
    return "Doctype";
  case Tag:
    return "Tag";
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
    return type == rhs.type && view == rhs.view;
  }

  bool isStartTag(std::string_view tag) const;
};
inline std::ostream &operator<<(std::ostream &os, const HtmlToken &token) {
  os << "[" << str(token.type) << "] '" << token.view << "'";
  return os;
}
