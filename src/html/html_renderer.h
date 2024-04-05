#pragma once
#include "form.h"
#include "AnchorList.h"
#include "lineprop.h"
#include "html_command.h"
#include "line.h"
#include <memory>

struct LineData;
struct Url;
class html_feed_environ;

class HtmlRenderer {
  Lineprop effect = 0;
  Lineprop ex_effect = 0;
  char symbol = '\0';
  Anchor *a_href = nullptr;
  Anchor *a_img = nullptr;
  FormAnchor *a_form = nullptr;
  HtmlCommand internal = {};

public:
  std::shared_ptr<LineData>
  render(const Url &currentUrl, html_feed_environ *h_env,
         const std::shared_ptr<AnchorList<FormAnchor>> &old);

private:
  Line renderLine(const Url &url, html_feed_environ *h_env,
                  const std::shared_ptr<LineData> &data, int nlines,
                  const char *str,
                  const std::shared_ptr<AnchorList<FormAnchor>> &old);
};
