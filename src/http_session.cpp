#include "http_session.h"
#include "url_quote.h"
#include "http_response.h"
#include "mytime.h"
#include "http_auth.h"
#include "mimetypes.h"
#include <sys/stat.h>

const char *DefaultType = nullptr;

std::shared_ptr<HttpResponse> loadGeneralFile(std::string_view path,
                                              std::optional<Url> current,
                                              const HttpOption &option,
                                              FormList *request) {
  auto res = std::make_shared<HttpResponse>();

  auto hr = res->f.openURL(path, current, option, request);

  // Directory Open ?
  // if (f.stream == nullptr) {
  //   switch (f.schema) {
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

  if (hr->url.schema == SCM_HTTP || hr->url.schema == SCM_HTTPS) {

    // if (fmInitialized) {
    //   term_cbreak();
    //   message(
    //       Sprintf("%s contacted. Waiting for reply...",
    //       hr->url.host.c_str())->ptr, 0, 0);
    //   refresh(term_io());
    // }
    auto http_response_code = res->readHeader(&res->f, hr->url);
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
          /* abort */
          // TRAP_OFF;
          res->page_loaded(hr->url);
          return res;
        }
        hr->add_auth_cookie_flag = true;
        return loadGeneralFile(path, current, option, request);
      }
    }

    if (hr && hr->status == HTST_CONNECT) {
      return loadGeneralFile(path, current, option, request);
    }

    res->f.modtime = mymktime(res->getHeader("Last-Modified:"));
  } else if (hr->url.schema == SCM_DATA) {
    res->type = res->f.guess_type;
  } else if (DefaultType) {
    res->type = DefaultType;
    DefaultType = nullptr;
  } else {
    res->type = guessContentType(hr->url.file.c_str());
    if (res->type.empty()) {
      res->type = "text/plain";
    }
    if (res->f.guess_type.size()) {
      res->type = res->f.guess_type;
    }
  }

  /* XXX: can we use guess_type to give the type to loadHTMLstream
   *      to support default utf8 encoding for XHTML here? */
  res->f.guess_type = res->type;

  res->page_loaded(hr->url);
  return res;
}
