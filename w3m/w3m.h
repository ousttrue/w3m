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
int dir_exist(const char* path);

void initImage(void);
void deinitImage(void);
void drawImage(void);
void clearImage(void);

