#pragma once

struct Frame;
struct Frame* displayBuffer();

struct VirtualTerm;
struct Frame* screenToFrame(const struct VirtualTerm* vt);

extern void message(char* s, int return_x, int return_y);
extern void disp_err_message(char* s, int redraw_current);
extern void disp_message_nsec(char* s, int redraw_current, int sec, int purge, int mouse);
extern void disp_message(char* s, int redraw_current);

