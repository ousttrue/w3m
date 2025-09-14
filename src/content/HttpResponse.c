#include "HttpResponse.h"
#include "convertline.h"
#include "mimehead.h"
#include "myctype.h"
#include <stdlib.h>
#include <wc.h>

struct HttpResponse readHttpResponse(struct Url* pu, union input_stream* stream)
{
    struct HttpResponse response = {
        .headers = newTextList(),
        .status_code = 0,
    };
    if (pu->scheme == SCM_HTTP || pu->scheme == SCM_HTTPS) {
        response.status_code = -1;
    }

    wc_ces charset = WC_CES_US_ASCII;
    Str lineBuf2 = NULL;
    Str tmp;
    while ((tmp = StrmyISgets(stream)) && tmp->length) {
        cleanup_line(tmp);
        if (tmp->ptr[0] == '\n' || tmp->ptr[0] == '\r' || tmp->ptr[0] == '\0') {
            if (!lineBuf2)
                /* there is no header */
                break;
            /* last header */
        } else {
            if (lineBuf2) {
                Strcat(lineBuf2, tmp);
            } else {
                lineBuf2 = tmp;
            }
            int c = ISgetc(stream);
            ISundogetc(stream);
            if (c == ' ' || c == '\t')
                /* header line is continued */
                continue;
            wc_ces mime_charset;
            lineBuf2 = decodeMIME(lineBuf2, &mime_charset);
            lineBuf2 = convertLine(lineBuf2, RAW_MODE,
                mime_charset ? &mime_charset : &charset,
                mime_charset ? mime_charset : WC_CES_UTF_8,
                WC_CES_WTF);
            /* separated with line and stored */
            tmp = Strnew_size(lineBuf2->length);
            char* q;
            for (char* p = lineBuf2->ptr; *p; p = q) {
                for (q = p; *q && *q != '\r' && *q != '\n'; q++)
                    ;
                // Lineprop* propBuffer;
                // lineBuf2 = checkType(Strnew_charp_n(p, q - p), &propBuffer, NULL);
                lineBuf2 = Strnew_charp_n(p, q - p);
                Strcat(tmp, lineBuf2);
                for (; *q && (*q == '\r' || *q == '\n'); q++)
                    ;
            }
            lineBuf2 = tmp;
        }

        if ((pu->scheme == SCM_HTTP
                || pu->scheme == SCM_HTTPS)
            && response.status_code == -1) {
            char* p = lineBuf2->ptr;
            while (*p && !IS_SPACE(*p))
                p++;
            while (*p && IS_SPACE(*p))
                p++;
            response.status_code = atoi(p);

            // message(getUI(), MSG_INFO, lineBuf2->ptr);
            // refresh(ttyWriter());
        }

        pushText(response.headers, lineBuf2->ptr);
        Strfree(lineBuf2);
        lineBuf2 = NULL;
    }

    return response;
}
