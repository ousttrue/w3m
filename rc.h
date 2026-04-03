#pragma once

#include <stdio.h>
#define W3MHELPERPANEL_CMDNAME "w3mhelperpanel"

void init_tmp(void);
struct _Buffer* load_option_panel(void);
struct parsed_tagarg;
void panel_set_option(struct parsed_tagarg*);
void sync_with_option(void);
char* rcFile(char* base);
char* etcFile(char* base);
char* confFile(char* base);
char* auxbinFile(char* base);
char* libFile(char* base);
char* helpFile(char* base);
void init_rc(void);
int set_param_option(char* option);
char* get_param_option(char* name);
void show_params(FILE* fp);
int str_to_bool(char* value, int old);
