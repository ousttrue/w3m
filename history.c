/* vi: set sw=4 ts=8 ai sm noet : */
#include "history.h"

#include "str_gc.h"
#include "subprocess.h"
#include "alloc.h"
#include "config.h"
#include "fm.h"
#include "proto.h"
#include "rc.h"

#include <errno.h>

#ifdef USE_HISTORY
Buffer *
historyBuffer(Hist *hist)
{
    Str src = Strnew();
    HistItem *item;
    char n[sizeof("0")];
    char *p, *q;

    sprintf(n, "%d", !zeroBasedLinkNo);
    Strcat_charp(src, _("<html>\n<head><title>History Page</title></head>\n"));
    Strcat_charp(src, _("<body>\n<h1>History Page</h1>\n<hr>\n"));
    Strcat_m_charp(src, "<ol start=", n, ">\n", NULL);
    if (hist && hist->list) {
	for (item = hist->list->last; item; item = item->prev) {
	    q = html_quote(item->ptr);
	    if (DecodeURL)
		p = html_quote(url_decode2(item->ptr, NULL));
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
    return loadHTMLString(src);
}

/*
 * If HISTORY_FILE was written to after we last read it, read it again and
 * merge our URLHist into it.
 */
static int
syncUrlHistory(void)
{
    Hist *hist;
    HistItem *item;
    struct stat st;

    if (stat(rcFile(HISTORY_FILE)->ptr, &st)) {
	if (errno == ENOENT)
	    return 0;
	else
	    goto err;
    }

    if (URLHist->mtime == (long long)st.st_mtime)
	return 0;

    hist = URLHist;
    URLHist = newHist();
    if (loadUrlHistory()) {
	URLHist = hist;
	goto err;
    }

    /* Merge */
    for (item = hist->list->first; item; item = item->next)
	if (!getHashHist(URLHist, item->ptr))
	    pushHist(URLHist, (char *)item->ptr);

    return 0;

err:
    disp_err_message("Can't sync URL history", FALSE);
    return 1;
}

int
loadUrlHistory(void)
{
    FILE *f;
    Str line;
    struct stat st;

    if (URLHist == NULL)
	return 1;
    if ((f = fopen(rcFile(HISTORY_FILE)->ptr, "rt")) == NULL)
	return 1;

    if (fstat(fileno(f), &st) == -1) {
	fclose(f);
	return 1;
    }
    URLHist->mtime = (long long)st.st_mtime;

    while (!feof(f)) {
	line = Strfgets(f);
	Strchop(line);
	Strremovefirstspaces(line);
	Strremovetrailingspaces(line);
	if (line->length == 0)
	    continue;
	pushHist(URLHist, url_quote(line->ptr));
    }
    fclose(f);
    return 0;
}

void
saveUrlHistory(void)
{
    FILE *f;
    HistItem *item;
    char *tmpf;
    long long mtime;
    struct stat st;

    if (URLHist == NULL || URLHist->list == NULL)
	return;

    syncUrlHistory();

    mtime = URLHist->mtime;

    tmpf = tmpfname(CurrentPid, TMPF_HIST, NULL)->ptr;
    if ((f = fopen(tmpf, "w")) == NULL)
	goto fail;

    /* Respect URLHistSize before saving */
    for (item = URLHist->list->first;
	 item && URLHist->list->nitem > URLHistSize;
	 item = item->next);

    for (; item; item = item->next)
	fprintf(f, "%s\n", (char *)item->ptr);
    if (fclose(f) == EOF)
	goto fail;

    if (stat(tmpf, &st))
	goto fail;

    URLHist->mtime = (long long)st.st_mtime;
    if (rename(tmpf, rcFile(HISTORY_FILE)->ptr))
	goto fail;

    return;

fail:
    URLHist->mtime = mtime;
    unlink(tmpf);
    disp_err_message("Can't save history", FALSE);
    return;
}
#endif				/* USE_HISTORY */

/*
 * The following functions are used for internal stuff, we need them
 * regardless if history is used or not.
 */

Hist *
newHist(void)
{
    Hist *hist;

    hist = New(Hist);
    hist->list = (HistList *)newGeneralList();
    hist->current = NULL;
    hist->hash = NULL;
    return hist;
}

Hist *
copyHist(Hist *hist)
{
    Hist *new;
    HistItem *item;

    if (hist == NULL)
	return NULL;
    new = newHist();
    for (item = hist->list->first; item; item = item->next)
	pushHist(new, item->ptr);
    return new;
}

HistItem *
unshiftHist(Hist *hist, const char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL ||
	hist->list->nitem >= HIST_LIST_MAX)
	return NULL;
    item = (HistItem *)newListItem(allocStr(ptr, -1),
				   (ListItem *)hist->list->first, NULL);
    if (hist->list->first)
	hist->list->first->prev = item;
    else
	hist->list->last = item;
    hist->list->first = item;
    hist->list->nitem++;
    return item;
}

HistItem *
pushHist(Hist *hist, const char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL ||
	hist->list->nitem >= HIST_LIST_MAX)
	return NULL;
    item = (HistItem *)newListItem(allocStr(ptr, -1),
				   NULL, (ListItem *)hist->list->last);
    if (hist->list->last)
	hist->list->last->next = item;
    else
	hist->list->first = item;
    hist->list->last = item;
    hist->list->nitem++;
    return item;
}

/* Don't mix pushHashHist() and pushHist()/unshiftHist(). */

HistItem *
pushHashHist(Hist *hist, const char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL ||
	hist->list->nitem >= HIST_LIST_MAX)
	return NULL;
    item = getHashHist(hist, ptr);
    if (item) {
	if (item->next)
	    item->next->prev = item->prev;
	else			/* item == hist->list->last */
	    hist->list->last = item->prev;
	if (item->prev)
	    item->prev->next = item->next;
	else			/* item == hist->list->first */
	    hist->list->first = item->next;
	hist->list->nitem--;
    }
    item = pushHist(hist, ptr);
    putHash_sv(hist->hash, ptr, item);
    return item;
}

HistItem *
getHashHist(Hist *hist, const char *ptr)
{
    HistItem *item;

    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->hash == NULL) {
	hist->hash = newHash_sv(HIST_HASH_SIZE);
	for (item = hist->list->first; item; item = item->next)
	    putHash_sv(hist->hash, item->ptr, item);
    }
    return (HistItem *)getHash_sv(hist->hash, ptr, NULL);
}

char *
lastHist(Hist *hist)
{
    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->list->last) {
	hist->current = hist->list->last;
	return (char *)hist->current->ptr;
    }
    return NULL;
}

char *
nextHist(Hist *hist)
{
    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->current && hist->current->next) {
	hist->current = hist->current->next;
	return (char *)hist->current->ptr;
    }
    return NULL;
}

char *
prevHist(Hist *hist)
{
    if (hist == NULL || hist->list == NULL)
	return NULL;
    if (hist->current && hist->current->prev) {
	hist->current = hist->current->prev;
	return (char *)hist->current->ptr;
    }
    return NULL;
}
