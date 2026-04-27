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

void addDeleteFile(const char* file);
void deleteFiles(void);
enum TmpFileType {
    TMPF_DFL = 0,
    TMPF_SRC = 1,
    TMPF_FRAME = 2,
    TMPF_CACHE = 3,
    TMPF_COOKIE = 4,
    TMPF_HIST = 5,
    MAX_TMPF_TYPE = 6,
};
const char* tmpfname(enum TmpFileType type, const char* ext);

bool MoveFile(const char* path1, const char* path2);

int checkOverWrite(struct CmdArgs* args, const char* path);
