#pragma once
#include <stdbool.h>

enum GrahicCharType {
    GRAPHIC_CHAR_CHARSET = 0,
    GRAPHIC_CHAR_DEC = 1,
    GRAPHIC_CHAR_ASCII = 2,
};
extern enum GrahicCharType UseGraphicChar;

struct TermEntry;
bool graph_ok(const struct TermEntry* t);
char graphchar(char c);
void setgraphchar(const struct TermEntry* t);
