#include "URLFile.h"
#include "ftp.h"

void UFhalfclose(struct URLFile* f, enum UrlScheme scheme)
{
    switch (scheme) {
    case SCM_FTP:
        closeFTP();
        break;
    default:
        is_close(f->stream);
        break;
    }
}
