#pragma once
#include "geometry.h"
#include "buffer.h"
#include "Content.h"

struct Buffer;
extern struct Buffer* Currentbuf;
extern struct Buffer* Firstbuf;
extern char ArgvIsURL;
extern int clear_buffer;

void parseArgs(int argc, char** argv);
void SAVE_BUFPOSITION(struct Buffer *sbufp);
void cmd_loadfile(struct UI ui, const char* fn);
void pushBuffer(struct UI ui, struct Buffer* buf);
void delBuffer(struct UI ui, struct Buffer* buf);
void repBuffer(struct UI ui, struct Buffer* oldbuf, struct Buffer* buf);
void setCurrentBuffer(struct Buffer* buf);
void cmd_loadContent(struct UI ui, struct Content c);
struct Buffer* pushContent(struct UI ui, struct Content c);
