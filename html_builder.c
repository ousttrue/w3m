#include "html_builder.h"
#include "html_form.h"
#include "html_table.h"
#include "html.h"
#include "content.h"
#include "document.h"
#include "line.h"
#include "image.h"
#include "input_stream.h"
#include "readbuffer.h"
#include "w3m_rc.h"
#include "symbol.h"
#include "indep.h"
#include "alloc.h"
#include <libwc/status.h>

Str process_title(struct HtmlBuilder *hb, struct HtmlTag* tag)
{
    if (hb->pre_title)
        return NULL;
    hb->cur_title = Strnew();
    return NULL;
}

void feed_title(struct HtmlBuilder *hb, const char* str)
{
    if (hb->pre_title)
        return;
    if (!hb->cur_title)
        return;
    while (*str) {
        if (*str == '&')
            Strcat_charp(hb->cur_title, getescapecmd(&str));
        else if (*str == '\n' || *str == '\r') {
            Strcat_char(hb->cur_title, ' ');
            str++;
        } else
            Strcat_char(hb->cur_title, *(str++));
    }
}

Str process_n_title(struct HtmlBuilder *hb, struct HtmlTag* tag)
{
    if (hb->pre_title)
        return NULL;
    if (!hb->cur_title)
        return NULL;
    Strremovefirstspaces(hb->cur_title);
    Strremovetrailingspaces(hb->cur_title);
    Str tmp = Strnew_m_charp("<title_alt title=\"",
        html_quote(hb->cur_title->ptr), "\">", NULL);
    hb->pre_title = hb->cur_title;
    hb->cur_title = NULL;
    return tmp;
}

struct Document* loadHTMLstream(int width,
    struct Url* base_url, struct Content* content, struct input_stream* stream, bool internal)
{
    struct HtmlBuilder _hb = {
        .max_textarea = MAX_TEXTAREA,
        .textarea_str = New_N(Str, MAX_TEXTAREA),
        .n_select = 0,
        .max_select = MAX_SELECT,
        .select_option = New_N(struct FormSelectOption, MAX_SELECT),
        .form_sp = -1,
        .form_max = -1,
        .forms_size = 0,
        .forms = NULL,
        .cur_hseq = 1,
        .cur_iseq = 1,
        .cur_baseURL = base_url,
        .meta_charset = 0,
    };
    struct HtmlBuilder* hb = &_hb;

    if (fmInitialized() && graph_ok()) {
        symbol_width = symbol_width0 = 1;
    } else {
        symbol_width0 = 0;
        get_symbol(getRuntime()->DisplayCharset, &symbol_width0);
        symbol_width = WcOption.use_wide ? symbol_width0 : 1;
    }

    enum ImageGetFlags image_flag = IMG_FLAG_SKIP;
    // if (newBuf->doc.image_flag)
    //     image_flag = newBuf->doc.image_flag;
    // else
    if (getRuntime()->activeImage && getRuntime()->displayImage && getRuntime()->autoImage)
        image_flag = IMG_FLAG_AUTO;

    struct html_feed_environ htmlenv1;
    struct environment envs[MAX_ENV_LEVEL];
    struct readbuffer obuf;
    init_henv(&htmlenv1, &obuf, envs, MAX_ENV_LEVEL, NULL, width, 0);
    htmlenv1.buf = newTextLineList();

    struct Document* doc = New(struct Document);
    enum wc_ces doc_charset = getRuntime()->DocumentCharset;
    enum wc_ces detected_charset = WC_CES_US_ASCII;
    int64_t linelen = 0;
    int64_t trbyte = 0;
    // if (newBuf) {
    //     if (newBuf->bufferprop & BP_FRAME)
    //         detected_charset = getRuntime()->InnerCharset;
    //     else if (newBuf->doc.charset)
    //         detected_charset = doc_charset = newBuf->doc.charset;
    // }
    if (content && content->charset && getRuntime()->UseContentCharset)
        doc_charset = content->charset;

    Str lineBuf2 = NULL;
    while ((lineBuf2 = is_get_str(stream, true)) && lineBuf2->length) {

        linelen += lineBuf2->length;
        if (content) {
            showProgress(&linelen, &trbyte, content->current_content_length);
        }
        /*
         * if (frame_source)
         * continue;
         */

        if (hb->meta_charset) { /* <META> */
            if (content->charset == 0 && getRuntime()->UseContentCharset) {
                doc_charset = hb->meta_charset;
                detected_charset = WC_CES_US_ASCII;
            }
            hb->meta_charset = 0;
        }

        lineBuf2 = convertLine(lineBuf2, HTML_MODE, &detected_charset, doc_charset);

        hb->cur_document_charset = detected_charset;

        HTMLlineproc0(hb, lineBuf2->ptr, &htmlenv1, internal);
    }
    if (obuf.status != R_ST_NORMAL) {
        HTMLlineproc0(hb, "\n", &htmlenv1, internal);
    }
    obuf.status = R_ST_NORMAL;
    completeHTMLstream(hb, &htmlenv1, &obuf);
    flushline(&htmlenv1, &obuf, 0, 2, htmlenv1.limit);

    if (htmlenv1.title)
        doc->title = htmlenv1.title;

    doc->trbyte = trbyte + linelen;
    doc->charset = detected_charset;
    doc->image_flag = image_flag;

    HTMLlineproc2(hb, base_url, doc, htmlenv1.buf);

    doc->topLine = doc->firstLine;
    doc->lastLine = doc->currentLine;
    doc->currentLine = doc->firstLine;
    return doc;
}


