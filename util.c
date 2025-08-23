#include "util.h"
#include "display.h"
#include "event_poller.h"
#include <stdio.h>
#include <stdlib.h>

int exec_cmd(char* cmd)
{
    fmTerm();
    int rv = system(cmd);
    if (rv) {
        printf("\n[Hit any key]");
        fflush(stdout);
        fmInit();
        {
            GetChFunc getch = event_begin_input(-1);
            getch();
            event_end_input(getch);
        }

        return rv;
    }
    fmInit();

    return 0;
}
