#include "html_tag.h"
#include "cmp.h"
#include "form.h"

bool HtmlTag::setAttr(HtmlTagAttr id, const std::string &value) {
  if (!this->acceptsAttr(id))
    return false;
  auto i = this->map[id];
  this->attrid[i] = id;
  this->value[i] = value;
  this->need_reconstruct = true;
  return true;
}

std::optional<std::string> HtmlTag::getAttr(HtmlTagAttr id) const {
  if (this->map.empty()) {
    return {};
  }
  int i = this->map[id];
  if (!this->existsAttr(id) || this->value[i].empty()) {
    return {};
  }
  return this->value[i];
}

int HtmlTag::ul_type(int default_type) const {
  if (auto value = this->getAttr(ATTR_TYPE)) {
    if (!strcasecmp(*value, "disc"))
      return (int)'d';
    else if (!strcasecmp(*value, "circle"))
      return (int)'c';
    else if (!strcasecmp(*value, "square"))
      return (int)'s';
  }
  return default_type;
}
