#include "internal.h"
#include "maparea.h"
#include "rc.h"
#include "cookie.h"
#include "downloadlist.h"
#include "KeyValue.h"
#include <stdlib.h>
#include <strings.h>

typedef void (*FormActionFunc)(struct UI ui, struct KeyValue*);

struct FormAction {
    const char* action;
    FormActionFunc rout;
};

static void change_charset(struct UI ui, struct KeyValue* arg)
{
    abort();
    // struct Buffer* buf = ui.current_buffer->linkBuffer[LB_N_INFO];
    // if (buf == NULL)
    //     return;
    // delBuffer(ui, ui.current_buffer);
    // ui.current_buffer = buf;
    // if (ui.current_buffer->bufferprop & BP_INTERNAL)
    //     return;
    // wc_ces charset;
    // charset = ui.current_buffer->document.charset;
    // for (; arg; arg = arg->next) {
    //     if (!strcmp(arg->arg, "charset"))
    //         charset = atoi(arg->value);
    // }
    // _docCSet(ui, charset);
}

struct FormAction internal_action[] = {
    { "map", follow_map },
    { "option", panel_set_option },
    { "cookie", set_cookie_flag },
    { "download", download_action },
    { "charset", change_charset },
    { "none", NULL },
    { NULL, NULL },
};

void do_internal(struct UI ui, const char* action, const char* data)
{
    for (int i = 0; internal_action[i].action; i++) {
        if (strcasecmp(internal_action[i].action, action) == 0) {
            if (internal_action[i].rout)
                internal_action[i].rout(ui, cgistr2tagarg(data));
            return;
        }
    }
}
