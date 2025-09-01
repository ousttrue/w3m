#pragma once
#include "line.h"

extern int graph_mode;

extern int useColor;
extern int basic_color; /* don't change */
extern int anchor_color; /* blue  */
extern int image_color; /* green */
extern int form_color; /* red   */
extern int bg_color; /* don't change */
extern int mark_color; /* cyan */
extern int useActiveColor;
extern int active_color; /* cyan */
extern int useVisitedColor;
extern int visited_color; /* magenta  */

struct VirtualTerm;

void vt_standout(struct VirtualTerm* vt);
void vt_standend(struct VirtualTerm* vt);
void vt_bold(struct VirtualTerm* vt);
void vt_boldend(struct VirtualTerm* vt);
void vt_underline(struct VirtualTerm* vt);
void vt_underlineend(struct VirtualTerm* vt);
void vt_graphstart(struct VirtualTerm* vt);
void vt_graphend(struct VirtualTerm* vt);
void vt_setfcolor(struct VirtualTerm* vt, int color);
void vt_setbcolor(struct VirtualTerm* vt, int color);

void vt_do_effects(struct VirtualTerm* vt, Lineprop m);

void EFFECT_ANCHOR_END_C(struct VirtualTerm* vt);
void vt_do_color(struct VirtualTerm* vt, Linecolor c);
void vt_line_end(struct VirtualTerm* vt);
