#include "buffer_list.h"
#include "tab.h"
#include "buffer.h"
#include "message.h"
#include "screen.h"
#include "w3m_rc.h"
#include "ctrlcode.h"
#include <string.h>

static void
writeBufferName(struct Buffer* buf, int n)
{
    int all = buf->doc->allLine;
    if (all == 0 && buf->doc->lastLine != NULL)
        all = buf->doc->lastLine->linenumber;
    screen_move((struct Vec2) { .y = n, .x = 0 });
    Str msg = Sprintf("<%s> [%d lines]", buf->doc->title, all);
    if (buf->content->filename != NULL) {
        switch (buf->content->url.scheme) {
        case SCM_LOCAL:
        case SCM_LOCAL_CGI:
            if (strcmp(buf->content->url.file, "-")) {
                Strcat_char(msg, ' ');
                Strcat_charp(msg, conv_from_system(buf->content->url.real_file));
            }
            break;
        case SCM_UNKNOWN:
        case SCM_MISSING:
            break;
        default:
            Strcat_char(msg, ' ');
            Strcat(msg, parsedURL2Str(&buf->content->url));
            break;
        }
    }
    screen_wc_addnstr_sup(msg->ptr, TTY_COLS() - 1);
}

static struct Buffer*
listBuffer(struct Buffer* top, struct Buffer* current)
{
    int i, c = 0;
    struct Buffer* buf = top;

    screen_move((struct Vec2) { 0 });
    if (getRuntime()->useColor) {
        screen_setfcolor(getRuntime()->basic_color);
        screen_setbcolor(getRuntime()->bg_color);
    }
    screen_clrtobotx();
    for (i = 0; i < LASTLINE(); i++) {
        if (buf == current) {
            c = i;
            screen_standout();
        }
        writeBufferName(buf, i);
        if (buf == current) {
            screen_standend();
            screen_clrtoeolx();
            screen_move((struct Vec2) { .y = i, .x = 0 });
            screen_toggle_stand();
        } else
            screen_clrtoeolx();
        if (buf->back == NULL) {
            screen_move((struct Vec2) { .y = i + 1, .x = 0 });
            screen_clrtobotx();
            break;
        }
        buf = buf->back;
    }
    screen_standout();
    message("Buffer selection mode: SPC for select / D for delete buffer");
    screen_standend();
    /*
     * move(LASTLINE(), COLS - 1); */
    screen_move((struct Vec2) { .y = c, .x = 0 });
    tty_write_screen();
    return buf->back;
}

/*
 * Select buffer visually
 */
struct Buffer*
tab_selectBuffer(struct TabBuffer* tab, struct Buffer* currentbuf, char* selectchar)
{
    int i = 0;
    int cpoint = 0;
    for (struct Buffer* buf = tab->firstBuffer; buf != NULL; buf = buf->back) {
        if (buf == currentbuf)
            cpoint = i;
        i++;
    }
    int maxbuf = i;

    int sclimit = LASTLINE();
    int spoint;
    struct Buffer* topbuf;
    if (cpoint >= sclimit) {
        spoint = sclimit / 2;
        topbuf = tab_nthBuffer(tab, cpoint - spoint);
    } else {
        topbuf = tab->firstBuffer;
        spoint = cpoint;
    }
    listBuffer(topbuf, currentbuf);

    for (;;) {
        char c;
        if ((c = getch()) == ESC_CODE) {
            if ((c = getch()) == '[' || c == 'O') {
                switch (c = getch()) {
                case 'A':
                    c = 'k';
                    break;
                case 'B':
                    c = 'j';
                    break;
                case 'C':
                    c = ' ';
                    break;
                case 'D':
                    c = 'B';
                    break;
                }
            }
        }
        switch (c) {
        case CTRL_N:
        case 'j':
            if (spoint < sclimit - 1) {
                if (currentbuf->back == NULL)
                    continue;
                writeBufferName(currentbuf, spoint);
                currentbuf = currentbuf->back;
                cpoint++;
                spoint++;
                screen_standout();
                writeBufferName(currentbuf, spoint);
                screen_standend();
                screen_move((struct Vec2) { .y = spoint, .x = 0 });
                screen_toggle_stand();
            } else if (cpoint < maxbuf - 1) {
                topbuf = currentbuf;
                currentbuf = currentbuf->back;
                cpoint++;
                spoint = 1;
                listBuffer(topbuf, currentbuf);
            }
            break;
        case CTRL_P:
        case 'k':
            if (spoint > 0) {
                writeBufferName(currentbuf, spoint);
                currentbuf = tab_nthBuffer(tab, --spoint);
                cpoint--;
                screen_standout();
                writeBufferName(currentbuf, spoint);
                screen_standend();
                screen_move((struct Vec2) { .y = spoint, .x = 0 });
                screen_toggle_stand();
            } else if (cpoint > 0) {
                i = cpoint - sclimit;
                if (i < 0)
                    i = 0;
                cpoint--;
                spoint = cpoint - i;
                currentbuf = tab_nthBuffer(tab, cpoint);
                topbuf = tab_nthBuffer(tab, i);
                listBuffer(topbuf, currentbuf);
            }
            break;
        default:
            *selectchar = c;
            return currentbuf;
        }
        /*
         * move(LASTLINE(), COLS - 1);
         */
        screen_move((struct Vec2) { .y = spoint, .x = 0 });
    }
}
