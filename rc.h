#pragma once
#include <stdio.h>
#include <gcstr.h>

void show_params(FILE* fp);
struct KeyValueList;
void panel_set_option(struct KeyValueList*);
int set_param_option(char* option);
void sync_with_option(void);
char* auxbinFile(char* base);
Str load_option_panel(void);
