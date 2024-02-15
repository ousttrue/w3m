#pragma once
#include <memory>

void message(const char *s, int return_x, int return_y);

void record_err_message(const char *s);
struct Buffer;
std::shared_ptr<Buffer> message_list_panel();
void disp_err_message(const char *s);
void disp_message_nsec(const char *s, int sec, int purge);
void disp_message(const char *s);
void set_delayed_message(const char *s);

void refresh_message();

void showProgress(long long *linelen, long long *trbyte,
                  long long current_content_length);

char *convert_size(long long size, int usefloat);
char *convert_size2(long long size1, long long size2, int usefloat);
