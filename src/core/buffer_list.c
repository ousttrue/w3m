#include "buffer_list.h"
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

// char* file_to_url(const char* file, const char* currentDir);
char* file_to_url(const char* file, const char* currentDir)
{
    file = expandPath(file);
    if (!file) {
        return NULL;
    }

    if (file[0] != '/') {
        Str tmp = Strnew_charp(currentDir);
        if (Strlastchar(tmp) != '/')
            Strcat_char(tmp, '/');
        Strcat_charp(tmp, file);
        file = tmp->ptr;
    }

    {
        Str tmp = Strnew_charp("file://");
        Strcat_charp(tmp, file_quote(cleanupName(file)));
        return tmp->ptr;
    }
}

void parseArgs(int argc, char** argv)
{
    struct UI ui = getUI();
    const char* url = (getUrlScheme(argv[1]) == SCM_MISSING && !ArgvIsURL)
        ? file_to_url(argv[1], CurrentDir)
        : url_quote(conv_from_system(argv[1]));

    struct Content c = loadGeneralFile(url, NULL, NULL, NO_REFERER, (struct UserInteraction) { 0 });
    struct Buffer* newbuf = makeBuffer(ui, &c);

    switch (newbuf->currentURL.scheme) {
    case SCM_MAILTO:
        break;
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
        unshiftHist(LoadHist, url);
    default:
        pushHashHist(URLHist, parsedURL2Str(&newbuf->currentURL)->ptr);
        break;
    }
    Firstbuf = Currentbuf = newbuf;
    // saveBufferInfo(ui);
}

void SAVE_BUFPOSITION(struct Buffer* sbufp)
{
    COPY_BUFPOSITION(sbufp, Currentbuf);
}

// void saveBufferInfo(struct UI ui)
// {
//     FILE* fp;
//     if ((fp = fopen(rcFile("bufinfo"), "w")) == NULL) {
//         return;
//     }
//     fprintf(fp, "%s\n", currentURL(ui)->ptr);
//     fclose(fp);
// }

void pushBuffer(struct UI ui, struct Buffer* buf)
{
    deleteImage(ui.current_buffer);
    if (clear_buffer)
        clearBuffer(ui.current_buffer);

    struct Buffer* b;
    if (Firstbuf == ui.current_buffer) {
        buf->nextBuffer = Firstbuf;
        Firstbuf = ui.current_buffer = buf;
    } else if ((b = prevBuffer(Firstbuf, ui.current_buffer)) != NULL) {
        b->nextBuffer = buf;
        buf->nextBuffer = ui.current_buffer;
        ui.current_buffer = buf;
    }
}

void cmd_loadContent(struct UI ui, struct Content c)
{
    // struct Buffer* buf;
    // if ((buf = ui.current_buffer->linkBuffer[LB_N_INFO]) != NULL) {
    //     ui.current_buffer = buf;
    //
    //     return;
    // }
    // if ((buf = ui.current_buffer->linkBuffer[LB_INFO]) != NULL)
    //     delBuffer(ui, buf);

    struct Buffer *buf = makeBuffer(ui, &c);
    if (!buf) {
        message(getUI(), MSG_ERR, "Can't load string");
        return;
    }
    // buf->bufferprop |= (BP_INTERNAL | prop);
    // if (!(buf->bufferprop & BP_NO_URL))
    //     buf->currentURL = copyParsedUrl(&ui.current_buffer->currentURL);
    pushBuffer(ui, buf);
}

struct Buffer* pushContent(struct UI ui, struct Content c)
{
    struct Buffer* buf = makeBuffer(ui, &c);
    if (!buf) {
        Str emsg = Sprintf("Can't load %s", parsedURL2Str(&c.url)->ptr);
        message(getUI(), MSG_ERR, emsg->ptr);
        return 0;
    }
    // struct Buffer* buf = makeBuffer(ui, &c);
    // if (buf == NULL) {
    //     message(getUI(), MSG_INFO, "Execution failed");
    //     return;
    // } else if (buf) {
    //     buf->filename = w;
    //     buf->buffername = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
    //     if (buf->content_type == CONTENTTYPE_UNKNOWN)
    //         buf->content_type = CONTENTTYPE_TEXT_PLAIN;
    //     pushBuffer(ui, buf);
    // }    // if (do_download) {
    //     // TODO
    //     abort();
    // }
    // struct Buffer* buf = makeBuffer(ui, &c);
    // if (buf == NULL) {
    //     /* FIXME: gettextize? */
    //     char* emsg = Sprintf("Can't load %s", a->url)->ptr;
    //     message(ui, MSG_ERR, emsg);
    // } else if (buf) {
    //     pushBuffer(ui, buf);
    // }    // struct Buffer* buf = makeBuffer(ui, &c);
    // if (buf == NULL) {
    //     /* FIXME: gettextize? */
    //     char* emsg = Sprintf("%s not found", conv_from_system(fn))->ptr;
    //     message(getUI(), MSG_ERR, emsg);
    // } else if (buf) {
    //     pushBuffer(ui, buf);
    // }    // struct Buffer* buf = makeBuffer(ui, &c);
    // if (buf == NULL) {
    //     char* emsg = Sprintf("Can't load %s", url)->ptr;
    //     message(ui, MSG_ERR, emsg);
    //     return NULL;
    // }
    //
    // struct Url pu = parseUrl(url, base);
    // pushHashHist(URLHist, parsedURL2Str(&pu)->ptr);
    //
    // if (buf == NULL) {
    //     return NULL;
    // }
    //
    // if (do_download) /* download (thus no need to render frames) */
    //     return loadNormalBuf(ui, buf);
    //
    // if (target == NULL || /* no target specified (that means this page is not a frame page) */
    //     !strcmp(target, "_top") /* this link is specified to be opened as an indivisual * page */
    // ) {
    //     return loadNormalBuf(ui, buf);
    // }
    //
    // return loadNormalBuf(ui, buf);

    pushBuffer(ui, buf);
    return buf;
}

void cmd_loadfile(struct UI ui, const char* fn)
{
    struct Content c = loadGeneralFile(file_to_url(fn, CurrentDir), NULL, NULL, NO_REFERER, UI_TTY);
    pushContent(ui, c);
}
