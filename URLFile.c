#include "URLFile.h"
#include "compression.h"
#include "alloc.h"
#include "file.h"
#include "fm.h"
#include "ftp.h"
#include "w3m_rc.h"
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <setjmp.h>
#include <signal.h>

void init_stream(struct URLFile* uf, int scheme, struct input_stream* stream)
{
    memset(uf, 0, sizeof(struct URLFile));
    uf->stream = stream;
    uf->scheme = scheme;
    uf->is_cgi = false;
    uf->ext = NULL;
    uf->modtime = -1;
}

void UFhalfclose(struct URLFile* f)
{
    switch (f->scheme) {
    case SCM_FTP:
        closeFTP();
        break;
    default:
        UFclose(f);
        break;
    }
}

static JMP_BUF AbortLoading;
static MySignalHandler
KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
}

