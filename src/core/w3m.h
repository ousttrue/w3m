#pragma once

extern char* mkd_tmp_dir;
extern int use_mark;
extern const char* config_file;
extern char FollowLocale;
extern int confirm_on_quit;
extern int CurrentKey;
extern const char* CurrentKeyData;
extern const char* CurrentCmdData;

// entry point
void main_loop(int argc, char** argv);

// raw mode
// TODO: UI
void fmInit();
// normal mode
// TODO: UI
void fmTerm();
