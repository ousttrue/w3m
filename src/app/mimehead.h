#pragma once
#include <string>
#include <vector>
#include <string.h>

struct growbuf {
  std::vector<char> _buf;

  growbuf() {}
  growbuf(const growbuf &) = delete;
  growbuf &operator=(const growbuf &) = delete;

  size_t length() const { return _buf.size(); }
  void clear() { _buf.clear(); }
  std::string to_Str() {
    std::string str(_buf.begin(), _buf.end());
    _buf.clear();
    return str;
  }
  void append(const unsigned char *src, int len) {
    auto size = _buf.size();
    _buf.resize(size + len);
    memcpy(_buf.data() + size, src, len);
  }
  void GROWBUF_ADD_CHAR(char ch) { _buf.push_back(ch); }
};

std::string decodeB(const char **ww);
void decodeB_to_growbuf(struct growbuf *gb, const char **ww);
std::string decodeQ(const char **ww);
std::string decodeQP(const char **ww);
void decodeQP_to_growbuf(struct growbuf *gb, const char **ww);
std::string decodeU(const char **ww);
void decodeU_to_growbuf(struct growbuf *gb, const char **ww);
std::string decodeMIME0(const std::string &orgstr);
