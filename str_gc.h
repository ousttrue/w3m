#pragma once
#include "Str.h"
#include "libwc/ces.h"

extern const wc_ces InnerCharset;
extern wc_ces SystemCharset;
extern const char* Editor;
extern const char* personal_document_root;
extern char* rc_dir;
extern char* tmp_dir;

pStr mydirname(const char* s);
pStr romanNumeral(int n);
pStr romanAlphabet(int n);
pStr myExtCommand(const char* cmd, const char* arg, bool redirect);
pStr editor_cmd(const char* file, int line);

pStr expandPath(const char* name);
/// expand `~` to username in string
pStr expandName(const char* name);
/// resolve relative path etc. for example /path/../to/../some
pStr cleanupName(const char* name);
pStr base64_encode(const char* src, size_t len);

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
pStr tmpfname(int CurrentPid, enum TmpFileType type, const char* ext);

enum LineMode {
    RAW_MODE,
    PAGER_MODE,
    HTML_MODE,
    HEADER_MODE,
};
void cleanup_line(pStr s, enum LineMode mode);
pStr convertLine(bool do_chop, pStr line, int mode, wc_ces* charset, wc_ces doc_charset);
pStr filename_extension(const char* patch, bool is_url);
pStr remove_space(const char* str);
pStr Str_form_quote(pStr x);
pStr shell_quote(const char* str);
pStr guess_filename(const char* file);
