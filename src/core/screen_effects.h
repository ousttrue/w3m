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

void standout(struct VirtualTerm* vt);
void standend(struct VirtualTerm* vt);
void bold(struct VirtualTerm* vt);
void boldend(struct VirtualTerm* vt);
void underline(struct VirtualTerm* vt);
void underlineend(struct VirtualTerm* vt);
void graphstart(struct VirtualTerm* vt);
void graphend(struct VirtualTerm* vt);
void setfcolor(struct VirtualTerm* vt, int color);
void setbcolor(struct VirtualTerm* vt, int color);

void do_effects(Lineprop m, struct VirtualTerm* vt);

void EFFECT_ANCHOR_END_C(struct VirtualTerm* vt);
void do_color(struct VirtualTerm* vt, Linecolor c);
void line_end(struct VirtualTerm* vt);
