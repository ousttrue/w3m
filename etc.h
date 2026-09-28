#ifndef W3M_ETC_H
#define W3M_ETC_H

#include "textlist.h"

enum {
    TMPF_DFL,
    TMPF_SRC,
    TMPF_FRAME,
    TMPF_CACHE,
    TMPF_COOKIE,
    TMPF_HIST,
    MAX_TMPF_TYPE,
};

extern TextList *fileToDelete;
#endif
