/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_DISPLAY_H
#define W3M_DISPLAY_H

#include "config.h"

/* Effect ( standout/underline ) */
#define P_EFFECT	0x40ff
#define PE_NORMAL	0x00
#define PE_MARK		0x01
#define PE_UNDER	0x02
#define PE_STAND	0x04
#define PE_BOLD		0x08
#define PE_ANCHOR       0x10
#define PE_EMPH         0x08
#define PE_IMAGE        0x20
#define PE_FORM         0x40
#define PE_ACTIVE	0x80
#define PE_VISITED	0x4000

/* Extra effect */
#define PE_EX_ITALIC	0x01
#define PE_EX_INSERT	0x02
#define PE_EX_STRIKE	0x04

#define PE_EX_ITALIC_E	PE_UNDER
#define PE_EX_INSERT_E	PE_UNDER
#define PE_EX_STRIKE_E	PE_STAND

#define CharType(c)	((c)&P_CHARTYPE)
#define CharEffect(c)	((c)&(P_EFFECT|PC_SYMBOL))
#define SetCharType(v,c)	((v)=(((v)&~P_CHARTYPE)|(c)))

void fmInit(void);
void fmTerm(void);

extern char fmInitialized;
extern char QuietMessage;
extern char TrapSignal;
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
