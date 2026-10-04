#include "backend.h"
#include "w3m.h"
#include "tab.h"
#include "charset.h"
#include "indep.h"
#include "config.h"
#include "cookie.h"
#include "download.h"
#include "proto.h"
#include "rc.h"
#include "terms.h"
#include "http_request.h"
#include "libwc/charset.h"

#include <gc/gc.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

int w3m_backend = false;
struct TextLineList* backend_halfdump_buf;
struct TextList* backend_batch_commands = NULL;

/* Prototype declaration of internal functions */
#ifdef HAVE_READLINE
#include <readline/readline.h>
#else /* ! HAVE_READLINE */
static char* readline(char*);
#endif /* ! HAVE_READLINE */
static struct TextList* split(const char*);

/* Prototype declaration of command functions */
static void get(struct TextList*);
static void post(struct TextList*);
static void set(struct TextList*);
static void show(struct TextList*);
static void quit(struct TextList*);
static void help(struct TextList*);

/* *INDENT-OFF* */
/* Table of command functions */
struct {
    const char* name;
    const char* option_string;
    const char* help;
    void (*func)(struct TextList*);
} command_table[] = {
    { "get", "[-download_only] URL", "Retrieve URL.", get },
    { "post", "[-download_only] [-target TARGET] [-charset CHARSET]"
              " [-enctype ENCTYPE] [-body BODY] [-boundary BOUNDARY] [-length LEN] URL",
        "Retrieve URL.", post },
    { "set", "VARIABLE VALUE", "Set VALUE to VARIABLE.", set },
    { "show", "VARIABLE", "Show value of VARIABLE.", show },
    { "quit", "", "Quit program.", quit },
    { "help", "", "Display help messages.", help },
    { NULL, NULL, NULL, NULL },
};
/* *INDENT-ON* */

/* Prototype declaration of functions to manipulate configuration variables */
static void set_column(struct TextList*);
static void show_column(struct TextList*);

/* *INDENT-OFF* */
/* Table of configuration variables */
struct {
    const char* name;
    void (*set_func)(struct TextList*);
    void (*show_func)(struct TextList*);
} variable_table[] = {
    { "column", set_column, show_column },
    { NULL, NULL, NULL },
};
/* *INDENT-ON* */

static void
print_headers(Buffer* buf, int len)
{
    struct TextListItem* tp;

    if (buf->document_header) {
        for (tp = buf->document_header->first; tp; tp = tp->next)
            printf("%s\n", tp->ptr);
    }
    printf("w3m-current-url: %s\n", parsedURL2Str(&buf->currentURL)->ptr);
    if (buf->baseURL)
        printf("w3m-base-url: %s\n", parsedURL2Str(buf->baseURL)->ptr);
    printf("w3m-content-type: %s\n", buf->type);
    if (buf->document_charset)
        printf("w3m-content-charset: %s\n",
            wc_ces_to_charset(buf->document_charset));
    if (len > 0)
        printf("w3m-content-length: %d\n", len);
}

static void
internal_get(const char* url, int flag, FormList* request)
{
    Buffer* buf;

    backend_halfdump_buf = NULL;
    do_download = flag;
    buf = loadGeneralFile(url, NULL, NO_REFERER, 0, request);
    do_download = false;
    if (buf != NULL && buf != NO_BUFFER) {
        if (is_html_type(buf->type) && backend_halfdump_buf) {
            struct TextLineListItem* p;
            pStr first, last;
            int len = 0;
            for (p = backend_halfdump_buf->first; p; p = p->next) {
                p->ptr->line = Str_conv_to_halfdump(&WcOption, p->ptr->line);
                len += p->ptr->line->len + 1;
            }
            first = Strnew_charp("<pre>\n");
            last = Strnew_m_charp("</pre><title>", html_quote(buf->buffername),
                "</title>\n", NULL);
            print_headers(buf, len + first->len + last->len);
            printf("\n");
            printf("%s", first->ptr);
            for (p = backend_halfdump_buf->first; p; p = p->next)
                printf("%s\n", p->ptr->line->ptr);
            printf("%s", last->ptr);
        } else {
            if (!strcasecmp(buf->type, "text/plain")) {
                Line* lp;
                int len = 0;
                for (lp = buf->firstLine; lp; lp = lp->next) {
                    len += lp->len;
                    if (lp->lineBuf[lp->len - 1] != '\n')
                        len++;
                }
                print_headers(buf, len);
                printf("\n");
                saveBuffer(buf, stdout, true);
            } else {
                print_headers(buf, 0);
            }
        }
    }
}

/* Command: get */
static void
get(struct TextList* argv)
{
    const char *p, *url = NULL;
    int flag = false;
    while ((p = TextList_unshift(argv))) {
        if (!strcasecmp(p, "-download_only"))
            flag = true;
        else
            url = p;
    }
    if (url) {
        internal_get(url, flag, NULL);
    }
}

/* Command: post */
static void
post(struct TextList* argv)
{
    FormList* request;
    const char *p, *target = NULL, *charset = NULL,
                   *enctype = NULL, *body = NULL, *boundary = NULL, *url = NULL;
    int flag = false, length = 0;

    while ((p = TextList_unshift(argv))) {
        if (!strcasecmp(p, "-download_only"))
            flag = true;
        else if (!strcasecmp(p, "-target"))
            target = TextList_unshift(argv);
        else if (!strcasecmp(p, "-charset"))
            charset = TextList_unshift(argv);
        else if (!strcasecmp(p, "-enctype"))
            enctype = TextList_unshift(argv);
        else if (!strcasecmp(p, "-body"))
            body = TextList_unshift(argv);
        else if (!strcasecmp(p, "-boundary"))
            boundary = TextList_unshift(argv);
        else if (!strcasecmp(p, "-length"))
            length = atol(TextList_unshift(argv));
        else
            url = p;
    }
    if (url) {
        request = newFormList(NULL, "post", charset, enctype, target, NULL, NULL);
        request->body = body;
        request->boundary = boundary;
        request->length = (length > 0) ? length : (body ? strlen(body) : 0);
        internal_get(url, flag, request);
    }
}

/* Command: set */
static void
set(struct TextList* argv)
{
    if (argv->nitem > 1) {
        int i;
        for (i = 0; variable_table[i].name; i++) {
            if (!strcasecmp(variable_table[i].name, argv->first->ptr)) {
                TextList_unshift(argv);
                if (variable_table[i].set_func)
                    variable_table[i].set_func(argv);
                break;
            }
        }
    }
}

/* Command: show */
static void
show(struct TextList* argv)
{
    if (argv->nitem >= 1) {
        int i;
        for (i = 0; variable_table[i].name; i++) {
            if (!strcasecmp(variable_table[i].name, argv->first->ptr)) {
                TextList_unshift(argv);
                if (variable_table[i].show_func)
                    variable_table[i].show_func(argv);
                break;
            }
        }
    }
}

/* Command: quit */
static void
quit(struct TextList* argv)
{
    save_cookies();
    w3m_exit(0);
}

/* Command: help */
static void
help(struct TextList* argv)
{
    int i;
    for (i = 0; command_table[i].name; i++)
        printf("%s %s\n    %s\n",
            command_table[i].name,
            command_table[i].option_string, command_table[i].help);
}

/* Sub command: set COLS */
static void
set_column(struct TextList* argv)
{
    if (argv->nitem == 1) {
        COLS = atol(argv->first->ptr);
    }
}

/* Sub command: show COLS */
static void
show_column(struct TextList* argv)
{
    fprintf(stdout, "column=%d\n", COLS);
}

/* Call appropriate command function based on given string */
static void
call_command_function(const char* str)
{
    int i;
    struct TextList* argv = split(str);
    if (argv->nitem > 0) {
        for (i = 0; command_table[i].name; i++) {
            if (!strcasecmp(command_table[i].name, argv->first->ptr)) {
                TextList_unshift(argv);
                if (command_table[i].func)
                    command_table[i].func(argv);
                break;
            }
        }
    }
}

/* Main function */
int backend(void)
{
    w3m_dump = 0;
    if (COLS == 0)
        COLS = DEFAULT_COLS;

    const char* str;
    if (backend_batch_commands) {
        while ((str = TextList_unshift(backend_batch_commands)))
            call_command_function(str);
    } else {
        while ((str = readline("w3m> ")))
            call_command_function(str);
    }
    quit(NULL);
    return 0;
}

/* Dummy function of readline(). */
#ifndef HAVE_READLINE
static char*
readline(char* prompt)
{
    pStr s;
    fputs(prompt, stdout);
    fflush(stdout);
    s = Strfgets(stdin);
    if (feof(stdin) && (strlen(s->ptr) == 0))
        return NULL;
    else
        return s->ptr;
}
#endif /* ! HAVE_READLINE */

/* Splits a string into a list of tokens and returns that list. */
static struct TextList*
split(const char* p)
{
    int in_double_quote = false, in_single_quote = false;
    pStr s = Strnew();
    struct TextList* tp = TextList_new();

    for (; *p; p++) {
        switch (*p) {
        case '"':
            if (in_single_quote)
                Strcat_char(s, '"');
            else
                in_double_quote = !in_double_quote;
            break;
        case '\'':
            if (in_double_quote)
                Strcat_char(s, '\'');
            else
                in_single_quote = !in_single_quote;
            break;
        case '\\':
            if (!in_single_quote) {
                /* Process escape characters. */
                p++;
                switch (*p) {
                case 't':
                    Strcat_char(s, '\t');
                    break;
                case 'r':
                    Strcat_char(s, '\r');
                    break;
                case 'f':
                    Strcat_char(s, '\f');
                    break;
                case 'n':
                    Strcat_char(s, '\n');
                    break;
                case '\0':
                    goto LAST;
                default:
                    Strcat_char(s, *p);
                }
            } else {
                Strcat_char(s, *p);
            }
            break;
        case ' ':
        case '\t':
        case '\r':
        case '\f':
        case '\n':
            /* Separators are detected. */
            if (in_double_quote || in_single_quote) {
                Strcat_char(s, *p);
            } else if (s->len > 0) {
                TextList_push(tp, s->ptr);
                s = Strnew();
            }
            break;
        default:
            Strcat_char(s, *p);
        }
    }
LAST:
    if (s->len > 0)
        TextList_push(tp, s->ptr);
    return tp;
}
