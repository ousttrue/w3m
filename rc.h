#pragma once

extern int no_rc_dir;

void init_rc(void);
void show_params(FILE* fp);
int str_to_bool(char* value, int old);
char* confFile(char* base);
char* rcFile(char* base);
struct parsed_tagarg;
void panel_set_option(struct parsed_tagarg*);
int set_param_option(char* option);
void sync_with_option(void);
char* get_param_option(char* name);
char* auxbinFile(char* base);
void init_tmp(void);
char* etcFile(char* base);
