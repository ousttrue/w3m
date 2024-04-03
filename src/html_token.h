#pragma once
#include <string>
#include <optional>

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
