#include "buffer_list.h"
#include "buffer_util.h"
#include "Content.h"
#include "HttpRequest.h"
#include "history.h"
#include "image_loader.h"
#include "linein.h"
#include "quote.h"
#include "ui.h"
#include "runtime.h"

struct Buffer* Currentbuf = 0;
struct Buffer* Firstbuf = 0;
char ArgvIsURL = true;
int clear_buffer = (true);

void parseArgs(int argc, char** argv)
{
    struct UI ui = getUI();
    const char* url = (getUrlScheme(argv[1]) == SCM_MISSING && !ArgvIsURL)
        ? file_to_url(argv[1], CurrentDir)
        : url_quote(conv_from_system(argv[1]));

    struct Content c = getContent(url, NULL, NULL, NO_REFERER, (struct UserInteraction) { 0 });
    struct Buffer* newbuf = makeBuffer(&c, ui.viewport.size.x, ui.use_graphic);

    switch (newbuf->content.url.scheme) {
    case SCM_MAILTO:
        break;
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
        unshiftHist(LoadHist, url);
    default:
        pushHashHist(URLHist, parsedURL2Str(&newbuf->content.url)->ptr);
        break;
    }
    Firstbuf = Currentbuf = newbuf;
    // saveBufferInfo(ui);
}

void SAVE_BUFPOSITION(struct Buffer* sbufp)
{
    COPY_DOCUMENT_POSITION(&sbufp->document, &Currentbuf->document);
}

// void saveBufferInfo(struct UI ui)
// {
//     FILE* fp;
//     if ((fp = fopen(rcFile("bufinfo"), "w")) == NULL) {
//         return;
//     }
//     fprintf(fp, "%s\n", content.url(ui)->ptr);
//     fclose(fp);
// }

void pushBuffer(struct Buffer* buf)
{
    if (!buf) {
        return;
    }
    pushHashHist(URLHist, parsedURL2Str(&buf->content.url)->ptr);

    deleteImage(&buf->document);
    if (clear_buffer) {
        // clearBuffer(ui.current_buffer);
    }

    buf->nextBuffer = Currentbuf;
    Currentbuf = buf;

    struct Buffer* b;
    if (Firstbuf == Currentbuf) {
        Firstbuf = buf;
    } else if ((b = prevBuffer(Firstbuf, Currentbuf))) {
        b->nextBuffer = buf;
    }
}

void delBuffer(struct Buffer* buf)
{
    if (buf == NULL)
        return;
    if (Currentbuf == buf)
        Currentbuf = buf->nextBuffer;
    Firstbuf = deleteBuffer(Firstbuf, buf);
    if (!Currentbuf)
        Currentbuf = Firstbuf;

    if (Firstbuf == NULL) {
        /* No more buffer */
        Firstbuf = nullBuffer();
        Currentbuf = Firstbuf;
    }
}

void repBuffer(struct Buffer* oldbuf, struct Buffer* buf)
{
    Firstbuf = replaceBuffer(Firstbuf, oldbuf, buf);
    Currentbuf = buf;
}

void setCurrentBuffer(struct Buffer* buf)
{
    if (!buf) {
        return;
    }
    Currentbuf = buf;

    // ui.current_buffer = buf;
    for (buf = Firstbuf; buf != NULL; buf = buf->nextBuffer) {
        if (buf == Currentbuf)
            continue;
        deleteImage(&buf->document);
        if (clear_buffer)
            tmpClearBuffer(buf);
    }
}

struct Buffer* pushContent(struct Content c, int cols, bool use_graphic)
{
    struct Buffer* buf = makeBuffer(&c, cols, use_graphic);
    if (!buf) {
        Str emsg = Sprintf("Can't load %s", parsedURL2Str(&c.url)->ptr);
        message(getUI(), MSG_ERR, emsg->ptr);
        return 0;
    }
    pushBuffer(buf);
    return buf;
}
