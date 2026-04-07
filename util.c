#include "util.h"
#include "display.h"
#include "term_tty.h"
#include <stdio.h>
#include <stdlib.h>

int exec_cmd(const char* cmd)
{
    int rv;

    fmTerm();
    if ((rv = system(cmd))) {
        printf("\n[Hit any key]");
        fflush(stdout);
        fmInit();
        getch();

        return rv;
    }
    fmInit();

    return 0;
}
