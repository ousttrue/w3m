const std = @import("std");
const c = @import("c.zig").c;

// #include "history.h"
// #include "textlist.h"
// #include "global.h"
// #include "rc.h"
// #include "url.h"
// #include "alloc.h"
// #include "indep.h"
// #include "hash.h"
// #include <sys/stat.h>
//
// #define HIST_LIST_MAX GENERAL_LIST_MAX
// #define HIST_HASH_SIZE 127
//
// #define HISTORY_FILE "history"
//
// typedef GeneralList HistList;
// struct Hist {
//     HistList* list;
//     ListItem* current;
//     Hash_sv* hash;
//     long long mtime;
// };
//
// struct Hist* LoadHist;
// struct Hist* SaveHist;
// struct Hist* URLHist;
// struct Hist* ShellHist;
// struct Hist* TextHist;
//
// static struct Hist* getHistory(enum HistoryType h)
// {
//     switch (h) {
//     case HistoryNone:
//         return NULL;
//     case HistoryLoad:
//         return LoadHist;
//     case HistorySave:
//         return SaveHist;
//     case HistoryURL:
//         return URLHist;
//     case HistoryShell:
//         return ShellHist;
//     case HistoryText:
//         return TextHist;
//     }
// }
//
// static struct Hist* newHist(void)
// {
//     struct Hist* hist = New(struct Hist);
//     hist->list = (HistList*)newGeneralList();
//     hist->current = NULL;
//     hist->hash = NULL;
//     return hist;
// }

export fn initHist() void {
    // LoadHist = newHist();
    // SaveHist = newHist();
    // ShellHist = newHist();
    // TextHist = newHist();
    // URLHist = newHist();
}

// static ListItem*
// getHashHist(struct Hist* hist, const char* ptr)
// {
//     ListItem* item;
//
//     if (hist == NULL || hist->list == NULL)
//         return NULL;
//     if (hist->hash == NULL) {
//         hist->hash = newHash_sv(HIST_HASH_SIZE);
//         for (item = hist->list->first; item; item = item->next)
//             putHash_sv(hist->hash, item->ptr, (void*)item);
//     }
//     return (ListItem*)getHash_sv(hist->hash, ptr, NULL);
// }

export fn hasHist(hist: c.HistoryType, ptr: [*c]const u8) bool {
    _ = hist;
    _ = ptr;
    return false;
    //     return getHashHist(getHistory(hist), ptr) != 0;
}

// static ListItem*
// _pushHist(struct Hist* hist, const char* ptr)
// {
//     ListItem* item;
//
//     if (hist == NULL || hist->list == NULL || hist->list->nitem >= HIST_LIST_MAX)
//         return NULL;
//     item = (ListItem*)newListItem((void*)allocStr(ptr, -1),
//         NULL, (ListItem*)hist->list->last);
//     if (hist->list->last)
//         hist->list->last->next = item;
//     else
//         hist->list->first = item;
//     hist->list->last = item;
//     hist->list->nitem++;
//     return item;
// }
//
// /* Merge entries from their history into ours */
// static void
// mergeHistory(struct Hist* ours, struct Hist* theirs)
// {
//     for (ListItem* item = theirs->list->first; item; item = item->next)
//         if (!getHashHist(ours, item->ptr))
//             _pushHist(ours, item->ptr);
// }

export fn historyBuffer(_hist: c.HistoryType) [*c]const u8 {
    _ = _hist;
    //     struct Hist* hist = getHistory(_hist);
    //     Str src = Strnew();
    //     ListItem* item;
    //     char *p, *q;
    //
    //     /* FIXME: gettextize? */
    //     Strcat_charp(src, "<html>\n<head><title>History Page</title></head>\n");
    //     Strcat_charp(src, "<body>\n<h1>History Page</h1>\n<hr>\n");
    //     Strcat_charp(src, "<ol>\n");
    //     if (hist && hist->list) {
    //         for (item = hist->list->last; item; item = item->prev) {
    //             q = html_quote(item->ptr);
    //             if (DecodeURL)
    //                 p = html_quote(url_decode2(item->ptr, NULL));
    //             else
    //                 p = q;
    //             Strcat_charp(src, "<li><a href=\"");
    //             Strcat_charp(src, q);
    //             Strcat_charp(src, "\">");
    //             Strcat_charp(src, p);
    //             Strcat_charp(src, "</a>\n");
    //         }
    //     }
    //     Strcat_charp(src, "</ol>\n</body>\n</html>");
    //     return src->ptr;
    // }
    //
    // static int _loadHistory(struct Hist* hist)
    // {
    //     FILE* f;
    //     Str line;
    //     struct stat st;
    //
    //     if (hist == NULL)
    //         return 1;
    //     if ((f = fopen(rcFile(HISTORY_FILE), "rt")) == NULL)
    //         return 1;
    //
    //     if (fstat(fileno(f), &st) == -1) {
    //         fclose(f);
    //         return 1;
    //     }
    //     hist->mtime = (long long)st.st_mtime;
    //
    //     while (!feof(f)) {
    //         line = Strfgets(f);
    //         Strchop(line);
    //         Strremovefirstspaces(line);
    //         Strremovetrailingspaces(line);
    //         if (line->length == 0)
    //             continue;
    //         _pushHist(hist, url_quote(line->ptr));
    //     }
    //     fclose(f);
    //     return 0;
    return "";
}

export fn loadHistory(hist: c.HistoryType) void {
    _ = hist;
    // return _loadHistory(getHistory(hist));
}

export fn saveHistory(_hist: c.HistoryType) void {
    _ = _hist;
    //     struct Hist* hist = getHistory(_hist);
    //     if (hist == NULL || hist->list == NULL)
    //         return;
    //
    //     char* histf = rcFile(HISTORY_FILE);
    //     struct stat st;
    //     if (stat(histf, &st) == -1) {
    //         return;
    //     }
    //
    //     if (hist->mtime != (long long)st.st_mtime) {
    //         struct Hist* fhist = newHist();
    //         if (_loadHistory(fhist)) {
    //             // disp_err_message(args, "Can't merge history", false);
    //         } else {
    //             mergeHistory(fhist, hist);
    //             hist = fhist;
    //         }
    //     }
    //
    //     const char* tmpf = tmpfname(TMPF_HIST, NULL);
    //     FILE* f = fopen(tmpf, "w");
    //     if (f == NULL) {
    //         return;
    //     }
    //
    //     ListItem* item = hist->list->first;
    //     for (; item && hist->list->nitem > URLHistSize;
    //         item = item->next)
    //         URLHistSize++;
    //     for (; item; item = item->next)
    //         fprintf(f, "%s\n", (char*)item->ptr);
    //     if (fclose(f) == EOF) {
    //         return;
    //     }
    //     int rename_ret = rename(tmpf, rcFile(HISTORY_FILE));
    //     if (rename_ret != 0) {
    //         // disp_err_message(args, "Can't open history", false);
    //         return;
    //     }
    //
    //     return;
    // }
    //
    // /*
    //  * The following functions are used for internal stuff, we need them regardless
    //  * if history is used or not.
    //  */
    // static struct Hist* copyHist(struct Hist* hist)
    // {
    //     if (hist == NULL)
    //         return NULL;
    //
    //     struct Hist* new = newHist();
    //     for (ListItem* item = hist->list->first; item; item = item->next)
    //         _pushHist(new, item->ptr);
    //     return new;
}

export fn unshiftHist(_hist: c.HistoryType, ptr: [*c]const u8) void {
    _ = _hist;
    _ = ptr;
    //     struct Hist* hist = getHistory(_hist);
    //     if (hist == NULL || hist->list == NULL || hist->list->nitem >= HIST_LIST_MAX)
    //         return;
    //     ListItem* item = (ListItem*)newListItem((void*)allocStr(ptr, -1),
    //         (ListItem*)hist->list->first, NULL);
    //     if (hist->list->first)
    //         hist->list->first->prev = item;
    //     else
    //         hist->list->last = item;
    //     hist->list->first = item;
    //     hist->list->nitem++;
    //     return;
}

export fn pushHist(hist: c.HistoryType, ptr: [*c]const u8) void {
    _ = hist;
    _ = ptr;
    // _pushHist(getHistory(hist), ptr);
}

// /* Don't mix pushHashHist() and pushHist()/unshiftHist(). */
//
// static ListItem*
// pushHashHist(struct Hist* hist, const char* ptr)
// {
//     ListItem* item;
//
//     if (hist == NULL || hist->list == NULL || hist->list->nitem >= HIST_LIST_MAX)
//         return NULL;
//     item = getHashHist(hist, ptr);
//     if (item) {
//         if (item->next)
//             item->next->prev = item->prev;
//         else /* item == hist->list->last */
//             hist->list->last = item->prev;
//         if (item->prev)
//             item->prev->next = item->next;
//         else /* item == hist->list->first */
//             hist->list->first = item->next;
//         hist->list->nitem--;
//     }
//     item = _pushHist(hist, ptr);
//     putHash_sv(hist->hash, ptr, (void*)item);
//     return item;
// }

export fn pushUrlHist(ptr: [*c]const u8) void {
    _ = ptr;
    // pushHashHist(getHistory(HistoryURL), ptr);
}

export fn lastHist(_hist: c.HistoryType) [*c]const u8 {
    _ = _hist;
    return "";

    //     struct Hist* hist = getHistory(_hist);
    //     if (hist == NULL || hist->list == NULL)
    //         return NULL;
    //     if (hist->list->last) {
    //         hist->current = hist->list->last;
    //         return hist->current->ptr;
    //     }
    //     return NULL;
}

export fn nextHist(_hist: c.HistoryType) [*c]const u8 {
    _ = _hist;
    return "";

    //     struct Hist* hist = getHistory(_hist);
    //     if (hist == NULL || hist->list == NULL)
    //         return NULL;
    //     if (hist->current && hist->current->next) {
    //         hist->current = hist->current->next;
    //         return hist->current->ptr;
    //     }
    //     return NULL;
}

export fn prevHist(_hist: c.HistoryType) [*c]const u8 {
    _ = _hist;
    return "";

    //     struct Hist* hist = getHistory(_hist);
    //     if (hist == NULL || hist->list == NULL)
    //         return NULL;
    //     if (hist->current && hist->current->prev) {
    //         hist->current = hist->current->prev;
    //         return hist->current->ptr;
    //     }
    //     return NULL;
}
