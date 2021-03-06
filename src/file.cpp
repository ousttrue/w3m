#include <unistd.h>
#include "textlist.h"
#include "frontend/terminal.h"
#include "frontend/screen.h"
#include "html/table.h"
#include "indep.h"
#include "commands.h"
#include "html/html_context.h"
#include "file.h"
#include "frontend/anchor.h"
#include "public.h"
#include "symbol.h"
#include "html/maparea.h"
#include "frontend/display.h"
#include "myctype.h"
#include "html/html.h"
#include "html/html.h"
#include "stream/local_cgi.h"
#include "regex.h"
#include "command_dispatcher.h"
#include "stream/url.h"
#include "entity.h"
#include "stream/cookie.h"
#include "stream/network.h"
#include "html/image.h"
#include "ctrlcode.h"
#include "mime/mimeencoding.h"
#include "mime/mimetypes.h"
#include "html/tagstack.h"
#include "stream/http.h"

#include "stream/auth.h"
#include "stream/compression.h"
#include "rc.h"
#include "loader.h"

#include "wtf.h"
#include "stream/input_stream.h"
#include "mime/mailcap.h"
#include "frontend/lineinput.h"
#include <assert.h>
#include <sys/types.h>
#include <signal.h>
#include <setjmp.h>
#if defined(HAVE_WAITPID) || defined(HAVE_WAIT3)
#include <sys/wait.h>
#endif
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
/* foo */

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif /* not max */
#ifndef min
#define min(a, b) ((a) > (b) ? (b) : (a))
#endif /* not min */

static JMP_BUF AbortLoading;
static void KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
}

int setModtime(char *path, time_t modtime)
{
    struct utimbuf t;
    struct stat st;

    if (stat(path, &st) == 0)
        t.actime = st.st_atime;
    else
        t.actime = time(NULL);
    t.modtime = modtime;
    return utime(path, &t);
}

/*
 * convert line
 */
Str convertLine(URLSchemeTypes scheme, Str line, int mode, CharacterEncodingScheme *charset,
                CharacterEncodingScheme doc_charset)
{

    line = wc_Str_conv_with_detect(line, charset, doc_charset, w3mApp::Instance().InnerCharset);

    if (mode != RAW_MODE)
        cleanup_line(line, mode);

    if (scheme == SCM_NEWS)
        StripRight(line);

    return line;
}

int matchattr(const char *p, const char *attr, int len, Str *value)
{
    int quoted;
    const char *q = NULL;

    if (strncasecmp(p, attr, len) == 0)
    {
        p += len;
        SKIP_BLANKS(&p);
        if (value)
        {
            *value = Strnew();
            if (*p == '=')
            {
                p++;
                SKIP_BLANKS(&p);
                quoted = 0;
                while (!IS_ENDL(*p) && (quoted || *p != ';'))
                {
                    if (!IS_SPACE(*p))
                        q = p;
                    if (*p == '"')
                        quoted = (quoted) ? 0 : 1;
                    else
                        (*value)->Push(*p);
                    p++;
                }
                if (q)
                    (*value)->Pop(p - q - 1);
            }
            return 1;
        }
        else
        {
            if (IS_ENDT(*p))
            {
                return 1;
            }
        }
    }
    return 0;
}

// char *checkHeader(BufferPtr buf, const char *field)
// {
//     int len;
//     TextListItem *i;
//     char *p;

//     if (buf == NULL || field == NULL || buf->document_header == NULL)
//         return NULL;
//     len = strlen(field);
//     for (i = buf->document_header->first; i != NULL; i = i->next)
//     {
//         if (!strncasecmp(i->ptr, field, len))
//         {
//             p = i->ptr + len;
//             return remove_space(p);
//         }
//     }
//     return NULL;
// }

static int
same_url_p(URL *pu1, URL *pu2)
{
    return (pu1->scheme == pu2->scheme && pu1->port == pu2->port &&
            (pu1->host.size() ? pu2->host.size() ? pu1->host == pu2->host : 0 : 1) &&
            (pu1->path.size() ? pu2->path.size() ? pu1->path == pu2->path : 0 : 1));
}

static int
is_period_char(unsigned char *ch)
{
    switch (*ch)
    {
    case ',':
    case '.':
    case ':':
    case ';':
    case '?':
    case '!':
    case ')':
    case ']':
    case '}':
    case '>':
        return 1;
    default:
        return 0;
    }
}

static int
is_beginning_char(unsigned char *ch)
{
    switch (*ch)
    {
    case '(':
    case '[':
    case '{':
    case '`':
    case '<':
        return 1;
    default:
        return 0;
    }
}

static int
is_word_char(unsigned char *ch)
{
    Lineprop ctype = get_mctype(*ch);

#ifdef USE_M17N
    if (ctype & (PC_CTRL | PC_KANJI | PC_UNKNOWN))
        return 0;
    if (ctype & (PC_WCHAR1 | PC_WCHAR2))
        return 1;
#else
    if (ctype == PC_CTRL)
        return 0;
#endif

    if (IS_ALNUM(*ch))
        return 1;

    switch (*ch)
    {
    case ',':
    case '.':
    case ':':
    case '\"': /* " */
    case '\'':
    case '$':
    case '%':
    case '*':
    case '+':
    case '-':
    case '@':
    case '~':
    case '_':
        return 1;
    }
#ifdef USE_M17N
    if (*ch == NBSP_CODE)
        return 1;
#else
    if (*ch == TIMES_CODE || *ch == DIVIDE_CODE || *ch == ANSP_CODE)
        return 0;
    if (*ch >= AGRAVE_CODE || *ch == NBSP_CODE)
        return 1;
#endif
    return 0;
}

#ifdef USE_M17N
static int
is_combining_char(unsigned char *ch)
{
    Lineprop ctype = get_mctype(*ch);

    if (ctype & PC_WCHAR2)
        return 1;
    return 0;
}
#endif

int is_boundary(unsigned char *ch1, unsigned char *ch2)
{
    if (!*ch1 || !*ch2)
        return 1;

    if (*ch1 == ' ' && *ch2 == ' ')
        return 0;

    if (*ch1 != ' ' && is_period_char(ch2))
        return 0;

    if (*ch2 != ' ' && is_beginning_char(ch1))
        return 0;

#ifdef USE_M17N
    if (is_combining_char(ch2))
        return 0;
#endif
    if (is_word_char(ch1) && is_word_char(ch2))
        return 0;

    return 1;
}

int getMetaRefreshParam(const char *q, Str *refresh_uri)
{
    if (q == NULL || refresh_uri == NULL)
        return 0;

    int refresh_interval = atoi(q);
    if (refresh_interval < 0)
        return 0;

    Str s_tmp = NULL;
    while (*q)
    {
        if (!strncasecmp(q, "url=", 4))
        {
            q += 4;
            if (*q == '\"') /* " */
                q++;
            auto r = q;
            while (*r && !IS_SPACE(*r) && *r != ';')
                r++;
            s_tmp = Strnew_charp_n(q, r - q);

            if (s_tmp->ptr[s_tmp->Size() - 1] == '\"')
            { /* "
                                                                 */
                s_tmp->Pop(1);
                s_tmp->ptr[s_tmp->Size()] = '\0';
            }
            q = r;
        }
        while (*q && *q != ';')
            q++;
        if (*q == ';')
            q++;
        while (*q && *q == ' ')
            q++;
    }
    *refresh_uri = s_tmp;
    return refresh_interval;
}

extern char *NullLine;
extern Lineprop NullProp[];

static const char *_size_unit[] = {"b", "kb", "Mb", "Gb", "Tb",
                                   "Pb", "Eb", "Zb", "Bb", "Yb", NULL};

char *
convert_size(clen_t size, int usefloat)
{
    float csize;
    int sizepos = 0;
    const char **sizes = _size_unit;

    csize = (float)size;
    while (csize >= 999.495 && sizes[sizepos + 1])
    {
        csize = csize / 1024.0;
        sizepos++;
    }
    return Sprintf(usefloat ? (char *)"%.3g%s" : (char *)"%.0f%s",
                   floor(csize * 100.0 + 0.5) / 100.0, sizes[sizepos])
        ->ptr;
}

char *
convert_size2(clen_t size1, clen_t size2, int usefloat)
{
    const char **sizes = _size_unit;
    float csize, factor = 1;
    int sizepos = 0;

    csize = (float)((size1 > size2) ? size1 : size2);
    while (csize / factor >= 999.495 && sizes[sizepos + 1])
    {
        factor *= 1024.0;
        sizepos++;
    }
    return Sprintf(usefloat ? (char *)"%.3g/%.3g%s" : (char *)"%.0f/%.0f%s",
                   floor(size1 / factor * 100.0 + 0.5) / 100.0,
                   floor(size2 / factor * 100.0 + 0.5) / 100.0,
                   sizes[sizepos])
        ->ptr;
}

void showProgress(clen_t *linelen, clen_t *trbyte, long long content_length)
{
    int i, j, rate, duration, eta, pos;
    static time_t last_time, start_time;
    time_t cur_time;
    Str messages;
    char *fmtrbyte, *fmrate;

    if (!w3mApp::Instance().fmInitialized)
        return;

    if (*linelen < 1024)
        return;

    if (content_length > 0)
    {
        double ratio;
        cur_time = time(0);
        if (*trbyte == 0)
        {
            Screen::Instance().LineCol((Terminal::lines() - 1), 0);
            Screen::Instance().CtrlToEolWithBGColor();
            start_time = cur_time;
        }
        *trbyte += *linelen;
        *linelen = 0;
        if (cur_time == last_time)
            return;
        last_time = cur_time;
        Screen::Instance().LineCol((Terminal::lines() - 1), 0);
        ratio = 100.0 * (*trbyte) / content_length;
        fmtrbyte = convert_size2(*trbyte, content_length, 1);
        duration = cur_time - start_time;
        if (duration)
        {
            rate = *trbyte / duration;
            fmrate = convert_size(rate, 1);
            eta = rate ? (content_length - *trbyte) / rate : -1;
            messages = Sprintf("%11s %3.0f%% "
                               "%7s/s "
                               "eta %02d:%02d:%02d     ",
                               fmtrbyte, ratio,
                               fmrate,
                               eta / (60 * 60), (eta / 60) % 60, eta % 60);
        }
        else
        {
            messages = Sprintf("%11s %3.0f%%                          ",
                               fmtrbyte, ratio);
        }
        Screen::Instance().Puts(messages->ptr);
        pos = 42;
        i = pos + (Terminal::columns() - pos - 1) * (*trbyte) / content_length;
        Screen::Instance().LineCol((Terminal::lines() - 1), pos);
        Screen::Instance().Enable(S_STANDOUT);

        Screen::Instance().PutAscii(' ');
        for (j = pos + 1; j <= i; j++)
            Screen::Instance().PutAscii('|');
        Screen::Instance().Disable(S_STANDOUT);
        /* no_clrtoeol(); */
        Screen::Instance().Refresh();
        Terminal::flush();
    }
    else
    {
        cur_time = time(0);
        if (*trbyte == 0)
        {
            Screen::Instance().LineCol((Terminal::lines() - 1), 0);
            Screen::Instance().CtrlToEolWithBGColor();
            start_time = cur_time;
        }
        *trbyte += *linelen;
        *linelen = 0;
        if (cur_time == last_time)
            return;
        last_time = cur_time;
        Screen::Instance().LineCol((Terminal::lines() - 1), 0);
        fmtrbyte = convert_size(*trbyte, 1);
        duration = cur_time - start_time;
        if (duration)
        {
            fmrate = convert_size(*trbyte / duration, 1);
            messages = Sprintf("%7s loaded %7s/s", fmtrbyte, fmrate);
        }
        else
        {
            messages = Sprintf("%7s loaded", fmtrbyte);
        }
        message(messages->ptr);
        Screen::Instance().Refresh();
        Terminal::flush();
    }
}

#ifdef USE_GOPHER

/*
 * loadGopherDir: get gopher directory
 */
Str loadGopherDir(const URLFilePtr &uf, URL *pu, CharacterEncodingScheme *charset)
{
    Str tmp;
    Str lbuf, name, file, host, port;
    char *p, *q;
    MySignalHandler prevtrap = NULL;
#ifdef USE_M17N
    CharacterEncodingScheme doc_charset = DocumentCharset;
#endif

    tmp = pu->ToStr();
    p = html_quote(tmp->ptr);
    tmp =
        convertLine(NULL, Strnew(file_unquote(tmp->ptr)), RAW_MODE,
                    charset, doc_charset);
    q = html_quote(tmp->ptr);
    tmp = Strnew_m_charp("<html>\n<head>\n<base href=\"", p, "\">\n<title>", q,
                         "</title>\n</head>\n<body>\n<h1>Index of ", q,
                         "</h1>\n<table>\n", NULL);

    if (SETJMP(AbortLoading) != 0)
        goto gopher_end;
    TRAP_ON;

    while (1)
    {
        if (lbuf = StrUFgets(uf), lbuf->length == 0)
            break;
        if (lbuf->ptr[0] == '.' &&
            (lbuf->ptr[1] == '\n' || lbuf->ptr[1] == '\r'))
            break;
        lbuf = convertLine(uf, lbuf, HTML_MODE, charset, doc_charset);
        p = lbuf->ptr;
        for (q = p; *q && *q != '\t'; q++)
            ;
        name = Strnew_charp_n(p, q - p);
        if (!*q)
            continue;
        p = q + 1;
        for (q = p; *q && *q != '\t'; q++)
            ;
        file = Strnew_charp_n(p, q - p);
        if (!*q)
            continue;
        p = q + 1;
        for (q = p; *q && *q != '\t'; q++)
            ;
        host = Strnew_charp_n(p, q - p);
        if (!*q)
            continue;
        p = q + 1;
        for (q = p; *q && *q != '\t' && *q != '\r' && *q != '\n'; q++)
            ;
        port = Strnew_charp_n(p, q - p);

        switch (name->ptr[0])
        {
        case '0':
            p = "[text file]";
            break;
        case '1':
            p = "[directory]";
            break;
        case 'm':
            p = "[message]";
            break;
        case 's':
            p = "[sound]";
            break;
        case 'g':
            p = "[gif]";
            break;
        case 'h':
            p = "[HTML]";
            break;
        default:
            p = "[unsupported]";
            break;
        }
        q = Strnew_m_charp("gopher://", host->ptr, ":", port->ptr,
                           "/", file->ptr, NULL)
                ->ptr;
        Strcat_m_charp(tmp, "<a href=\"",
                       html_quote(wc_conv_strict(q, *charset)),
                       "\">", p, html_quote(name->ptr + 1), "</a>\n", NULL);
    }

gopher_end:
    TRAP_OFF;

    tmp->Push("</table>\n</body>\n</html>\n");
    return tmp;
}
#endif /* USE_GOPHER */

BufferPtr loadImageBuffer(const URL &url, const InputStreamPtr &stream)
{
    auto newBuf = Buffer::Create(url);
    ImageManager::Instance().loadImage(newBuf, IMG_FLAG_STOP);

    Image image;
    image.url = url.ToStr()->ptr;
    // image.ext = uf->ext;
    image.width = -1;
    image.height = -1;
    image.cache = NULL;

    // TODO:
    auto cache = getImage(&image, nullptr, IMG_FLAG_AUTO);
    struct stat st;
    // if (!GetCurBaseUrl()->is_nocache && cache->loaded & IMG_FLAG_LOADED &&
    //     !stat(cache->file, &st))
    //     goto image_buffer;

    // TRAP_ON;
    // if (stream->type() != IST_ENCODED)
    //     stream = newEncodedStream(stream, uf->encoding);
    // if (save2tmp(uf, cache->file) < 0)
    // {
    //     TRAP_OFF;
    //     return NULL;
    // }
    // TRAP_OFF;

    cache->loaded = IMG_FLAG_LOADED;
    cache->index = 0;

// image_buffer:
    // if (newBuf == NULL)
    //     newBuf = Buffer::Create(INIT_BUFFER_WIDTH());
    // cache->loaded |= IMG_FLAG_DONT_REMOVE;
    // if (newBuf->sourcefile.empty() && uf->scheme != SCM_LOCAL)
    //     newBuf->sourcefile = cache->file;

    auto tmp = Sprintf("<img src=\"%s\"><br><br>", html_quote(image.url));
    auto tmpf = tmpfname(TMPF_SRC, ".html");
    auto src = fopen(tmpf->ptr, "w");
    newBuf->mailcap_source = tmpf->ptr;

    // auto f = URLFile::FromStream(SCM_LOCAL, );
    newBuf = loadHTMLStream(url, StrStream::Create(tmp->ptr), WC_CES_UTF_8, true);
    if (src)
        fclose(src);

    newBuf->CurrentAsLast();

    newBuf->image_flag = IMG_FLAG_AUTO;
    return newBuf;
}

/*
 * saveBuffer: write buffer to file
 */
static void
_saveBuffer(BufferPtr buf, LinePtr l, FILE *f, int cont)
{
    int set_charset = !w3mApp::Instance().DisplayCharset;
    CharacterEncodingScheme charset = w3mApp::Instance().DisplayCharset ? w3mApp::Instance().DisplayCharset : WC_CES_US_ASCII;

    auto is_html = is_html_type(buf->type);

    Str tmp;
pager_next:
    for (; l != NULL; l = buf->m_document->NextLine(l))
    {
        if (is_html)
            tmp = l->buffer.conv_symbol();
        else
            tmp = Strnew_charp_n(l->buffer.lineBuf(), l->buffer.ByteLength());
        tmp = wc_Str_conv(tmp, w3mApp::Instance().InnerCharset, charset);
        tmp->Puts(f);
        // if (tmp->Back() != '\n' && !(cont && buf->m_document->NextLine(l) && buf->m_document->NextLine(l)->bpos))
        putc('\n', f);
    }

    // if (buf->pagerSource && !(buf->bufferprop & BP_CLOSE))
    // {
    //     l = getNextPage(buf, w3mApp::Instance().PagerMax);
    //     if (set_charset)
    //         charset = buf->m_document->document_charset;

    //     goto pager_next;
    // }
}

void saveBuffer(BufferPtr buf, FILE *f, int cont)
{
    _saveBuffer(buf, buf->m_document->FirstLine(), f, cont);
}

void saveBufferBody(BufferPtr buf, FILE *f, int cont)
{
    LinePtr l = buf->m_document->FirstLine();

    while (l != NULL && l->real_linenumber == 0)
        l = buf->m_document->NextLine(l);
    _saveBuffer(buf, l, f, cont);
}

/*
 * getshell: execute shell command and get the result into a buffer
 */
#define SHELLBUFFERNAME "*Shellout*"
BufferPtr
getshell(char *cmd)
{
    auto buf = loadcmdout(cmd, loadBuffer);
    if (buf == NULL)
        return NULL;
    buf->filename = cmd;
    buf->buffername = Sprintf("%s %s", SHELLBUFFERNAME,
                              conv_from_system(cmd))
                          ->ptr;
    return buf;
}

#define PIPEBUFFERNAME "*stream*"
/*
 * getpipe: execute shell command and connect pipe to the buffer
 */
BufferPtr
getpipe(char *cmd)
{
    // if (cmd == NULL || *cmd == '\0')
        return NULL;

//     FILE *popen(const char *, const char *);
//     auto f = popen(cmd, "r");
//     if (f == NULL)
//         return NULL;

//     auto buf = Buffer::Create({});
//     buf->pagerSource = newFileStream(f, pclose);
//     buf->filename = cmd;
//     buf->buffername = Sprintf("%s %s", PIPEBUFFERNAME,
//                               conv_from_system(cmd))
//                           ->ptr;
//     buf->bufferprop |= BP_PIPE;
// #ifdef USE_M17N
//     buf->m_document->document_charset = WC_CES_US_ASCII;
// #endif
//     return buf;
}

/*
 * Open pager buffer
 */
BufferPtr openPagerBuffer(const InputStreamPtr &stream, CharacterEncodingScheme content_charset)
{
    return nullptr;
    // auto buf = Buffer::Create({});
    // buf->pagerSource = stream;
    // buf->buffername = getenv("MAN_PN");
    // if (buf->buffername.empty())
    //     buf->buffername = PIPEBUFFERNAME;
    // else
    //     buf->buffername = conv_from_system(buf->buffername.c_str());
    // buf->bufferprop |= BP_PIPE;

    // if (content_charset && w3mApp::Instance().UseContentCharset)
    //     buf->m_document->document_charset = content_charset;
    // else
    //     buf->m_document->document_charset = WC_CES_US_ASCII;

    // buf->SetCurrentLine(buf->m_document->FirstLine());

    // return buf;
}

BufferPtr openGeneralPagerBuffer(const InputStreamPtr &stream, CharacterEncodingScheme content_charset)
{
    BufferPtr buf;
    std::string t = "text/plain";
    // BufferPtr t_buf = NULL;
    // auto uf = URLFile::FromStream(SCM_UNKNOWN, stream);

    // if (w3mApp::Instance().SearchHeader)
    // {
    //     t_buf = Buffer::Create(INIT_BUFFER_WIDTH());
    //     readHeader(uf, t_buf, true, NULL);
    //     t = checkContentType(t_buf);
    //     if (t.empty())
    //         t = "text/plain";
    //     if (t_buf)
    //     {
    //         t_buf->SetTopLine(t_buf->FirstLine());
    //         t_buf->SetCurrentLine(t_buf->LastLine());
    //     }
    //     w3mApp::Instance().SearchHeader = false;
    // }
    // else if (w3mApp::Instance().DefaultType.size())
    // {
    //     t = w3mApp::Instance().DefaultType;
    //     w3mApp::Instance().DefaultType.clear();
    // }

    if (is_html_type(t))
    {
        buf = loadHTMLStream({}, stream, content_charset, true);
    }
    else if (is_plain_text_type(t.c_str()))
    {
        // if (stream->type() != IST_ENCODED)
        //     stream = newEncodedStream(stream, uf->encoding);
        buf = openPagerBuffer(stream, content_charset);
        buf->type = "text/plain";
    }
    else if (ImageManager::Instance().activeImage && ImageManager::Instance().displayImage && !ImageManager::Instance().useExtImageViewer &&
             t.starts_with("image/"))
    {
        // *GetCurBaseUrl() = URL::Parse("-", NULL);
        buf = loadImageBuffer({}, stream);
        buf->type = "text/html";
    }
    else
    {
        buf = doExternal(URL::StdIn(), stream, t.c_str());
        if (buf)
        {
            return buf;
        }
    }

    /* unknown type is regarded as text/plain */
    // if (stream->type() != IST_ENCODED)
    //     stream = newEncodedStream(stream, uf->encoding);
    buf = openPagerBuffer(stream, content_charset);
    buf->type = "text/plain";
    buf->real_type = t;
    buf->url = URL::StdIn();
    return buf;
}

#define CPIPEBUFFERNAME "*stream(closed)*"
LinePtr getNextPage(BufferPtr buf, int plen)
{
    LinePtr top = buf->TopLine();
    LinePtr last = buf->m_document->LastLine();
    LinePtr cur = buf->CurrentLine();
    int i;
    int nlines = 0;
    clen_t linelen = 0;
    Str lineBuf2;
    char pre_lbuf = '\0';

    CharacterEncodingScheme charset;
    CharacterEncodingScheme doc_charset = w3mApp::Instance().DocumentCharset;
    uint8_t old_auto_detect = WcOption.auto_detect;

    int squeeze_flag = false;

    MySignalHandler prevtrap = NULL;

    // if (buf->pagerSource == NULL)
        return NULL;

    // if (last != NULL)
    // {
    //     nlines = last->real_linenumber;
    //     pre_lbuf = *(last->lineBuf());
    //     if (pre_lbuf == '\0')
    //         pre_lbuf = '\n';
    //     buf->SetCurrentLine(last);
    // }

    // charset = buf->m_document->document_charset;
    // if (buf->m_document->document_charset != WC_CES_US_ASCII)
    //     doc_charset = buf->m_document->document_charset;
    // else if (w3mApp::Instance().UseContentCharset)
    // {
    //     // content_charset = WC_CES_NONE;
    //     // checkContentType(buf);
    //     // if (content_charset)
    //     //     doc_charset = content_charset;
    // }
    // WcOption.auto_detect = buf->auto_detect;

    // auto success = TrapJmp([&]() {
    //     for (i = 0; i < plen; i++)
    //     {
    //         lineBuf2 = buf->pagerSource->mygets();
    //         if (lineBuf2->Size() == 0)
    //         {
    //             /* Assume that `cmd == buf->filename' */
    //             if (buf->filename.size())
    //                 buf->buffername = Sprintf("%s %s",
    //                                           CPIPEBUFFERNAME,
    //                                           conv_from_system(buf->filename))
    //                                       ->ptr;
    //             else if (getenv("MAN_PN") == NULL)
    //                 buf->buffername = CPIPEBUFFERNAME;
    //             buf->bufferprop |= BP_CLOSE;
    //             break;
    //         }
    //         linelen += lineBuf2->Size();
    //         // showProgress(&linelen, &trbyte, 0);
    //         lineBuf2 = convertLine(SCM_UNKNOWN, lineBuf2, PAGER_MODE, &charset, doc_charset);
    //         if (w3mApp::Instance().squeezeBlankLine)
    //         {
    //             squeeze_flag = false;
    //             if (lineBuf2->ptr[0] == '\n' && pre_lbuf == '\n')
    //             {
    //                 ++nlines;
    //                 --i;
    //                 squeeze_flag = true;
    //                 continue;
    //             }
    //             pre_lbuf = lineBuf2->ptr[0];
    //         }
    //         ++nlines;
    //         StripRight(lineBuf2);

    //         buf->AddNewLine(PropertiedString(lineBuf2), nlines);
    //         if (!top)
    //         {
    //             top = buf->m_document->FirstLine();
    //             cur = top;
    //         }
    //         if (buf->m_document->LastLine()->real_linenumber - buf->m_document->FirstLine()->real_linenumber >= w3mApp::Instance().PagerMax)
    //         {
    //             LinePtr l = buf->m_document->FirstLine();
    //             do
    //             {
    //                 if (top == l)
    //                     top = buf->m_document->NextLine(l);
    //                 if (cur == l)
    //                     cur = buf->m_document->NextLine(l);
    //                 if (last == l)
    //                     last = NULL;
    //                 l = buf->m_document->NextLine(l);
    //             } while (l && l->bpos);
    //             buf->SetFirstLine(l);
    //             buf->SetPrevLine(buf->m_document->FirstLine(), NULL);
    //         }
    //     }

    //     return true;
    // });

    // if (!success)
    // {
    //     return nullptr;
    // }

    // buf->m_document->document_charset = charset;
    // WcOption.auto_detect = (AutoDetectTypes)old_auto_detect;

    // buf->SetTopLine(top);
    // buf->SetCurrentLine(cur);
    // if (!last)
    //     last = buf->m_document->FirstLine();
    // else if (last && (buf->NextLine(last) || !squeeze_flag))
    //     last = buf->NextLine(last);
    // return last;
}

int checkSaveFile(const InputStreamPtr &stream, char *path2)
{
    struct stat st1, st2;
    int des = stream->FD();

    if (des < 0)
        return 0;
    if (*path2 == '|' && w3mApp::Instance().PermitSaveToPipe)
        return 0;
    if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return -1;
    return 0;
}

int checkOverWrite(const char *path)
{
    struct stat st;
    if (stat(path, &st) < 0)
        return 0;

    /* FIXME: gettextize? */
    auto ans = inputAnswer("File exists. Overwrite? (y/n)");
    if (ans && TOLOWER(*ans) == 'y')
        return 0;
    else
        return -1;
}

const char *inputAnswer(const char *prompt)
{
    if (w3mApp::Instance().QuietMessage)
        return "n";

    if (w3mApp::Instance().fmInitialized)
    {
        Terminal::term_raw();
        return inputChar(prompt, 1);
    }

    printf("%s", prompt);
    fflush(stdout);
    return Strfgets(stdin)->ptr;
}

const char *guess_filename(std::string_view file)
{
    if (file.empty())
    {
        return DEF_SAVE_FILE;
    }

    auto p = mybasename(file.data());
    auto s = p;
    if (*p == '#')
        p++;
    while (*p != '\0')
    {
        if ((*p == '#' && *(p + 1) != '\0') || *p == '?')
        {
            *p = '\0';
            break;
        }
        p++;
    }
    return s;
}

// char *
// guess_save_name(BufferPtr buf, std::string_view path)
// {
//     if (buf && buf->document_header)
//     {
//         Str name = NULL;
//         char *p, *q;
//         if ((p = checkHeader(buf, "Content-Disposition:")) != NULL &&
//             (q = strcasestr(p, "filename")) != NULL &&
//             (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
//             matchattr(q, "filename", 8, &name))
//             path = name->ptr;
//         else if ((p = checkHeader(buf, "Content-Type:")) != NULL &&
//                  (q = strcasestr(p, "name")) != NULL &&
//                  (q == p || IS_SPACE(*(q - 1)) || *(q - 1) == ';') &&
//                  matchattr(q, "name", 4, &name))
//             path = name->ptr;
//     }
//     return guess_filename(path);
// }

static const char *tmpf_base[MAX_TMPF_TYPE] = {
    "tmp",
    "src",
    "frame",
    "cache",
    "cookie",
};
static unsigned int tmpf_seq[MAX_TMPF_TYPE];

Str tmpfname(TmpFileTypes type, const char *ext)
{
    Str tmpf;
    tmpf = Sprintf("%s/w3m%s%d-%d%s",
                   w3mApp::Instance().tmp_dir.c_str(),
                   tmpf_base[type],
                   w3mApp::Instance().CurrentPid, tmpf_seq[type]++, (ext) ? ext : "");
    w3mApp::Instance().fileToDelete.push_back(tmpf->ptr);
    return tmpf;
}

Str myEditor(const char *cmd, const char *file, int line)
{
    Str tmp = NULL;
    int set_file = false, set_line = false;

    for (auto p = cmd; *p; p++)
    {
        if (*p == '%' && *(p + 1) == 's' && !set_file)
        {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            tmp->Push(file);
            set_file = true;
            p++;
        }
        else if (*p == '%' && *(p + 1) == 'd' && !set_line && line > 0)
        {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            tmp->Push(Sprintf("%d", line));
            set_line = true;
            p++;
        }
        else
        {
            if (tmp)
                tmp->Push(*p);
        }
    }
    if (!set_file)
    {
        if (tmp == NULL)
            tmp = Strnew(cmd);
        if (!set_line && line > 1 && strcasestr(cmd, "vi"))
            tmp->Push(Sprintf(" +%d", line));
        Strcat_m_charp(tmp, " ", file, NULL);
    }
    return tmp;
}

Str myExtCommand(const char *cmd, const char *arg, int redirect)
{
    Str tmp = NULL;
    int set_arg = false;

    for (auto p = cmd; *p; p++)
    {
        if (*p == '%' && *(p + 1) == 's' && !set_arg)
        {
            if (tmp == NULL)
                tmp = Strnew_charp_n(cmd, (int)(p - cmd));
            tmp->Push(arg);
            set_arg = true;
            p++;
        }
        else
        {
            if (tmp)
                tmp->Push(*p);
        }
    }
    if (!set_arg)
    {
        if (redirect)
            tmp = Strnew_m_charp("(", cmd, ") < ", arg, NULL);
        else
            tmp = Strnew_m_charp(cmd, " ", arg, NULL);
    }
    return tmp;
}

void mySystem(char *command, int background)
{
#ifndef __MINGW32_VERSION
    if (background)
    {
#ifndef __EMX__
        Terminal::flush();
        if (!fork())
        {
            setup_child(false, 0, -1);
            myExec(command);
        }
#else
        Str cmd = Strnew("start /f ");
        cmd->Push(command);
        system(cmd->ptr);
#endif
    }
    else
#endif /* __MINGW32_VERSION */
        system(command);
}

void myExec(const char *command)
{
    mySignal(SIGINT, SIG_DFL);
    execl("/bin/sh", "sh", "-c", command, NULL);
    exit(127);
}

const char *mydirname(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    if (s != p)
        p--;
    while (s != p && *p == '/')
        p--;
    while (s != p && *p != '/')
        p--;
    if (*p != '/')
        return ".";
    while (s != p && *p == '/')
        p--;
    return allocStr(s, strlen(s) - strlen(p) + 1);
}

/* get last modified time */
const char *last_modified(const BufferPtr &buf)
{
    TextListItem *ti;
    struct stat st;

    // if (buf->document_header)
    // {
    //     for (ti = buf->document_header->first; ti; ti = ti->next)
    //     {
    //         if (strncasecmp(ti->ptr, "Last-modified: ", 15) == 0)
    //         {
    //             return ti->ptr + 15;
    //         }
    //     }
    //     return "unknown";
    // }
    // else
    if (buf->url.scheme == SCM_LOCAL)
    {
        if (stat(buf->url.path.c_str(), &st) < 0)
            return "unknown";
        return ctime(&st.st_mtime);
    }
    return "unknown";
}

/* FIXME: gettextize? */
#define FILE_IS_READABLE_MSG "SECURITY NOTE: file %s must not be accessible by others"

FILE *openSecretFile(const char *fname)
{
    char *efname;
    struct stat st;

    if (fname == NULL)
        return NULL;
    efname = expandPath(fname);
    if (stat(efname, &st) < 0)
        return NULL;

    /* check permissions, if group or others readable or writable,
     * refuse it, because it's insecure.
     *
     * XXX: disable_secret_security_check will introduce some
     *    security issues, but on some platform such as Windows
     *    it's not possible (or feasible) to disable group|other
     *    readable and writable.
     *   [w3m-dev 03368][w3m-dev 03369][w3m-dev 03370]
     */
    if (Network::Instance().disable_secret_security_check)
        /* do nothing */;
    else if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0)
    {
        if (w3mApp::Instance().fmInitialized)
        {
            message(Sprintf(FILE_IS_READABLE_MSG, fname)->ptr);
            Screen::Instance().Refresh();
            Terminal::flush();
        }
        else
        {
            fputs(Sprintf(FILE_IS_READABLE_MSG, fname)->ptr, stderr);
            fputc('\n', stderr);
        }
        sleep(2);
        return NULL;
    }

    return fopen(efname, "r");
}

#ifdef USE_INCLUDED_SRAND48
static unsigned long R1 = 0x1234abcd;
static unsigned long R2 = 0x330e;
#define A1 0x5deec
#define A2 0xe66d
#define C 0xb

void srand48(long seed)
{
    R1 = (unsigned long)seed;
    R2 = 0x330e;
}

long lrand48(void)
{
    R1 = (A1 * R1 << 16) + A1 * R2 + A2 * R1 + ((A2 * R2 + C) >> 16);
    R2 = (A2 * R2 + C) & 0xffff;
    return (long)(R1 >> 1);
}
#endif

char *lastFileName(const char *path)
{
    auto p = path;
    auto q = path;
    while (*p != '\0')
    {
        if (*p == '/')
            q = p + 1;
        p++;
    }

    return allocStr(q, -1);
}
