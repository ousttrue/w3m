#pragma once

// runtime

extern int CurrentKey;
extern int prev_key;
extern char *CurrentKeyData;
extern char *CurrentCmdData;
extern int prec_num;
#define PREC_NUM (prec_num ? prec_num : 1)
extern bool on_target;

void mainLoop();
void resize_hook(int);
