#pragma once
#include <stdio.h>
#include <gcstr.h>

void show_params(FILE* fp);
struct KeyValueList;
void panel_set_option(struct KeyValueList*);
void sync_with_option(void);
char* auxbinFile(char* base);
