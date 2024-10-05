#pragma once
#include "text/Str.h"
#include <stdbool.h>
#include <stdint.h>

enum GraphicCharType {
  GRAPHIC_CHAR_ASCII = 2,
  GRAPHIC_CHAR_DEC = 1,
  GRAPHIC_CHAR_CHARSET = 0,
};
extern enum GraphicCharType UseGraphicChar;

extern bool QuietMessage;

void term_fmInit();
bool term_is_initialized();
void term_fmTerm();
void term_cbreak();
void term_raw();

bool term_graph_ok();
void term_clear();
void term_title(const char *s);
void term_bell();
void term_refresh();
void term_message(const char *msg);
Str term_inputpwd();
void term_input(const char *msg);
const char *term_inputAnswer(const char *prompt);
void term_showProgress(int64_t *linelen, int64_t *trbyte,
                       int64_t current_content_length);
#ifndef _WIN32
bool term_inputAuth(const char *realm, bool proxy, Str *uname, Str *pwd);
#endif

void term_err_message(const char *s);
const char *term_message_to_html();
void disp_err_message(const char *s, int redraw_current);
void disp_message_nsec(const char *s, int redraw_current, int sec, int purge,
                       int mouse);
void disp_message(const char *s, int redraw_current);
#define disp_message_nomouse disp_message
void set_delayed_message(const char *s);
void term_show_delayed_message();
