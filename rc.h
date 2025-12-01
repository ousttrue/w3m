#pragma once
#include <stdio.h>
#include <gcstr.h>

struct KeyValueList;
void panel_set_option(struct KeyValueList*);
void sync_with_option(void);
char* auxbinFile(char* base);
