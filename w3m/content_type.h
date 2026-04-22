#pragma once

void initMimeTypes(void);
const char* guessContentType(const char* filename);
bool is_html_type(const char* type);
