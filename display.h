/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_DISPLAY_H
#define W3M_DISPLAY_H

#include "config.h"

void fmInit(void);
void fmTerm(void);

#ifdef USE_COLOR
extern int useColor;
extern int highIntensityColors;
extern int basic_color;	/* don't change */
extern int anchor_color;	/* blue  */
extern int image_color;	/* green */
extern int form_color;	/* red   */
#ifdef USE_BG_COLOR
extern int bg_color;	/* don't change */
extern int mark_color;	/* cyan */
#endif				/* USE_BG_COLOR */
extern int useActiveColor;
extern int active_color;	/* cyan */
extern int useVisitedColor;
extern int visited_color;	/* magenta  */
#endif				/* USE_COLOR */
#endif
