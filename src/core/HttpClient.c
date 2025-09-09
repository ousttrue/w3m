#include "HttpClient.h"
#include "http.h"
// #include "ui.h"
#include <openssl/ssl.h>
#include <unistd.h>

void initHttpClient(struct HttpClient* c)
{
    c->status = HTST_NORMAL,
    c->nredir = 0;
    // c->url = path;
    c->page = NULL;
    c->content_type = "text/plain";
    c->charset = WC_CES_US_ASCII;
    c->current_content_length = 0;

    c->uname = NULL;
    c->pwd = NULL;
    c->realm = NULL;
    c->add_auth_cookie_flag = 0;
}

bool checkRedirection(struct HttpClient* c, struct Url* pu)
{
    if (c->nredir >= FollowRedirection) {
        Str tmp = Sprintf("Number of redirections exceeded %d at %s",
            FollowRedirection, parsedURL2Str(pu)->ptr);
        // message(getUI(), MSG_ERR, tmp->ptr);
        return false;
    }

    for (int i = 0; i < c->nredir; ++i) {
        if (same_url_p(pu, &c->puv[i])) {
            Str tmp = Sprintf("Redirection loop detected (%s)", parsedURL2Str(pu)->ptr);
            // message(getUI(), MSG_ERR, tmp->ptr);
            return false;
        }
    }

    c->puv[c->nredir++] = copyParsedURL(pu);
    return true;
}
