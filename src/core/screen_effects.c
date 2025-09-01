#include "screen_effects.h"
#include "line_prop.h"
#include "screen.h"
#include <stdbool.h>

int useColor = (true);
int basic_color = (8); /* don't change */
int anchor_color = (4); /* blue  */
int image_color = (2); /* green */
int form_color = (1); /* red   */
int bg_color = (8); /* don't change */
int mark_color = (6); /* cyan */
int useActiveColor = (false);
int active_color = (6); /* cyan */
int useVisitedColor = (false);
int visited_color = (5); /* magenta  */

static int ulmode = 0, somode = 0, bomode = 0;
static int anch_mode = 0, emph_mode = 0, imag_mode = 0, form_mode = 0,
           active_mode = 0, visited_mode = 0, mark_mode = 0;
int graph_mode = 0;
static Linecolor color_mode = 0;

void vt_do_color(struct VirtualTerm* vt, Linecolor c)
{
    if (c & 0x8)
        vt_setfcolor(vt, c & 0x7);
    else if (color_mode & 0x8)
        vt_setfcolor(vt, basic_color);
    if (c & 0x80)
        vt_setbcolor(vt, (c >> 4) & 0x7);
    else if (color_mode & 0x80)
        vt_setbcolor(vt, bg_color);
    color_mode = c;
}

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

static void EFFECT_ANCHOR_START_C(struct VirtualTerm* vt) { vt_setfcolor(vt, anchor_color); }
static void EFFECT_IMAGE_START_C(struct VirtualTerm* vt) { vt_setfcolor(vt, image_color); }
static void EFFECT_FORM_START_C(struct VirtualTerm* vt) { vt_setfcolor(vt, form_color); }
static void EFFECT_ACTIVE_START_C(struct VirtualTerm* vt) { (vt_setfcolor(vt, active_color), vt_underline(vt)); }
static void EFFECT_VISITED_START_C(struct VirtualTerm* vt) { vt_setfcolor(vt, visited_color); }
static void EFFECT_MARK_START_C(struct VirtualTerm* vt) { vt_setbcolor(vt, mark_color); }

static void EFFECT_IMAGE_END_C(struct VirtualTerm* vt) { vt_setfcolor(vt, basic_color); }
void EFFECT_ANCHOR_END_C(struct VirtualTerm* vt) { vt_setfcolor(vt, basic_color); }
static void EFFECT_FORM_END_C(struct VirtualTerm* vt) { vt_setfcolor(vt, basic_color); }
static void EFFECT_ACTIVE_END_C(struct VirtualTerm* vt) { (vt_setfcolor(vt, basic_color), vt_underlineend(vt)); }
static void EFFECT_VISITED_END_C(struct VirtualTerm* vt) { vt_setfcolor(vt, basic_color); }
static void EFFECT_MARK_END_C(struct VirtualTerm* vt) { vt_setbcolor(vt, bg_color); }

static void EFFECT_ANCHOR_START_M(struct VirtualTerm* vt) { vt_underline(vt); }
static void EFFECT_ANCHOR_END_M(struct VirtualTerm* vt) { vt_underlineend(vt); }
static void EFFECT_IMAGE_START_M(struct VirtualTerm* vt) { vt_standout(vt); }
static void EFFECT_IMAGE_END_M(struct VirtualTerm* vt) { vt_standend(vt); }
static void EFFECT_FORM_START_M(struct VirtualTerm* vt) { vt_standout(vt); }
static void EFFECT_FORM_END_M(struct VirtualTerm* vt) { vt_standend(vt); }
static void EFFECT_ACTIVE_START_NC(struct VirtualTerm* vt) { vt_underline(vt); }
static void EFFECT_ACTIVE_END_NC(struct VirtualTerm* vt) { vt_underlineend(vt); }
static void EFFECT_ACTIVE_START_M(struct VirtualTerm* vt) { vt_bold(vt); }
static void EFFECT_ACTIVE_END_M(struct VirtualTerm* vt) { vt_boldend(vt); }
static void EFFECT_VISITED_START_M(struct VirtualTerm* vt) { /**/ ; }
static void EFFECT_VISITED_END_M(struct VirtualTerm* vt) { /**/ ; }
static void EFFECT_MARK_START_M(struct VirtualTerm* vt) { vt_standout(vt); }
static void EFFECT_MARK_END_M(struct VirtualTerm* vt) { vt_standend(vt); }

#define define_effect(name_start, name_end, color_start, color_end, mono_start, mono_end) \
    static void name_start(struct VirtualTerm* vt)                                        \
    {                                                                                     \
        if (useColor) {                                                                   \
            color_start(vt);                                                              \
        } else {                                                                          \
            mono_start(vt);                                                               \
        }                                                                                 \
    }                                                                                     \
    static void name_end(struct VirtualTerm* vt)                                          \
    {                                                                                     \
        if (useColor) {                                                                   \
            color_end(vt);                                                                \
        } else {                                                                          \
            mono_end(vt);                                                                 \
        }                                                                                 \
    }

define_effect(effect_anchor_start, effect_anchor_end, EFFECT_ANCHOR_START_C,
    EFFECT_ANCHOR_END_C, EFFECT_ANCHOR_START_M, EFFECT_ANCHOR_END_M);

define_effect(effect_image_start, effect_image_end, EFFECT_IMAGE_START_C,
    EFFECT_IMAGE_END_C, EFFECT_IMAGE_START_M, EFFECT_IMAGE_END_M);

define_effect(effect_form_start, effect_form_end, EFFECT_FORM_START_C,
    EFFECT_FORM_END_C, EFFECT_FORM_START_M, EFFECT_FORM_END_M);

define_effect(effect_mark_start, effect_mark_end, EFFECT_MARK_START_C,
    EFFECT_MARK_END_C, EFFECT_MARK_START_M, EFFECT_MARK_END_M);

/*****************/
static void effect_active_start(struct VirtualTerm* vt)
{
    if (useColor) {
        if (useActiveColor) {
            {
                EFFECT_ACTIVE_START_C(vt);
            }
        } else {
            EFFECT_ACTIVE_START_NC(vt);
        }
    } else {
        EFFECT_ACTIVE_START_M(vt);
    }
}

static void effect_active_end(struct VirtualTerm* vt)
{
    if (useColor) {
        if (useActiveColor) {
            EFFECT_ACTIVE_END_C(vt);
        } else {
            EFFECT_ACTIVE_END_NC(vt);
        }
    } else {
        EFFECT_ACTIVE_END_M(vt);
    }
}

static void effect_visited_start(struct VirtualTerm* vt)
{
    if (useVisitedColor) {
        if (useColor) {
            EFFECT_VISITED_START_C(vt);
        } else {
            EFFECT_VISITED_START_M(vt);
        }
    }
}

static void effect_visited_end(struct VirtualTerm* vt)
{
    if (useVisitedColor) {
        if (useColor) {
            EFFECT_VISITED_END_C(vt);
        } else {
            EFFECT_VISITED_END_M(vt);
        }
    }
}

void vt_standout(struct VirtualTerm* vt)
{
    vt->CurrentMode |= S_STANDOUT;
}

void vt_standend(struct VirtualTerm* vt)
{
    vt->CurrentMode &= ~S_STANDOUT;
}

void vt_toggle_stand(struct VirtualTerm* vt)
{
    int i;
    l_prop* pr = vt->ScreenImage[vt->CurLine]->lineprop;
    pr[vt->CurColumn] ^= S_STANDOUT;
    if (CHMODE(pr[vt->CurColumn]) != C_WCHAR2) {
        for (i = vt->CurColumn + 1; CHMODE(pr[i]) == C_WCHAR2; i++)
            pr[i] ^= S_STANDOUT;
    }
}

void vt_bold(struct VirtualTerm* vt)
{
    vt->CurrentMode |= S_BOLD;
}

void vt_boldend(struct VirtualTerm* vt)
{
    vt->CurrentMode &= ~S_BOLD;
}

void vt_underline(struct VirtualTerm* vt)
{
    vt->CurrentMode |= S_UNDERLINE;
}

void vt_underlineend(struct VirtualTerm* vt)
{
    vt->CurrentMode &= ~S_UNDERLINE;
}

void vt_graphstart(struct VirtualTerm* vt)
{
    vt->CurrentMode |= S_GRAPHICS;
}

void vt_graphend(struct VirtualTerm* vt)
{
    vt->CurrentMode &= ~S_GRAPHICS;
}

void vt_setfcolor(struct VirtualTerm* vt, int color)
{
    vt->CurrentMode &= ~COL_FCOLOR;
    if ((color & 0xf) <= 7)
        vt->CurrentMode |= (((color & 7) | 8) << 8);
}

void vt_setbcolor(struct VirtualTerm* vt, int color)
{
    vt->CurrentMode &= ~COL_BCOLOR;
    if ((color & 0xf) <= 7)
        vt->CurrentMode |= (((color & 7) | 8) << 12);
}

#define do_effect1(effect, modeflag, action_start, action_end) \
    if (m & effect) {                                          \
        if (!modeflag) {                                       \
            action_start(vt);                                  \
            modeflag = true;                                   \
        }                                                      \
    }

#define do_effect2(effect, modeflag, action_start, action_end) \
    if (modeflag) {                                            \
        action_end(vt);                                        \
        modeflag = false;                                      \
    }

void vt_do_effects(struct VirtualTerm* vt, Lineprop m)
{
    /* effect end */
    do_effect2(PE_UNDER, ulmode, vt_underline, vt_underlineend);
    do_effect2(PE_STAND, somode, vt_standout, vt_standend);
    do_effect2(PE_BOLD, bomode, vt_bold, vt_boldend);
    do_effect2(PE_EMPH, emph_mode, vt_bold, vt_boldend);
    do_effect2(PE_ANCHOR, anch_mode, effect_anchor_start, effect_anchor_end);
    do_effect2(PE_IMAGE, imag_mode, effect_image_start, effect_image_end);
    do_effect2(PE_FORM, form_mode, effect_form_start, effect_form_end);
    do_effect2(PE_VISITED, visited_mode, effect_visited_start, effect_visited_end);
    do_effect2(PE_ACTIVE, active_mode, effect_active_start, effect_active_end);
    do_effect2(PE_MARK, mark_mode, effect_mark_start, effect_mark_end);
    if (graph_mode) {
        vt_graphend(vt);
        graph_mode = false;
    }

    /* effect start */
    do_effect1(PE_UNDER, ulmode, vt_underline, vt_underlineend);
    do_effect1(PE_STAND, somode, vt_standout, vt_standend);
    do_effect1(PE_BOLD, bomode, vt_bold, vt_boldend);
    do_effect1(PE_EMPH, emph_mode, vt_bold, vt_boldend);
    do_effect1(PE_ANCHOR, anch_mode, effect_anchor_start, effect_anchor_end);
    do_effect1(PE_IMAGE, imag_mode, effect_image_start, effect_image_end);
    do_effect1(PE_FORM, form_mode, effect_form_start, effect_form_end);
    do_effect1(PE_VISITED, visited_mode, effect_visited_start, effect_visited_end);
    do_effect1(PE_ACTIVE, active_mode, effect_active_start, effect_active_end);
    do_effect1(PE_MARK, mark_mode, effect_mark_start, effect_mark_end);
}

void vt_line_end(struct VirtualTerm* vt)
{
    if (somode) {
        somode = false;
        vt_standend(vt);
    }
    if (ulmode) {
        ulmode = false;
        vt_underlineend(vt);
    }
    if (bomode) {
        bomode = false;
        vt_boldend(vt);
    }
    if (emph_mode) {
        emph_mode = false;
        vt_boldend(vt);
    }

    if (anch_mode) {
        anch_mode = false;
        effect_anchor_end(vt);
    }
    if (imag_mode) {
        imag_mode = false;
        effect_image_end(vt);
    }
    if (form_mode) {
        form_mode = false;
        effect_form_end(vt);
    }
    if (visited_mode) {
        visited_mode = false;
        effect_visited_end(vt);
    }
    if (active_mode) {
        active_mode = false;
        effect_active_end(vt);
    }
    if (mark_mode) {
        mark_mode = false;
        effect_mark_end(vt);
    }
    if (graph_mode) {
        graph_mode = false;
        vt_graphend(vt);
    }
    if (color_mode)
        vt_do_color(vt, 0);
}
// if (somode) {
//     somode = FALSE;
//     standend(vt);
// }
// if (ulmode) {
//     ulmode = FALSE;
//     underlineend(vt);
// }
// if (bomode) {
//     bomode = FALSE;
//     boldend(vt);
// }
// if (emph_mode) {
//     emph_mode = FALSE;
//     boldend(vt);
// }
//
// if (anch_mode) {
//     anch_mode = FALSE;
//     effect_anchor_end(vt);
// }
// if (imag_mode) {
//     imag_mode = FALSE;
//     effect_image_end(vt);
// }
// if (form_mode) {
//     form_mode = FALSE;
//     effect_form_end(vt);
// }
// if (visited_mode) {
//     visited_mode = FALSE;
//     effect_visited_end(vt);
// }
// if (active_mode) {
//     active_mode = FALSE;
//     effect_active_end(vt);
// }
// if (mark_mode) {
//     mark_mode = FALSE;
//     effect_mark_end(vt);
// }
// if (graph_mode) {
//     graph_mode = FALSE;
//     graphend(vt);
// }
// if (color_mode)
//     do_color(0);
