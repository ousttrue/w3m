#include "dict.h"
#include "w3m_rc.h"
#include "message.h"
#include "indep.h"
#include "Str.h"
#include "tab_list.h"
#include "tab.h"
#include "buffer.h"
#include "content.h"
#include "document.h"

#define DICTBUFFERNAME "*dictionary*"

void execdict(const char* word)
{
    if (!getRuntime()->UseDictCommand || word == NULL || *word == '\0') {
        return;
    }
    char* w = conv_to_system(word);
    if (*w == '\0') {
        return;
    }

    Str dictcmd = Sprintf("%s?%s", getRuntime()->DictCommand, Str_form_quote(Strnew_charp(w))->ptr);

    struct Content* content = get_content_cache(dictcmd->ptr, NULL,
        (struct LoadOption) { .base_url = NULL, .referer = NO_REFERER, .flag = 0 });
    if (!content) {
        disp_message("Execution failed", TRUE);
        return;
    }

    struct Buffer* buf = buf_new(content);
    buf->content->filename = w;
    buf->doc->title = Sprintf("%s %s", DICTBUFFERNAME, word)->ptr;
    if (buf->content->content_type == NULL)
        buf->content->content_type = "text/plain";
    tab_push_buffer(CurrentTab(), buf);
}
