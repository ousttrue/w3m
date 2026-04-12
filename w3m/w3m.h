#pragma once
#include <stdbool.h>

struct CmdArgs {
    void* p;
};

typedef void (*CmdFunc)(struct CmdArgs);

extern void w3mFunc(const char* cmd);

extern bool w3m_args(int argc, const char** argv);
extern int w3m_loop(void);

extern char* w3m_auxbin_dir(void);
extern char* w3m_lib_dir(void);
extern char* w3m_etc_dir(void);
extern char* w3m_conf_dir(void);
