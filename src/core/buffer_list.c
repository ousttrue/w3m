#include "buffer_list.h"
#include "buffer.h"
#include "Content.h"
#include "HttpRequest.h"
#include "buffer.h"
#include "buffer_loader.h"
#include "history.h"
#include "image.h"
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

    struct Content c = loadGeneralFile(url, NULL, NULL, NO_REFERER, (struct UserInteraction) { 0 });
    struct Buffer* newbuf = makeBuffer(ui, &c);

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
    COPY_DOCUMENT_POSITION(sbufp, Currentbuf);
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

void pushBuffer(struct UI ui, struct Buffer* buf)
{
    if (!buf) {
        return;
    }
    pushHashHist(URLHist, parsedURL2Str(&buf->content.url)->ptr);

    deleteImage(ui.current_buffer);
    if (clear_buffer)
        clearBuffer(ui.current_buffer);

    buf->nextBuffer = ui.current_buffer;
    Currentbuf = buf;
    struct Buffer* b;
    if (Firstbuf == ui.current_buffer) {
        Firstbuf = buf;
    } else if ((b = prevBuffer(Firstbuf, ui.current_buffer)) != NULL) {
        b->nextBuffer = buf;
    }
}

void delBuffer(struct UI ui, struct Buffer* buf)
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

    // if (ui.current_buffer == buf)
    //     ui.current_buffer = buf->nextBuffer;
    // Firstbuf = deleteBuffer(Firstbuf, buf);
    // if (!ui.current_buffer)
    //     ui.current_buffer = nthBuffer(Firstbuf, i - 1);
    // ;
    // if (Firstbuf == NULL) {
    //     Firstbuf = nullBuffer();
    //     ui.current_buffer = Firstbuf;
    // }
}

void repBuffer(struct UI ui, struct Buffer* oldbuf, struct Buffer* buf)
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
        deleteImage(buf);
        if (clear_buffer)
            tmpClearBuffer(buf);
    }
}

struct Buffer* pushContent(struct UI ui, struct Content c)
{
    struct Buffer* buf = makeBuffer(ui, &c);
    if (!buf) {
        Str emsg = Sprintf("Can't load %s", parsedURL2Str(&c.url)->ptr);
        message(getUI(), MSG_ERR, emsg->ptr);
        return 0;
    }
    pushBuffer(ui, buf);
    return buf;
}
