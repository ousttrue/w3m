#include "contentinfo.h"
#include "Str.h"
#include "message.h"
#include <strings.h>
#include <unistd.h>

int FollowRedirection = 10;

ContentInfo::~ContentInfo() {
  if (this->sourcefile.size() &&
      (!this->real_type || strncasecmp(this->real_type, "image/", 6))) {
  }
}

static bool same_url_p(const Url &pu1, const Url &pu2) {
  return (pu1.schema == pu2.schema && pu1.port == pu2.port &&
          (pu1.host.size() ? pu2.host.size() ? pu1.host == pu2.host : false
                           : true) &&
          (pu1.file.size() ? pu2.file.size() ? pu1.file == pu2.file : false
                           : true));
}

bool ContentInfo::checkRedirection(const Url &pu) {

  if (redirectins.size() >= static_cast<size_t>(FollowRedirection)) {
    auto tmp = Sprintf("Number of redirections exceeded %d at %s",
                       FollowRedirection, pu.to_Str().c_str());
    disp_err_message(tmp->ptr, false);
    return false;
  }

  for (auto &url : redirectins) {
    if (same_url_p(pu, url)) {
      // same url found !
      auto tmp = Sprintf("Redirection loop detected (%s)", pu.to_Str().c_str());
      disp_err_message(tmp->ptr, false);
      return false;
    }
  }

  redirectins.push_back(pu);

  return true;
}
