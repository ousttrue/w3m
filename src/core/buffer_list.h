#pragma once
#include "geometry.h"
#include "Content.h"

struct Buffer;
extern struct Buffer* Currentbuf;
extern struct Buffer* Firstbuf;
extern char ArgvIsURL;
extern int clear_buffer;

void parseArgs(struct UI *ui, int argc, char** argv);
void SAVE_BUFPOSITION(struct Buffer* sbufp);
void pushBuffer(struct Buffer* buf);
void delBuffer(struct Buffer* buf);
void repBuffer(struct Buffer* oldbuf, struct Buffer* buf);
void setCurrentBuffer(struct Buffer* buf);
struct Buffer* pushContent(struct UI *ui, struct Content c, int cols, bool use_graphic);
