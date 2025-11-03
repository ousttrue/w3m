#ifndef W3M_DISPLAY_H
#define W3M_DISPLAY_H

void fmInit(void);
void fmTerm(void);

void record_err_message(char* s);
void disp_message_nsec(char* s, int redraw_current, int sec, int purge, int mouse);

#endif
