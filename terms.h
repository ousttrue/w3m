#pragma once
#include "Str.h"
#include <stdbool.h>
#include <stdint.h>

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
char *term_inputAnswer(char *prompt);
void term_showProgress(int64_t *linelen, int64_t *trbyte,
                       int64_t current_content_length);
bool term_inputAuth(const char *realm, bool proxy, Str *uname, Str *pwd);

void setup_child(int child, int i, int f);

void term_err_message(const char *s);
const char *term_message_to_html();
void disp_err_message(char *s, int redraw_current);
void disp_message_nsec(char *s, int redraw_current, int sec, int purge,
                       int mouse);
void disp_message(char *s, int redraw_current);
#define disp_message_nomouse disp_message
void set_delayed_message(char *s);
void term_show_delayed_message();
