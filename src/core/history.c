#include "history.h"
#include "runtime.h"
#include "hash.h"
#include "quote.h"
#include "html_quote.h"
#include "w3m.h"
#include <alloc.h>
#include <sys/stat.h>

#define HISTORY_FILE "history"

int UseHistory = (true);
int URLHistSize = (100);
int SaveURLHist = (true);

/* Merge entries from their history into ours */
static int
mergeHistory(struct Hist* ours, struct Hist* theirs)
{
    HistItem* item;

    for (item = theirs->list->first; item; item = item->next)
        if (!getHashHist(ours, item->ptr))
            pushHist(ours, (char*)item->ptr);

    return 0;
}

Str historyBuffer_html(struct Hist* hist)
{
    Str src = Strnew();
    Strcat_charp(src, "<html>\n<head><title>History Page</title></head>\n");
    Strcat_charp(src, "<body>\n<h1>History Page</h1>\n<hr>\n");
    Strcat_charp(src, "<ol>\n");
    if (hist && hist->list) {
        HistItem* item;
        for (item = hist->list->last; item; item = item->prev) {
            const char* q = html_quote(item->ptr);
            const char* p;
            if (DecodeURL)
                p = html_quote(url_decode2(item->ptr, 0));
            else
                p = q;
            Strcat_charp(src, "<li><a href=\"");
            Strcat_charp(src, q);
            Strcat_charp(src, "\">");
            Strcat_charp(src, p);
            Strcat_charp(src, "</a>\n");
        }
    }
    Strcat_charp(src, "</ol>\n</body>\n</html>");
    return src;
}

int loadHistory(struct Hist* hist)
{
    FILE* f;
    Str line;
    struct stat st;

    if (hist == NULL)
        return 1;
    if ((f = fopen(rcFile(HISTORY_FILE), "rt")) == NULL)
        return 1;

    if (fstat(fileno(f), &st) == -1) {
        fclose(f);
        return 1;
    }
    hist->mtime = (long long)st.st_mtime;

    while (!feof(f)) {
        line = Strfgets(f);
        Strchop(line);
        Strremovefirstspaces(line);
        Strremovetrailingspaces(line);
        if (line->length == 0)
            continue;
        pushHist(hist, url_quote(line->ptr));
    }
    fclose(f);
    return 0;
}

void saveHistory(struct Hist* hist, size_t size)
{
    FILE* f;
    struct Hist* fhist;
    HistItem* item;
    int rename_ret;
    struct stat st;

    if (hist == NULL || hist->list == NULL)
        return;

    const char* histf;
    histf = rcFile(HISTORY_FILE);
    if (stat(histf, &st) == -1)
        goto fail;
    if (hist->mtime != (long long)st.st_mtime) {
        fhist = newHist();
        if (loadHistory(fhist) || mergeHistory(fhist, hist))
            message(getUI(), MSG_ERR, "Can't merge history");
        else
            hist = fhist;
    }

    char* tmpf = tmpfname(TMPF_HIST, NULL)->ptr;
    if ((f = fopen(tmpf, "w")) == NULL)
        goto fail;
    for (item = hist->list->first; item && hist->list->nitem > size;
        item = item->next)
        size++;
    for (; item; item = item->next)
        fprintf(f, "%s\n", (char*)item->ptr);
    if (fclose(f) == EOF)
        goto fail;
    rename_ret = rename(tmpf, rcFile(HISTORY_FILE));
    if (rename_ret != 0)
        goto fail;

    return;

fail:
    message(getUI(), MSG_ERR, "Can't open history");
    return;
}

/*
 * The following functions are used for internal stuff, we need them regardless
 * if history is used or not.
 */

struct Hist* newHist(void)
{
    struct Hist* hist;

    hist = New(struct Hist);
    hist->list = (HistList*)newGeneralList();
    hist->current = NULL;
    hist->hash = NULL;
    return hist;
}

struct Hist* copyHist(struct Hist* hist)
{
    struct Hist* new;
    HistItem* item;

    if (hist == NULL)
        return NULL;
    new = newHist();
    for (item = hist->list->first; item; item = item->next)
        pushHist(new, (char*)item->ptr);
    return new;
}

HistItem*
unshiftHist(struct Hist* hist, const char* ptr)
{
    HistItem* item;

    if (hist == NULL || hist->list == NULL || hist->list->nitem >= HIST_LIST_MAX)
        return NULL;
    item = (HistItem*)newListItem((void*)allocStr(ptr, -1),
        (ListItem*)hist->list->first, NULL);
    if (hist->list->first)
        hist->list->first->prev = item;
    else
        hist->list->last = item;
    hist->list->first = item;
    hist->list->nitem++;
    return item;
}

HistItem*
pushHist(struct Hist* hist, const char* ptr)
{
    HistItem* item;

    if (hist == NULL || hist->list == NULL || hist->list->nitem >= HIST_LIST_MAX)
        return NULL;
    item = (HistItem*)newListItem((void*)allocStr(ptr, -1),
        NULL, (ListItem*)hist->list->last);
    if (hist->list->last)
        hist->list->last->next = item;
    else
        hist->list->first = item;
    hist->list->last = item;
    hist->list->nitem++;
    return item;
}

/* Don't mix pushHashHist() and pushHist()/unshiftHist(). */

HistItem*
pushHashHist(struct Hist* hist, const char* ptr)
{
    HistItem* item;

    if (hist == NULL || hist->list == NULL || hist->list->nitem >= HIST_LIST_MAX)
        return NULL;
    item = getHashHist(hist, ptr);
    if (item) {
        if (item->next)
            item->next->prev = item->prev;
        else /* item == hist->list->last */
            hist->list->last = item->prev;
        if (item->prev)
            item->prev->next = item->next;
        else /* item == hist->list->first */
            hist->list->first = item->next;
        hist->list->nitem--;
    }
    item = pushHist(hist, ptr);
    putHash_sv(hist->hash, ptr, (void*)item);
    return item;
}

HistItem*
getHashHist(struct Hist* hist, const char* ptr)
{
    HistItem* item;

    if (hist == NULL || hist->list == NULL)
        return NULL;
    if (hist->hash == NULL) {
        hist->hash = newHash_sv(HIST_HASH_SIZE);
        for (item = hist->list->first; item; item = item->next)
            putHash_sv(hist->hash, (char*)item->ptr, (void*)item);
    }
    return (HistItem*)getHash_sv(hist->hash, ptr, NULL);
}

char* lastHist(struct Hist* hist)
{
    if (hist == NULL || hist->list == NULL)
        return NULL;
    if (hist->list->last) {
        hist->current = hist->list->last;
        return (char*)hist->current->ptr;
    }
    return NULL;
}

char* nextHist(struct Hist* hist)
{
    if (hist == NULL || hist->list == NULL)
        return NULL;
    if (hist->current && hist->current->next) {
        hist->current = hist->current->next;
        return (char*)hist->current->ptr;
    }
    return NULL;
}

char* prevHist(struct Hist* hist)
{
    if (hist == NULL || hist->list == NULL)
        return NULL;
    if (hist->current && hist->current->prev) {
        hist->current = hist->current->prev;
        return (char*)hist->current->ptr;
    }
    return NULL;
}
