#include "contentinfo.h"
#include <strings.h>
#include <unistd.h>

ContentInfo::~ContentInfo() {
  if (this->sourcefile.size() &&
      (!this->real_type || strncasecmp(this->real_type, "image/", 6))) {
  }
  if (this->mailcap_source) {
    unlink(this->mailcap_source);
  }
}
