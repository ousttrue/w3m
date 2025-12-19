#include "util.h"
#include "w3m_rc.h"

#include <stdio.h>
#include <stdlib.h>

int exec_cmd(char* cmd)
{
    exitRawMode();
    int rv = system(cmd);
    if (rv == 0) {
        // success
        enterRawMode();
        return 0;
    } else {
        // error
        printf("\n[Hit any key]");
        fflush(stdout);
        enterRawMode();
        getch();
        return rv;
    }
}
