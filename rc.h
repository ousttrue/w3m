#pragma once
#include <stdio.h>
#include <gcstr.h>

void init_rc(void);
void show_params(FILE* fp);
char* confFile(char* base);
char* rcFile(char* base);
struct KeyValueList;
void panel_set_option(struct KeyValueList*);
int set_param_option(char* option);
void sync_with_option(void);
char* get_param_option(char* name);
char* auxbinFile(char* base);
void init_tmp(void);
char* etcFile(char* base);
Str load_option_panel(void);
