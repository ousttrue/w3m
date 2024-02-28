#pragma once
#include <string_view>
#include <string>
#include <string_view>
#include <list>
#include <unordered_map>
#include <memory>

extern bool UseHistory;
extern int URLHistSize;
extern bool SaveURLHist;

struct Hist {

private:
  std::list<std::string> list;
  std::list<std::string>::iterator current;
  std::unordered_map<std::string, bool> hash;
  long long mtime = {};

  Hist();

public:
  static std::shared_ptr<Hist> newHist();
  bool loadHistory();
  void saveHistory(int size);
  std::shared_ptr<Hist> copyHist() const;
  void mergeHistory(const Hist &theirs);
  std::string_view nextHist();
  std::string_view prevHist();
  std::string_view lastHist();
  void push(std::string_view ptr);
  bool getHashHist(std::string_view ptr) const;
  std::string toHtml() const;
};

extern std::shared_ptr<Hist> LoadHist;
extern std::shared_ptr<Hist> SaveHist;
extern std::shared_ptr<Hist> URLHist;
extern std::shared_ptr<Hist> ShellHist;
extern std::shared_ptr<Hist> TextHist;
