#include <sstream>
#include <plog/Log.h>
#include "stream/http.h"
#include "stream/url.h"
#include "loader.h"
#include "stream/cookie.h"
#include "stream/input_stream.h"
#include "stream/compression.h"

#include "html/form.h"
#include "frontend/display.h"
#include "stream/network.h"
#include "w3m.h"
#include "file.h"
#include "mime/mimetypes.h"
#include "mime/mailcap.h"
#include <myctype.h>
#include <string_view_util.h>

void _Push(const HttpRequestPtr &request, std::stringstream &ss)
{
    // stop recursion
}

template <typename... ARGS>
void _Push(const HttpRequestPtr &request, std::stringstream &ss, Str s, ARGS... args)
{
    ss << s->ptr;
    _Push(request, ss, args...);
}

template <typename... ARGS>
void _Push(const HttpRequestPtr &request, std::stringstream &ss, std::string_view s, ARGS... args)
{
    ss << s;
    _Push(request, ss, args...);
}

template <typename T, typename... ARGS>
void _Push(const HttpRequestPtr &request, std::stringstream &ss, const T &t, ARGS... args)
{
    ss << t;
    _Push(request, ss, args...);
}

template <typename... ARGS>
void Push(const HttpRequestPtr &request, ARGS... args)
{
    std::stringstream ss;
    _Push(request, ss, args...);
    request->lines.push_back(ss.str());
}

///
/// HttpRequest
///
std::shared_ptr<HttpRequest> HttpRequest::Create(const URL &url, FormPtr form)
{
    auto request = std::make_shared<HttpRequest>();
    request->url = url;
    request->form = form;
    if (form && form->method == FORM_METHOD_POST && form->body)
        request->method = HTTP_METHOD_POST;
    if (form && form->method == FORM_METHOD_HEAD)
        request->method = HTTP_METHOD_HEAD;

    // FIRST LINE => GET / HTTP/1.1
    auto host = url.ToStr(true, false);
    std::stringstream ss;
    ss
        << request->Method()
        << " "
        << host->ptr
        << " HTTP/1.0\r\n";
    request->lines.push_back(ss.str());

    return request;
}

///
/// HttpResponse
///
std::shared_ptr<HttpResponse> HttpResponse::Read(const std::shared_ptr<InputStream> &stream, const URL &url)
{
    auto res = std::make_shared<HttpResponse>();

    while (!stream->eos())
    {
        auto line = stream->mygets();
        if (res->PushIsEndHeader(line->ptr, url))
        {
            break;
        }
    }

    return res;
}

bool HttpResponse::PushIsEndHeader(std::string_view line, const URL &url)
{
    if (line.ends_with("\r\n"))
    {
        line.remove_suffix(2);
    }

    if (line.empty())
    {
        // empty line finish HTTP response header
        return true;
    }

    if (lines.empty())
    {
        LOGD << line;
        lines.push_back(std::string(line));

        // first line
        // HTTP/1.1 200 OK
        if (!line.starts_with("HTTP/"))
        {
            // invalid http response. abort
            return true;
        }

        if (line[6] != '.')
        {
            return true;
        }

        version_major = line[5] - '0';
        if (version_major != 1)
        {
            // unknown http version
            return true;
        }
        version_minor = line[7] - '0';
        if (version_minor != 0 && version_minor != 1)
        {
            // unknown http version
            return true;
        }

        status_code = (HttpResponseStatusCode)atoi(line.data() + 9);
    }
    else
    {
        lines.push_back(std::string(line));
        // KEY: VALUE
        // ex. Content-Type: text/html
        auto [key, value] = svu::split(line, ':');

        // headers
        if (svu::ic_eq(key, "content-encoding"))
        {
            content_encoding = get_compression_type(value);
        }
        else if (svu::ic_eq(key, "content-type"))
        {
            content_type = value;
            auto [tt, charset] = svu::split(value, ';');
            if (charset.size())
            {
                // has charset
                content_type = tt;
                auto [_, cs] = svu::split(charset, '=');
                if (cs.size())
                {
                    content_charset = cs;
                }
            }
        }
        else if (svu::ic_eq(key, "set-cookie"))
        {
            CookieManager::Instance().ProcessHttpHeader(url, false, value);
        }
        else if (svu::ic_eq(key, "set-cookie2"))
        {
            CookieManager::Instance().ProcessHttpHeader(url, true, value);
        }
    }

    // header is continuous
    return false;
}

//         else if (strncasecmp(lineBuf2->ptr, "w3m-control:", 12) == 0 &&
//                  uf->scheme == SCM_LOCAL_CGI)
//         {
//             auto p = lineBuf2->ptr + 12;
//             SKIP_BLANKS(&p);
//             Str funcname = Strnew();
//             while (*p && !IS_SPACE(*p))
//                 funcname->Push(*(p++));
//             SKIP_BLANKS(&p);
//             Command f = getFuncList(funcname->ptr);
//             if (f)
//             {
//                 auto tmp = Strnew(p);
//                 StripRight(tmp);
//                 pushEvent(f, tmp->ptr);
//             }
//         }

std::string_view HttpResponse::FindHeader(std::string_view key) const
{
    for (auto &l : lines)
    {
        auto [k, v] = svu::split(l, ':');
        if (svu::ic_eq(k, key))
        {
            return v;
        }
    }

    return {};
}

///
/// HttpClient
///
std::tuple<InputStreamPtr, HttpResponsePtr> HttpClient::GetResponse(const URL &url,
                                                                    const URL *base, HttpReferrerPolicy referer,
                                                                    FormPtr form)
{
    //
    // build HTTP request
    //
    auto request = HttpRequest::Create(url, form);
    Push(request, "User-Agent: ", (Network::Instance().UserAgent.empty() ? w3mApp::w3m_version : Network::Instance().UserAgent), "\r\n");
    Push(request, "Accept: ", Network::Instance().AcceptMedia, "\r\n");
    Push(request, "Accept-Encoding: ", Network::Instance().AcceptEncoding, "\r\n");
    Push(request, "Accept-Language: ", Network::Instance().AcceptLang, "\r\n");

    if (url.host.size())
    {
        Push(request, "Host: ", url.host, url.NonDefaultPort(), "\r\n");
    }
    if (url.is_nocache || Network::Instance().NoCache)
    {
        Push(request, "Pragma: no-cache\r\n");
        Push(request, "Cache-control: no-cache\r\n");
    }

    Str cookie;
    if (request->method != HTTP_METHOD_CONNECT &&
        CookieManager::Instance().use_cookie && (cookie = find_cookie(url)))
    {
        Push(request, "Cookie: ", cookie->c_str(), "\r\n");
        /* [DRAFT 12] s. 10.1 */
        if (cookie->ptr[0] != '$')
        {
            Push(request, "Cookie2: $Version=\"1\"\r\n");
        }
    }

    if (request->method == HTTP_METHOD_POST)
    {
        if (request->form->enctype == FORM_ENCTYPE_MULTIPART)
        {
            Push(request, "Content-type: multipart/form-data; boundary=", request->form->boundary, "\r\n");
            Push(request, "Content-length: ", request->form->length, "\r\n");
        }
        else
        {
            if (w3mApp::Instance().override_content_type)
            {
                Push(request, "Content-type: application/x-www-form-urlencoded\r\n");
            }
            Push(request, "Content-length: ", request->form->length, "\r\n");

            // ?
            if (w3mApp::Instance().header_string.size())
            {
                Push(request, w3mApp::Instance().header_string);
            }
        }
    }
    else
    {
        // ?
        if (w3mApp::Instance().header_string.size())
        {
            Push(request, w3mApp::Instance().header_string);
        }
    }

    if (!Network::Instance().NoSendReferer)
    {
        if (referer == HttpReferrerPolicy::StrictOriginWhenCrossOrigin)
        {
            if (base && base->scheme == url.scheme)
            {
                // strict(same scheme)
                if (url.HasSameOrigin(*base))
                {
                    Push(request, "Referer: ", base->ToReferer(), "\r\n");
                }
                else
                {
                    Push(request, "Referer: ", base->ToRefererOrigin(), "\r\n");
                }
            }
        }
        else
        {
            /* Don't send Referer: if https:// -> http:// */
        }
    }

    //
    // open stream and send request
    //
    auto stream = OpenHttpAndSendRequest(request);
    if (!stream)
    {
        // fail to open stream
        return {};
    }
    show_message(Strnew_m_charp(url.host, " contacted. Waiting for reply...")->ptr);

    //
    // read HTTP response headers
    //
    auto response = HttpResponse::Read(stream, url);
    show_message(response->lines.front());
    exchanges.push_back({request, response});

    //
    // redirection
    //
    if (response->HasRedirectionStatus())
    {
        auto location = response->FindHeader("Location");
        if (location.empty())
        {
            return {};
        }

        // redirect
        auto new_url = URL::Parse(location, nullptr);
        LOGD << "redirect to:" << location;
        return GetResponse(new_url, nullptr, referer, nullptr);
    }

    return {stream, response};
}

ContentStreamPtr HttpClient::GetStream(const URL &url, const URL *base, HttpReferrerPolicy referer, FormPtr form)
{
    auto [stream, response] = GetResponse(url, base, referer, form);

    //
    // read HTTP response body
    //
    // if (t == NULL && url.path.size())
    // {
    //     if (!((http_response_code >= 400 && http_response_code <= 407) ||
    //           (http_response_code >= 500 && http_response_code <= 505)))
    //         t = guessContentType(url.path);
    // }

    if (response->content_encoding != CMP_NOCOMPRESS)
    {
        // if (!(w3mApp::Instance().w3m_dump & DUMP_SOURCE) &&
        //     (w3mApp::Instance().w3m_dump & ~DUMP_FRAME || is_text_type(t) || searchExtViewer(t)))
        // {
        //     // if (t_buf == NULL)
        //     /*t_buf->sourcefile =*/uncompress_stream(f, true);
        //     auto [type, ext] = uncompressed_file_type(url.path);
        //     if(ext.size())
        //     {
        //         f->ext = ext.data();
        //     }
        // }
        // else
        // {
        //     t = compress_application_type(f->compression).data();
        //     f->compression = CMP_NOCOMPRESS;
        // }
        stream = decompress(stream, response->content_encoding);
        assert(stream);
    }

    return std::make_shared<ContentStream>(exchanges.back().request->url, stream, response->content_type, response->content_charset);

    // auto proc = loadBuffer;
    // else if (ImageManager::Instance().activeImage && ImageManager::Instance().displayImage && !useExtImageViewer &&
    //          !(w3mApp::Instance().w3m_dump & ~DUMP_FRAME) && t.starts_with("image/"))
    //     proc = loadImageBuffer;
    // else if (w3mApp::Instance().w3m_backend)
    // ;

    // frame_source = flag & RG_FRAME_SRC;
    // auto b = loadSomething(f, url.real_file.size() ? const_cast<char *>(url.real_file.c_str()) : const_cast<char *>(url.path.c_str()), proc);
    // f->stream = nullptr;
    // frame_source = 0;
    // if (b)
    // {
    //     b->real_scheme = f->scheme;
    //     b->real_type = t;
    //     if (b->currentURL.host.empty() && b->currentURL.path.empty())
    //         b->currentURL = url;
    //     if (is_html_type(t))
    //         b->type = "text/html";
    //     else if (w3mApp::Instance().w3m_backend)
    //     {
    //         Str s = Strnew(t);
    //         b->type = s->ptr;
    //     }
    //     else if (proc == loadImageBuffer)
    //         b->type = "text/html";
    //     else
    //         b->type = "text/plain";
    //     if (url.fragment.size())
    //     {
    //         if (proc == loadHTMLBuffer)
    //         {
    //             auto a = searchURLLabel(b, const_cast<char *>(url.fragment.c_str()));
    //             if (a != NULL)
    //             {
    //                 b->Goto(a->start, label_topline);
    //             }
    //         }
    //         else
    //         { /* plain text */
    //             int l = atoi(url.fragment.c_str());
    //             b->GotoRealLine(l);
    //             b->pos = 0;
    //             b->ArrangeCursor();
    //         }
    //     }
    // }
    // if (w3mApp::Instance().header_string.size())
    //     w3mApp::Instance().header_string.clear();
    // preFormUpdateBuffer(b);
    // return b;
}
