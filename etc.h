#ifndef W3M_ETC_H
#define W3M_ETC_H

#include "textlist.h"

#define LINELEN	256		/* Initial line length */
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
