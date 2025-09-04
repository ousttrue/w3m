#pragma once
#include <stdio.h>
#include <wc.h>

extern char* tmp_dir;
extern char* rc_dir;

struct _Buffer;
struct KeyValue;
struct _ParsedURL;

void show_params(FILE* fp);
int str_to_bool(const char* value, int old);
int set_param_option(char* option);
char* get_param_option(char* name);
void init_rc(void);
void init_tmp(void);
struct _Buffer* load_option_panel(void);
void panel_set_option(struct KeyValue*);
void sync_with_option(void);
char* rcFile(const char* base);
char* etcFile(char* base);
char* confFile(char* base);
char* auxbinFile(char* base);
// char* libFile(char* base);
// char* helpFile(char* base);


