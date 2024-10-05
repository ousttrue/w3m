// term control
#pragma once

void termcon_initialize();
void termcon_finalize();
void termcon_color_default();
void termcon_attribute_clear();
void termcon_standout_start();
void termcon_standout_end();
void termcon_bold_start();
void termcon_bold_end();
void termcon_underline_start();
void termcon_underline_end();
bool termcon_acs_has();
void termcon_acs_enable();
const char *termcon_acs_map();
void termcon_altchar_end();
void termcon_altchar_start();
void termcon_clear();
void termcon_clear_eol();
void termcon_cursor_move(int line, int col);
void termcon_cursor_right();
