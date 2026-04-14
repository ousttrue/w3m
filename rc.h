#pragma once
#include <w3m.h>
#include <stdbool.h>
#include <stdio.h>

#define W3MHELPERPANEL_CMDNAME "w3mhelperpanel"

void init_tmp(void);
struct Buffer* load_option_panel(void);
struct parsed_tagarg;
void panel_set_option(struct CmdArgs *args, struct parsed_tagarg*);
void sync_with_option(struct CmdArgs *args);
char* rcFile(const char* base);
char* etcFile(const char* base);
char* confFile(const char* base);
char* auxbinFile(const char* base);
char* libFile(const char* base);
char* helpFile(const char* base);
void init_rc(void);
int set_param_option(const char* option);
char* get_param_option(const char* name);
void show_params(FILE* fp);
int str_to_bool(const char* value, bool old);
void loadSiteconf(void);
