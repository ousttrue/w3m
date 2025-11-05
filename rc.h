#pragma once

extern int no_rc_dir;

extern void show_params(FILE* fp);
extern int str_to_bool(char* value, int old);
extern char* confFile(char* base);
extern char* rcFile(char* base);
