#include "line_input.h"
#include <w3m.h>
#include "LineInput.h"
#include "term_tty.h"
#include "global.h"
#include "screen.h"
#include "tab.h"
#include "Str.h"
#include "display.h"
#include <stdio.h>

const char* inputLineHistSearch(struct CmdArgs* args, const char* prompt, const char* def_str,
    enum InputLineFlags flag, enum HistoryType hist, IncrFunc incrfunc)
{
    struct LineInputResult res = do_lineinput(args, prompt, def_str, flag, hist, incrfunc);

    if (CurrentTab) {
        if (res.need_redraw)
            displayBuffer(args, B_FORCE_REDRAW);
    }

    if (res.i_broken) {
        return NULL;
    }

    sc_move((LINES - 1), 0);
    tty_write_sc();

    return res.str;
}

const char* inputAnswer(struct CmdArgs* args, const char* prompt)
{
    if (QuietMessage)
        return "n";

    if (fmInitialized) {
        tty_raw();
        return inputChar(args, prompt);
    } else {
        printf("%s", prompt);
        fflush(stdout);
        return Strfgets(stdin)->ptr;
    }
}
