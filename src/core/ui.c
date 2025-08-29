#include "ui.h"
#include "screen.h"
#include "term_size.h"

struct UI getUI()
{
    struct UI ui = {
        .vt = getScreen(),
        .rows = getLines(),
        .cols = getCols(),
    };
    return ui;
}
