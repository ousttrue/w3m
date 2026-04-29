#include "util.h"
#include "main.h"
#include <w3m.h>
#include <stdio.h>
#include <stdlib.h>

int exec_cmd(struct CmdArgs* args, const char* cmd)
{
    tty_deinit();
    int rv = system(cmd);
    if (rv) {
        printf("\n[Hit any key]");
        fflush(stdout);
        tty_init();
        getch(args);
    } else {
        tty_init();
    }
    return rv;
}
