#include "tab.h"
#include "image.h"
#include "rc.h"

void pushBuffer(Buffer* buf)
{
    deleteImage(Currentbuf);
    if (clear_buffer)
        tmpClearBuffer(Currentbuf);

    Buffer* b;
    if (Firstbuf == Currentbuf) {
        buf->nextBuffer = Firstbuf;
        Firstbuf = Currentbuf = buf;
    } else if ((b = prevBuffer(Firstbuf, Currentbuf)) != NULL) {
        b->nextBuffer = buf;
        buf->nextBuffer = Currentbuf;
        Currentbuf = buf;
    }
}
