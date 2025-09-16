#include "progress.h"
#include "screen.h"
#include "screen_effects.h"
#include "w3m.h"
#include <Str.h>
#include <math.h>
#include <time.h>

static char* _size_unit[] = {
    "b",
    "kb",
    "Mb",
    "Gb",
    "Tb",
    "Pb",
    "Eb",
    "Zb",
    "Bb",
    "Yb",
    NULL,
};

static char* convert_size2(long long size1, long long size2, int usefloat)
{
    char** sizes = _size_unit;
    float csize, factor = 1;
    int sizepos = 0;

    csize = (float)((size1 > size2) ? size1 : size2);
    while (csize / factor >= 999.495 && sizes[sizepos + 1]) {
        factor *= 1024.0;
        sizepos++;
    }
    return Sprintf(usefloat ? "%.3g/%.3g%s" : "%.0f/%.0f%s",
        floor(size1 / factor * 100.0 + 0.5) / 100.0,
        floor(size2 / factor * 100.0 + 0.5) / 100.0,
        sizes[sizepos])
        ->ptr;
}

char* convert_size(long long size, int usefloat)
{
    float csize;
    int sizepos = 0;
    char** sizes = _size_unit;

    csize = (float)size;
    while (csize >= 999.495 && sizes[sizepos + 1]) {
        csize = csize / 1024.0;
        sizepos++;
    }
    return Sprintf(usefloat ? "%.3g%s" : "%.0f%s",
        floor(csize * 100.0 + 0.5) / 100.0, sizes[sizepos])
        ->ptr;
}


void showProgress(struct UI ui, long long current_content_length, long long* linelen, long long* trbyte)
{
    if (*linelen < 1024)
        return;

    struct VirtualTerm* vt = getScreen();
    int i, j, rate, duration, eta, pos;
    static time_t last_time, start_time;
    time_t cur_time;
    Str messages;
    char *fmtrbyte, *fmrate;
    if (current_content_length > 0) {
        double ratio;
        cur_time = time(0);
        if (*trbyte == 0) {
            vt_move(vt, getScreen()->ROWS - 1, 0);
            vt_clrtoeolx(vt);
            start_time = cur_time;
        }
        *trbyte += *linelen;
        *linelen = 0;
        if (cur_time == last_time)
            return;
        last_time = cur_time;
        vt_move(vt, getScreen()->ROWS - 1, 0);
        ratio = 100.0 * (*trbyte) / current_content_length;
        fmtrbyte = convert_size2(*trbyte, current_content_length, 1);
        duration = cur_time - start_time;
        if (duration) {
            rate = *trbyte / duration;
            fmrate = convert_size(rate, 1);
            eta = rate ? (current_content_length - *trbyte) / rate : -1;
            messages = Sprintf("%11s %3.0f%% "
                               "%7s/s "
                               "eta %02d:%02d:%02d     ",
                fmtrbyte, ratio,
                fmrate,
                eta / (60 * 60), (eta / 60) % 60, eta % 60);
        } else {
            messages = Sprintf("%11s %3.0f%%                          ",
                fmtrbyte, ratio);
        }
        vt_addstr(vt, messages->ptr);
        pos = 42;
        i = pos + (getScreen()->COLS - pos - 1) * (*trbyte) / current_content_length;
        vt_move(vt, getScreen()->ROWS - 1, pos);
        vt_standout(vt);
        vt_addch(vt, ' ');
        for (j = pos + 1; j <= i; j++)
            vt_addch(vt, '|');
        vt_standend(vt);
        /* no_clrtoeol(); */
        // refresh(ttyWriter());
    } else {
        cur_time = time(0);
        if (*trbyte == 0) {
            vt_move(vt, getScreen()->ROWS - 1, 0);
            vt_clrtoeolx(vt);
            start_time = cur_time;
        }
        *trbyte += *linelen;
        *linelen = 0;
        if (cur_time == last_time)
            return;
        last_time = cur_time;
        vt_move(vt, getScreen()->ROWS - 1, 0);
        fmtrbyte = convert_size(*trbyte, 1);
        duration = cur_time - start_time;
        if (duration) {
            fmrate = convert_size(*trbyte / duration, 1);
            messages = Sprintf("%7s loaded %7s/s", fmtrbyte, fmrate);
        } else {
            messages = Sprintf("%7s loaded", fmtrbyte);
        }
        message(ui, MSG_INFO, messages->ptr);
        // refresh(ttyWriter());
    }
}
