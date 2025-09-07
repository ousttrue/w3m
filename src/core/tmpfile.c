#include "tmpfile.h"
#include "indep.h"
#include "rc.h"
#include "ui.h"
#include "textlist.h"
#include "image.h"
#include <string.h>
#include <unistd.h>

TextList* g_fileToDelete = NULL;

void initDeleteFile()
{
    g_fileToDelete = newTextList();
}

void deinitDeleteFile()
{
    for (char* f = popText(g_fileToDelete); f; f = popText(g_fileToDelete)) {
        unlink(f);
        if (enable_inline_image == INLINE_IMG_SIXEL && strcmp(f + strlen(f) - 4, ".gif") == 0) {
            Str firstframe = Strnew_charp(f);
            Strcat_charp(firstframe, "-1");
            unlink(firstframe->ptr);
        }
    }
}

void pushDeleteFile(const char* path)
{
    pushText(g_fileToDelete, path);
}

Str tmpfname(enum TmpFileType type, const char* ext)
{
    static char* tmpf_base[MAX_TMPF_TYPE] = {
        "tmp",
        "src",
        "cache",
        "cookie",
        "hist",
    };
    static unsigned int tmpf_seq[MAX_TMPF_TYPE] = { 0 };

    char* dir;
    switch (type) {
    case TMPF_HIST:
        dir = rc_dir;
        break;
    case TMPF_DFL:
    case TMPF_COOKIE:
    case TMPF_SRC:
    case TMPF_CACHE:
    default:
        dir = tmp_dir;
    }

    Str tmpf = Sprintf("%s/w3m%s%d-%d%s",
        dir,
        tmpf_base[type],
        CurrentPid, tmpf_seq[type]++, (ext) ? ext : "");

    pushDeleteFile(tmpf->ptr);
    return tmpf;
}
