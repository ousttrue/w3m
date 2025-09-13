#pragma once
#include "geometry.h"
#include "Content.h"
#include <stdio.h>
#include <wc.h>

struct KeyValue;

void show_params(FILE* fp);
int str_to_bool(const char* value, int old);
int set_param_option(const char* option);
const char* get_param_option(const char* name);
void init_rc(void);
void init_tmp(void);
struct Content load_option_panel(struct UI ui);
void panel_set_option(struct UI ui, struct KeyValue*);
void sync_with_option(void);

FILE* openSecretFile(const char* fname);
