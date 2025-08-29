#include "ui.h"
#include "screen.h"

struct UI getUI()
{
    struct UI ui = {
        .vt = getScreen(),
    };
    return ui;
}
