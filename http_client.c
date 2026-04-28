#include "http_client.h"

static int same_url_p(struct Url* pu1, struct Url* pu2)
{
    return (pu1->scheme == pu2->scheme && pu1->port == pu2->port && (pu1->host ? pu2->host ? !strcasecmp(pu1->host, pu2->host) : 0 : 1)
        && (pu1->file ? pu2->file ? !strcmp(pu1->file, pu2->file) : 0 : 1));
}

void http_init(struct HttpClient* http)
{
    http->session_count = 0;
    memset(http->message_sessions, 0, sizeof(http->message_sessions));
}

enum HttpReidrectionStatus http_redirect(struct HttpClient* http, struct Url url)
{
    if (http->session_count + 1 >= FollowRedirection) {
        return HTTP_REDIRECTION_EXCEEDED;
    }

    for (int i = 0; i < http->session_count; ++i) {
        if (same_url_p(&http->message_sessions[i], &url)) {
            return HTTP_REDIRECTION_LOOP_DETECTED;
        }
    }

    // if (nredir >= FollowRedirection) {
    //     Str tmp = Sprintf("Number of redirections exceeded %d at %s",
    //         FollowRedirection, parsedURL2Str(pu)->ptr);
    //     disp_err_message(args, tmp->ptr, FALSE);
    //     return FALSE;
    // }

    // if ((same_url_p(pu, &puv[(nredir - 1) % nredir_size]) || (!(nredir % 2) && same_url_p(pu, &puv[(nredir / 2) % nredir_size])))) {
    //     /* FIXME: gettextize? */
    //     tmp = Sprintf("Redirection loop detected (%s)",
    //         parsedURL2Str(pu)->ptr);
    //     disp_err_message(args, tmp->ptr, FALSE);
    //     return FALSE;
    // }

    http->message_sessions[http->session_count++] = url;

    // if (!puv) {
    // puv = New_N(struct Url, nredir_size);
    // memset(puv, 0, sizeof(struct Url) * nredir_size);
    // }
    // copyParsedURL(&puv[nredir % nredir_size], pu);

    return HTTP_REDIRECTION_OK;
}
