#include "http_session.h"
#include "url_quote.h"
#include "http_response.h"
#include "mytime.h"
#include "http_auth.h"
#include "mimetypes.h"
#include "url_stream.h"
#include <sys/stat.h>

const char *DefaultType = nullptr;
bool UseExternalDirBuffer = true;
std::string DirBufferCommand = "file:///$LIB/dirlist" CGI_EXTENSION;

std::shared_ptr<HttpResponse> loadGeneralFile(const std::string &path,
                                              std::optional<Url> current,
                                              const HttpOption &option,
                                              const std::shared_ptr<Form> &request) {
  auto res = std::make_shared<HttpResponse>();

  auto [hr, stream] = openURL(path, current, option, request);

  // Directory Open ?
  // if (f.stream == nullptr) {
  //   switch (f.scheme) {
  //   case SCM_LOCAL: {
  //     struct stat st;
  //     if (stat(pu.real_file.c_str(), &st) < 0)
  //       return nullptr;
  //     if (S_ISDIR(st.st_mode)) {
  //       if (UseExternalDirBuffer) {
  //         Str *cmd =
  //             Sprintf("%s?dir=%s#current", DirBufferCommand,
  //             pu.file.c_str());
  //         auto b = loadGeneralFile(cmd->ptr, {}, {.referer = NO_REFERER});
  //         if (b != nullptr && b != NO_BUFFER) {
  //           b->info->currentURL = pu;
  //           b->info->filename = Strnew(b->info->currentURL.real_file)->ptr;
  //         }
  //         return b;
  //       } else {
  //         page = loadLocalDir(pu.real_file.c_str());
  //         t = "local:directory";
  //       }
  //     }
  //   } break;
  //
  //   case SCM_UNKNOWN:
  //     disp_err_message(Sprintf("Unknown URI: %s", pu.to_Str().c_str())->ptr,
  //                      false);
  //     break;
  //
  //   default:
  //     break;
  //   }
  //   if (page && page->length > 0)
  //     return page_loaded();
  //   return nullptr;
  // }

  if (hr && hr->status == HTST_MISSING) {
    return nullptr;
  }

  if (hr->url.scheme == SCM_HTTP || hr->url.scheme == SCM_HTTPS) {

    // if (fmInitialized) {
    //   term_cbreak();
    //   message(
    //       Sprintf("%s contacted. Waiting for reply...",
    //       hr->url.host.c_str())->ptr, 0, 0);
    //   refresh(term_io());
    // }
    auto http_response_code = res->readHeader(stream, hr->url);
    const char *p;
    if (((http_response_code >= 301 && http_response_code <= 303) ||
         http_response_code == 307) &&
        (p = res->getHeader("Location:")) != nullptr &&
        res->checkRedirection(hr->url)) {
      /* document moved */
      /* 301: Moved Permanently */
      /* 302: Found */
      /* 303: See Other */
      /* 307: Temporary Redirect (HTTP/1.1) */
      return loadGeneralFile(url_quote(p), hr->url, option, {});
    }

    res->type = res->checkContentType();
    if (res->type.empty() && hr->url.file.size()) {
      if (!((http_response_code >= 400 && http_response_code <= 407) ||
            (http_response_code >= 500 && http_response_code <= 505)))
        res->type = guessContentType(hr->url.file.c_str());
    }
    if (res->type.empty()) {
      res->type = "text/plain";
    }
    hr->add_auth_cookie();
    if ((p = res->getHeader("WWW-Authenticate:")) &&
        http_response_code == 401) {
      /* Authentication needed */
      struct http_auth hauth;
      if (findAuthentication(&hauth, *res, "WWW-Authenticate:") &&
          (hr->realm = get_auth_param(hauth.param, "realm"))) {
        getAuthCookie(&hauth, "Authorization:", hr->url, hr.get(), request,
                      &hr->uname, &hr->pwd);
        if (hr->uname == nullptr) {
          res->page_loaded(hr->url, stream);
          return res;
        }
        hr->add_auth_cookie_flag = true;
        return loadGeneralFile(path, current, option, request);
      }
    }

    if (hr && hr->status == HTST_CONNECT) {
      return loadGeneralFile(path, current, option, request);
    }

    /*f.modtime =*/mymktime(res->getHeader("Last-Modified:"));
  } else if (hr->url.scheme == SCM_DATA) {
    // res->type = f.guess_type;
  } else if (DefaultType) {
    res->type = DefaultType;
    DefaultType = nullptr;
  } else {
    res->type = guessContentType(hr->url.file.c_str());
    if (res->type.empty()) {
      res->type = "text/plain";
    }
    // if (f.guess_type.size()) {
    //   res->type = f.guess_type;
    // }
  }

  /* XXX: can we use guess_type to give the type to loadHTMLstream
   *      to support default utf8 encoding for XHTML here? */
  // f.guess_type = res->type;

  // res->ssl_certificate = f.ssl_certificate;
  res->page_loaded(hr->url, stream);
  return res;
}
