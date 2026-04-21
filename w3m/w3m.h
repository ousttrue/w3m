#pragma once
#include <stdbool.h>
#include <stdint.h>

struct CmdArgs {
    int ch;
};

typedef void (*CmdFunc)(struct CmdArgs* args);

int getch_timeout(uint32_t ms, struct CmdArgs* args);

static inline int getch(struct CmdArgs* args)
{
    return getch_timeout(0, args);
}

void unget(int ch);

extern void w3mFunc(const char* cmd);

extern bool w3m_args(struct CmdArgs* args, int argc, const char** argv);
extern int w3m_loop(void);

extern char* w3m_auxbin_dir(void);
extern char* w3m_lib_dir(void);
extern char* w3m_etc_dir(void);
extern char* w3m_conf_dir(void);
extern void set_environ(const char* var, const char* value);

void initImage(void);
void termImage(void);
void drawImage(void);
void clearImage(void);

void put_image_osc5379(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
void put_image_sixel(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
void put_image_iterm2(const char* url, int x, int y, int w, int h);
void put_image_kitty(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);
