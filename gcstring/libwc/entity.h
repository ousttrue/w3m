#pragma once

extern char UseAltEntity;

const char* conv_entity(unsigned int ch);
int getescapechar(const char** s);
const char* getescapecmd(const char** s);
