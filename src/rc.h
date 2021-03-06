#pragma once
#include <memory>
#include <stdio.h>
#include "html/parsetag.h"

#define MINIMUM_PIXEL_PER_CHAR 4.0
#define MAXIMUM_PIXEL_PER_CHAR 32.0

void init_rc(void);
void panel_set_option(tcb::span<parsed_tagarg>);
const char *rcFile(const char *base);
const char *etcFile(const char *base);
const char *confFile(const char *base);
const char *auxbinFile(const char *base);
int str_to_bool(const char *value, int old);
void show_params(FILE *fp);
int set_param_option(const char *option);
const char *get_param_option(const char *name);
void sync_with_option(void);
std::shared_ptr<struct Buffer> load_option_panel(void);
