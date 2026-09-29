#pragma once
#include "Str.h"
#include "libwc/wc.h"
#include "libwc/ces.h"

extern const wc_ces InnerCharset;
extern wc_ces SystemCharset;
extern const char* Editor;
extern const char* personal_document_root;
extern char* rc_dir;
extern char* tmp_dir;

Str mydirname(const char* s);
Str romanNumeral(int n);
Str romanAlphabet(int n);
Str myExtCommand(const char* cmd, const char* arg, bool redirect);
Str editor_cmd(const char* file, int line);

Str expandPath(const char* name);
/// expand `~` to username in string
Str expandName(const char* name);
Str file_to_url(const char* file, const char* CurrentDir);
/// resolve relative path etc. for example /path/../to/../some
Str cleanupName(const char* name);
Str base64_encode(const char* src, size_t len);

void initFileToDelete();
void pushTmpFile(const char* tmpf);
const char* popFileToDelete();

enum TmpFileType {
    TMPF_DFL,
    TMPF_SRC,
    TMPF_FRAME,
    TMPF_CACHE,
    TMPF_COOKIE,
    TMPF_HIST,
    MAX_TMPF_TYPE,
};
Str tmpfname(int CurrentPid, enum TmpFileType type, const char* ext);

Str convertLine(bool do_chop, Str line, int mode, wc_ces* charset, wc_ces doc_charset);
