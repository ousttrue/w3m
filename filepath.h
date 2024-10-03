#pragma once

const char *lastFileName(const char *path);
const char *mybasename(const char *s);
const char *mydirname(const char *s);
char *guess_filename(const char *file);

bool dir_exist(const char *path);
