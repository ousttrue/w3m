#pragma once
#include "geometry.h"

void showProgress(struct UI *ui, long long current_content_length, long long* linelen, long long* trbyte);
char* convert_size(long long size, int usefloat);
