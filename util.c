#include "util.h"
#include "display.h"
#include <w3m.h>
#include <stdio.h>
#include <stdlib.h>

int exec_cmd(struct CmdArgs args, const char* cmd)
{
    fmTerm();
    int rv = system(cmd);
    if (rv) {
        printf("\n[Hit any key]");
        fflush(stdout);
        fmInit();
        getch(args);
        return rv;
    }
    fmInit();
    return 0;
}
