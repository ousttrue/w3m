#pragma once
void fmInit(void);
void fmTerm(void);
void record_err_message(char* s);
void disp_message_nsec(char* s, int redraw_current, int sec, int purge, int mouse);
struct _Buffer;
void displayBuffer(struct _Buffer* buf, int mode);
void loadImage(struct _Buffer* buf, int flag);
