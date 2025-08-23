#include "w3m.h"
#include "display.h"
#include "fm.h"

int main(int argc, char** argv)
{
    const char* line_str = parseArgs(argc, argv);
    displayBuffer(Currentbuf, B_FORCE_REDRAW);
    if (line_str) {
        _goLine(line_str);
    }

    return main_loop();
}
