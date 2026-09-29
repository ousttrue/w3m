#pragma once

const char* conv_entity(unsigned int ch);
const char* getescapestr(const char** s, bool is_attr, int* pis_simple);
const char* getescapecmd(const char** s);
const char* html_unquote(const char* str);
const char* html_unquote_attr(const char* str);
