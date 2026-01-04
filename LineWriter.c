#include "LineWriter.h"
#include "screen.h"
#include "w3m_rc.h"
#include "symbol.h"
#include "ctrlcode.h"
#include <libwc/status.h>
#include <libwc/wtf_width.h>

#define EFFECT_ANCHOR_START effect_anchor_start()
#define EFFECT_ANCHOR_END effect_anchor_end()
#define EFFECT_IMAGE_START effect_image_start()
#define EFFECT_IMAGE_END effect_image_end()
#define EFFECT_FORM_START effect_form_start()
#define EFFECT_FORM_END effect_form_end()
#define EFFECT_ACTIVE_START effect_active_start()
#define EFFECT_ACTIVE_END effect_active_end()
#define EFFECT_VISITED_START effect_visited_start()
#define EFFECT_VISITED_END effect_visited_end()
#define EFFECT_MARK_START effect_mark_start()
#define EFFECT_MARK_END effect_mark_end()

/*-
 * color:
 *     0  black
 *     1  red
 *     2  green
 *     3  yellow
 *     4  blue
 *     5  magenta
 *     6  cyan
 *     7  white
 */

#define EFFECT_ANCHOR_START_C screen_setfcolor(getRuntime()->anchor_color)
#define EFFECT_IMAGE_START_C screen_setfcolor(getRuntime()->image_color)
#define EFFECT_FORM_START_C screen_setfcolor(getRuntime()->form_color)
#define EFFECT_ACTIVE_START_C (screen_setfcolor(getRuntime()->active_color), screen_underline())
#define EFFECT_VISITED_START_C screen_setfcolor(getRuntime()->visited_color)
#define EFFECT_MARK_START_C screen_setbcolor(getRuntime()->mark_color)

#define EFFECT_IMAGE_END_C screen_setfcolor(getRuntime()->basic_color)
#define EFFECT_ANCHOR_END_C screen_setfcolor(getRuntime()->basic_color)
#define EFFECT_FORM_END_C screen_setfcolor(getRuntime()->basic_color)
#define EFFECT_ACTIVE_END_C (screen_setfcolor(getRuntime()->basic_color), screen_underlineend())
#define EFFECT_VISITED_END_C screen_setfcolor(getRuntime()->basic_color)
#define EFFECT_MARK_END_C screen_setbcolor(getRuntime()->bg_color)

#define EFFECT_ANCHOR_START_M screen_underline()
#define EFFECT_ANCHOR_END_M screen_underlineend()
#define EFFECT_IMAGE_START_M screen_standout()
#define EFFECT_IMAGE_END_M screen_standend()
#define EFFECT_FORM_START_M screen_standout()
#define EFFECT_FORM_END_M screen_standend()
#define EFFECT_ACTIVE_START_NC screen_underline()
#define EFFECT_ACTIVE_END_NC screen_underlineend()
#define EFFECT_ACTIVE_START_M screen_bold()
#define EFFECT_ACTIVE_END_M screen_boldend()
#define EFFECT_VISITED_START_M /**/
#define EFFECT_VISITED_END_M /**/
#define EFFECT_MARK_START_M screen_standout()
#define EFFECT_MARK_END_M screen_standend()
#define define_effect(name_start, name_end, color_start, color_end, mono_start, mono_end) \
    static void name_start                                                                \
    {                                                                                     \
        if (getRuntime()->useColor) {                                                     \
            color_start;                                                                  \
        } else {                                                                          \
            mono_start;                                                                   \
        }                                                                                 \
    }                                                                                     \
    static void name_end                                                                  \
    {                                                                                     \
        if (getRuntime()->useColor) {                                                     \
            color_end;                                                                    \
        } else {                                                                          \
            mono_end;                                                                     \
        }                                                                                 \
    }

define_effect(EFFECT_ANCHOR_START, EFFECT_ANCHOR_END, EFFECT_ANCHOR_START_C,
    EFFECT_ANCHOR_END_C, EFFECT_ANCHOR_START_M, EFFECT_ANCHOR_END_M)
    define_effect(EFFECT_IMAGE_START, EFFECT_IMAGE_END, EFFECT_IMAGE_START_C,
        EFFECT_IMAGE_END_C, EFFECT_IMAGE_START_M, EFFECT_IMAGE_END_M)
        define_effect(EFFECT_FORM_START, EFFECT_FORM_END, EFFECT_FORM_START_C,
            EFFECT_FORM_END_C, EFFECT_FORM_START_M, EFFECT_FORM_END_M)
            define_effect(EFFECT_MARK_START, EFFECT_MARK_END, EFFECT_MARK_START_C,
                EFFECT_MARK_END_C, EFFECT_MARK_START_M, EFFECT_MARK_END_M)

    /*****************/
    static void EFFECT_ACTIVE_START
{
    if (getRuntime()->useColor) {
        if (getRuntime()->useActiveColor) {
            EFFECT_ACTIVE_START_C;
        } else {
            EFFECT_ACTIVE_START_NC;
        }
    } else {
        EFFECT_ACTIVE_START_M;
    }
}

static void EFFECT_ACTIVE_END
{
    if (getRuntime()->useColor) {
        if (getRuntime()->useActiveColor) {
            EFFECT_ACTIVE_END_C;
        } else {
            EFFECT_ACTIVE_END_NC;
        }
    } else {
        EFFECT_ACTIVE_END_M;
    }
}

static void EFFECT_VISITED_START
{
    if (getRuntime()->useVisitedColor) {
        if (getRuntime()->useColor) {
            EFFECT_VISITED_START_C;
        } else {
            EFFECT_VISITED_START_M;
        }
    }
}

static void EFFECT_VISITED_END
{
    if (getRuntime()->useVisitedColor) {
        if (getRuntime()->useColor) {
            EFFECT_VISITED_END_C;
        } else {
            EFFECT_VISITED_END_M;
        }
    }
}

#define do_effect1(effect, modeflag, action_start, action_end) \
    if (m & effect) {                                          \
        if (!modeflag) {                                       \
            action_start;                                      \
            modeflag = true;                                   \
        }                                                      \
    }

#define do_effect2(effect, modeflag, action_start, action_end) \
    if (modeflag) {                                            \
        action_end;                                            \
        modeflag = false;                                      \
    }

static void
do_effects(struct LineWriter* this, Lineprop m)
{
    /* effect end */
    do_effect2(PE_UNDER, this->ulmode, screen_underline(), screen_underlineend());
    do_effect2(PE_STAND, this->somode, screen_standout(), screen_standend());
    do_effect2(PE_BOLD, this->bomode, screen_bold(), screen_boldend());
    do_effect2(PE_EMPH, this->emph_mode, screen_bold(), screen_boldend());
    do_effect2(PE_ANCHOR, this->anch_mode, EFFECT_ANCHOR_START, EFFECT_ANCHOR_END);
    do_effect2(PE_IMAGE, this->imag_mode, EFFECT_IMAGE_START, EFFECT_IMAGE_END);
    do_effect2(PE_FORM, this->form_mode, EFFECT_FORM_START, EFFECT_FORM_END);
    do_effect2(PE_VISITED, this->visited_mode, EFFECT_VISITED_START,
        EFFECT_VISITED_END);
    do_effect2(PE_ACTIVE, this->active_mode, EFFECT_ACTIVE_START, EFFECT_ACTIVE_END);
    do_effect2(PE_MARK, this->mark_mode, EFFECT_MARK_START, EFFECT_MARK_END);
    if (this->graph_mode) {
        screen_graphend();
        this->graph_mode = false;
    }

    /* effect start */
    do_effect1(PE_UNDER, this->ulmode, screen_underline(), screen_underlineend());
    do_effect1(PE_STAND, this->somode, screen_standout(), screen_standend());
    do_effect1(PE_BOLD, this->bomode, screen_bold(), screen_boldend());
    do_effect1(PE_EMPH, this->emph_mode, screen_bold(), screen_boldend());
    do_effect1(PE_ANCHOR, this->anch_mode, EFFECT_ANCHOR_START, EFFECT_ANCHOR_END);
    do_effect1(PE_IMAGE, this->imag_mode, EFFECT_IMAGE_START, EFFECT_IMAGE_END);
    do_effect1(PE_FORM, this->form_mode, EFFECT_FORM_START, EFFECT_FORM_END);
    do_effect1(PE_VISITED, this->visited_mode, EFFECT_VISITED_START,
        EFFECT_VISITED_END);
    do_effect1(PE_ACTIVE, this->active_mode, EFFECT_ACTIVE_START, EFFECT_ACTIVE_END);
    do_effect1(PE_MARK, this->mark_mode, EFFECT_MARK_START, EFFECT_MARK_END);
}

void do_color(struct LineWriter* this, Linecolor c)
{
    if (c & 0x8)
        screen_setfcolor(c & 0x7);
    else if (this->color_mode & 0x8)
        screen_setfcolor(getRuntime()->basic_color);
    if (c & 0x80)
        screen_setbcolor((c >> 4) & 0x7);
    else if (this->color_mode & 0x80)
        screen_setbcolor(getRuntime()->bg_color);
    this->color_mode = c;
}

void beginLine()
{
    if (getRuntime()->useColor) {
        EFFECT_ANCHOR_END_C;
        screen_setbcolor(getRuntime()->bg_color);
    }
}

void endLine(struct LineWriter* this)
{
    if (this->somode) {
        this->somode = false;
        screen_standend();
    }
    if (this->ulmode) {
        this->ulmode = false;
        screen_underlineend();
    }
    if (this->bomode) {
        this->bomode = false;
        screen_boldend();
    }

    if (this->emph_mode) {
        this->emph_mode = false;
        screen_boldend();
    }

    if (this->anch_mode) {
        this->anch_mode = false;
        EFFECT_ANCHOR_END;
    }
    if (this->imag_mode) {
        this->imag_mode = false;
        EFFECT_IMAGE_END;
    }
    if (this->form_mode) {
        this->form_mode = false;
        EFFECT_FORM_END;
    }
    if (this->visited_mode) {
        this->visited_mode = false;
        EFFECT_VISITED_END;
    }
    if (this->active_mode) {
        this->active_mode = false;
        EFFECT_ACTIVE_END;
    }
    if (this->mark_mode) {
        this->mark_mode = false;
        EFFECT_MARK_END;
    }
    if (this->graph_mode) {
        this->graph_mode = false;
        screen_graphend();
    }
    if (this->color_mode) {
        do_color(this, 0);
    }
}

void addMChar(struct LineWriter* this, char* p, Lineprop mode, size_t len)
{
    Lineprop m = CharEffect(mode);

    char c = *p;

    if (mode & PC_WCHAR2)
        return;

    do_effects(this, m);
    if (mode & PC_SYMBOL) {
        char** symbol;

        int w = (mode & PC_KANJI) ? 2 : 1;

        c = ((char)wtf_get_code((wc_uchar*)p) & 0x7f) - SYMBOL_BASE;
        if (graph_ok() && c < N_GRAPH_SYMBOL) {
            if (!this->graph_mode) {
                screen_graphstart();
                this->graph_mode = true;
            }

            if (w == 2 && WcOption.use_wide)
                screen_wc_addstr(graph2_symbol[(unsigned char)c % N_GRAPH_SYMBOL]);
            else
                screen_addch(*graph_symbol[(unsigned char)c % N_GRAPH_SYMBOL], 1);
        } else {
            symbol = get_symbol(getRuntime()->DisplayCharset, &w);
            screen_wc_addstr(symbol[(unsigned char)c % N_SYMBOL]);
        }
    } else if (mode & PC_CTRL) {
        switch (c) {
        case '\t':
            screen_add_tab();
            break;
        case '\n':
            screen_addch(' ', 1);
            break;
        case '\r':
            break;
        case DEL_CODE:
            screen_wc_addstr("^?");
            break;
        default:
            screen_addch('^', 1);
            screen_addch(c + '@', 1);
            break;
        }
    }

    else if (mode & PC_UNKNOWN) {
        char buf[5];
        sprintf(buf, "[%.2X]",
            (unsigned char)wtf_get_code((wc_uchar*)p) | 0x80);
        screen_wc_addstr(buf);
    } else {
        screen_addmch(p, len, wtf_width(p));
    }
}

void addChar(struct LineWriter* this, char c, Lineprop mode)
{
    addMChar(this, &c, mode, 1);
}

void addStr(struct LineWriter* this, char* p, Lineprop* pr, int len, int offset, int limit)
{
    int i = 0, rcol = 0, ncol, delta = 1;

    if (offset) {
        for (i = 0; i < len; i++) {
            if (calcPosition(p, pr, len, i, 0, CP_AUTO) > offset)
                break;
        }
        if (i >= len)
            return;
        while (pr[i] & PC_WCHAR2)
            i++;
        addChar(this, '{', 0);
        rcol = offset + 1;
        ncol = calcPosition(p, pr, len, i, 0, CP_AUTO);
        for (; rcol < ncol; rcol++)
            addChar(this, ' ', 0);
    }
    for (; i < len; i += delta) {
        delta = wtf_len((wc_uchar*)&p[i]);
        ncol = calcPosition(p, pr, len, i + delta, 0, CP_AUTO);
        if (ncol - offset > limit)
            break;
        if (p[i] == '\t') {
            for (; rcol < ncol; rcol++)
                addChar(this, ' ', 0);
            continue;
        } else {
            addMChar(this, &p[i], pr[i], delta);
        }
        rcol = ncol;
    }
}

void addPasswd(struct LineWriter* this, char* p, Lineprop* pr, int len, int offset, int limit)
{
    int ncol = calcPosition(p, pr, len, len, 0, CP_AUTO);
    if (ncol > offset + limit)
        ncol = offset + limit;

    int rcol = 0;
    if (offset) {
        addChar(this, '{', 0);
        rcol = offset + 1;
    }
    for (; rcol < ncol; rcol++)
        addChar(this, '*', 0);
}
