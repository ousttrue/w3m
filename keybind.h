#ifndef W3M_KEYBIND_H
#define W3M_KEYBIND_H

extern int CurrentKey;
extern char *CurrentCmdData;

extern unsigned char GlobalKeymap[];
extern unsigned char EscKeymap[];
extern unsigned char EscBKeymap[];
extern unsigned char EscDKeymap[];
#ifdef __EMX__
extern unsigned char PcKeymap[];
#endif
#endif
