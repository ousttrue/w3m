#include "option.h"
#define N_(Text) Text
#include <sstream>
#include "Str.h"
#include "html/table.h"
#include "html/html_quote.h"

int param_ptr::setParseValue(const std::string &value) {
  //   double ppc;
  //   switch (p->type) {
  //   case P_INT:
  //     if (atoi(value.c_str()) >= 0)
  //       *(int *)p->varptr = (p->inputtype == PI_ONOFF)
  //                               ? str_to_bool(value.c_str(), *(int
  //                               *)p->varptr) : atoi(value.c_str());
  //     break;
  //
  //   case P_NZINT:
  //     if (atoi(value.c_str()) > 0)
  //       *(int *)p->varptr = atoi(value.c_str());
  //     break;
  //
  //   case P_SHORT:
  //     *(short *)p->varptr = (p->inputtype == PI_ONOFF)
  //                               ? str_to_bool(value.c_str(), *(short
  //                               *)p->varptr) : atoi(value.c_str());
  //     break;
  //
  //   case P_CHARINT:
  //     *(char *)p->varptr = (p->inputtype == PI_ONOFF)
  //                              ? str_to_bool(value.c_str(), *(char
  //                              *)p->varptr) : atoi(value.c_str());
  //     break;
  //
  //   case P_CHAR:
  //     *(char *)p->varptr = value[0];
  //     break;
  //
  //   case P_STRING:
  //     *(const char **)p->varptr = Strnew(value)->ptr;
  //     break;
  //
  // #if defined(USE_SSL) && defined(USE_SSL_VERIFY)
  //   case P_SSLPATH:
  //     if (value.size())
  //       *(const char **)p->varptr = rcFile(value.c_str());
  //     else
  //       *(char **)p->varptr = NULL;
  //     ssl_path_modified = 1;
  //     break;
  // #endif
  //
  //   case P_PIXELS:
  //     ppc = atof(value.c_str());
  //     if (ppc >= MINIMUM_PIXEL_PER_CHAR && ppc <= MAXIMUM_PIXEL_PER_CHAR * 2)
  //       *(double *)p->varptr = ppc;
  //     break;
  //
  //   case P_SCALE:
  //     ppc = atof(value.c_str());
  //     if (ppc >= 10 && ppc <= 1000)
  //       *(double *)p->varptr = ppc;
  //     break;
  //   }

  return 1;
}

std::string param_ptr::to_str() const {
  // switch (this->type) {
  // case P_INT:
  // case P_NZINT:
  //   return Sprintf("%d", *(int *)this->varptr)->ptr;
  // case P_SHORT:
  //   return Sprintf("%d", *(short *)this->varptr)->ptr;
  // case P_CHARINT:
  //   return Sprintf("%d", *(char *)this->varptr)->ptr;
  // case P_CHAR:
  //   return Sprintf("%c", *(char *)this->varptr)->ptr;
  // case P_STRING:
  // case P_SSLPATH:
  //   /*  SystemCharset -> InnerCharset */
  //   return Strnew_charp(*(char **)this->varptr)->ptr;
  // case P_PIXELS:
  // case P_SCALE:
  //   return Sprintf("%g", *(double *)this->varptr)->ptr;
  // }
  /* not reached */
  return "";
}

Option::Option() {
  std::list<param_ptr> params1 = {
      // {"tabstop", P_NZINT, PI_TEXT, (void *)&Tabstop, CMT_TABSTOP, NULL},
      // {"indent_incr", P_NZINT, PI_TEXT, (void *)&IndentIncr, CMT_INDENT_INCR,
      //  NULL},
      // {"pixel_per_char", P_PIXELS, PI_TEXT, (void *)&pixel_per_char,
      //  CMT_PIXEL_PER_CHAR, NULL},
      // {"target_self", P_CHARINT, PI_ONOFF, (void *)&TargetSelf, CMT_TSELF,
      //  NULL},
      // {"open_tab_blank", P_INT, PI_ONOFF, (void *)&open_tab_blank,
      //  CMT_OPEN_TAB_BLANK, NULL},
      // {"open_tab_dl_list", P_INT, PI_ONOFF, (void *)&open_tab_dl_list,
      //  CMT_OPEN_TAB_DL_LIST, NULL},
      // {"display_link", P_INT, PI_ONOFF, (void *)&displayLink, CMT_DISPLINK,
      //  NULL},
      // {"display_link_number", P_INT, PI_ONOFF, (void *)&displayLinkNumber,
      //  CMT_DISPLINKNUMBER, NULL},
      // {"decode_url", P_INT, PI_ONOFF, (void *)&DecodeURL, CMT_DECODE_URL,
      // NULL},
      // {"display_lineinfo", P_INT, PI_ONOFF, (void *)&displayLineInfo,
      //  CMT_DISPLINEINFO, NULL},
      // {"ext_dirlist", P_INT, PI_ONOFF, (void *)&UseExternalDirBuffer,
      //  CMT_EXT_DIRLIST, NULL},
      // {"dirlist_cmd", P_STRING, PI_TEXT, (void *)&DirBufferCommand,
      //  CMT_DIRLIST_CMD, NULL},
      // {"use_dictcommand", P_INT, PI_ONOFF, (void *)&UseDictCommand,
      //  CMT_USE_DICTCOMMAND, NULL},
      // {"dictcommand", P_STRING, PI_TEXT, (void *)&DictCommand,
      // CMT_DICTCOMMAND,
      //  NULL},
      // {"multicol", P_INT, PI_ONOFF, (void *)&multicolList, CMT_MULTICOL,
      // NULL},
      // {"alt_entity", P_CHARINT, PI_ONOFF, (void *)&UseAltEntity,
      // CMT_ALT_ENTITY,
      //  NULL},
      // {"display_borders", P_CHARINT, PI_ONOFF, (void *)&DisplayBorders,
      //  CMT_DISP_BORDERS, NULL},
      // {"disable_center", P_CHARINT, PI_ONOFF, (void *)&DisableCenter,
      //  CMT_DISABLE_CENTER, NULL},
      // {"fold_textarea", P_CHARINT, PI_ONOFF, (void *)&FoldTextarea,
      //  CMT_FOLD_TEXTAREA, NULL},
      // {"display_ins_del", P_INT, PI_SEL_C, (void *)&displayInsDel,
      //  CMT_DISP_INS_DEL, displayinsdel},
      // {"ignore_null_img_alt", P_INT, PI_ONOFF, (void *)&ignore_null_img_alt,
      //  CMT_IGNORE_NULL_IMG_ALT, NULL},
      // {"view_unseenobject", P_INT, PI_ONOFF, (void *)&view_unseenobject,
      //  CMT_VIEW_UNSEENOBJECTS, NULL},
      // /* XXX: emacs-w3m force to off display_image even if image options off
      // */
      // {"display_image", P_INT, PI_ONOFF, (void *)&displayImage,
      // CMT_DISP_IMAGE,
      //  NULL},
      // {"pseudo_inlines", P_INT, PI_ONOFF, (void *)&pseudoInlines,
      //  CMT_PSEUDO_INLINES, NULL},
      // {"show_srch_str", P_INT, PI_ONOFF, (void *)&show_srch_str,
      //  CMT_SHOW_SRCH_STR, NULL},
      // {"label_topline", P_INT, PI_ONOFF, (void *)&label_topline,
      //  CMT_LABEL_TOPLINE, NULL},
      // {"nextpage_topline", P_INT, PI_ONOFF, (void *)&nextpage_topline,
      //  CMT_NEXTPAGE_TOPLINE, NULL},
  };

  std::list<param_ptr> params3 = {
      // {"use_history", P_INT, PI_ONOFF, (void *)&UseHistory, CMT_HISTORY,
      // NULL},
      // {"history", P_INT, PI_TEXT, (void *)&URLHistSize, CMT_HISTSIZE, NULL},
      // {"save_hist", P_INT, PI_ONOFF, (void *)&SaveURLHist, CMT_SAVEHIST,
      // NULL},
      // {"close_tab_back", P_INT, PI_ONOFF, (void *)&close_tab_back,
      //  CMT_CLOSE_TAB_BACK, NULL},
      // {"emacs_like_lineedit", P_INT, PI_ONOFF, (void *)&emacs_like_lineedit,
      //  CMT_EMACS_LIKE_LINEEDIT, NULL},
      // {"space_autocomplete", P_INT, PI_ONOFF, (void *)&space_autocomplete,
      //  CMT_SPACE_AUTOCOMPLETE, NULL},
      // {"mark_all_pages", P_INT, PI_ONOFF, (void *)&MarkAllPages,
      //  CMT_MARK_ALL_PAGES, NULL},
      // {"wrap_search", P_INT, PI_ONOFF, (void *)&WrapDefault, CMT_WRAP, NULL},
      // {"ignorecase_search", P_INT, PI_ONOFF, (void *)&IgnoreCase,
      //  CMT_IGNORE_CASE, NULL},
      // {"clear_buffer", P_INT, PI_ONOFF, (void *)&clear_buffer, CMT_CLEAR_BUF,
      //  NULL},
      // {"decode_cte", P_CHARINT, PI_ONOFF, (void *)&DecodeCTE, CMT_DECODE_CTE,
      //  NULL},
      // {"auto_uncompress", P_CHARINT, PI_ONOFF, (void *)&AutoUncompress,
      //  CMT_AUTO_UNCOMPRESS, NULL},
      // {"preserve_timestamp", P_CHARINT, PI_ONOFF, (void *)&PreserveTimestamp,
      //  CMT_PRESERVE_TIMESTAMP, NULL},
      // {"keymap_file", P_STRING, PI_TEXT, (void *)&keymap_file,
      // CMT_KEYMAP_FILE,
      //  NULL},
  };

  std::list<param_ptr> params4 = {
      // {"no_cache", P_CHARINT, PI_ONOFF, (void *)&NoCache, CMT_NO_CACHE,
      // NULL},
  };

  std::list<param_ptr> params5 = {
      // {"document_root", P_STRING, PI_TEXT, (void *)&document_root, CMT_DROOT,
      //  NULL},
      // {"personal_document_root", P_STRING, PI_TEXT,
      //  (void *)&personal_document_root, CMT_PDROOT, NULL},
      // {"cgi_bin", P_STRING, PI_TEXT, (void *)&cgi_bin, CMT_CGIBIN, NULL},
      // {"index_file", P_STRING, PI_TEXT, (void *)&index_file, CMT_IFILE,
      // NULL},
  };

  std::list<param_ptr> params6 = {
      // {"mime_types", P_STRING, PI_TEXT, (void *)&mimetypes_files,
      // CMT_MIMETYPES,
      //  NULL},
      // {"mailcap", P_STRING, PI_TEXT, (void *)&mailcap_files, CMT_MAILCAP,
      // NULL},
      // // {"editor", P_STRING, PI_TEXT, (void *)&Editor, CMT_EDITOR, NULL},
      // {"mailto_options", P_INT, PI_SEL_C, (void *)&MailtoOptions,
      //  CMT_MAILTO_OPTIONS, (void *)mailtooptionsstr},
      // {"extbrowser", P_STRING, PI_TEXT, (void *)&ExtBrowser, CMT_EXTBRZ,
      // NULL},
      // {"extbrowser2", P_STRING, PI_TEXT, (void *)&ExtBrowser2, CMT_EXTBRZ2,
      //  NULL},
      // {"extbrowser3", P_STRING, PI_TEXT, (void *)&ExtBrowser3, CMT_EXTBRZ3,
      //  NULL},
      // {"extbrowser4", P_STRING, PI_TEXT, (void *)&ExtBrowser4, CMT_EXTBRZ4,
      //  NULL},
      // {"extbrowser5", P_STRING, PI_TEXT, (void *)&ExtBrowser5, CMT_EXTBRZ5,
      //  NULL},
      // {"extbrowser6", P_STRING, PI_TEXT, (void *)&ExtBrowser6, CMT_EXTBRZ6,
      //  NULL},
      // {"extbrowser7", P_STRING, PI_TEXT, (void *)&ExtBrowser7, CMT_EXTBRZ7,
      //  NULL},
      // {"extbrowser8", P_STRING, PI_TEXT, (void *)&ExtBrowser8, CMT_EXTBRZ8,
      //  NULL},
      // {"extbrowser9", P_STRING, PI_TEXT, (void *)&ExtBrowser9, CMT_EXTBRZ9,
      //  NULL},
      // {"bgextviewer", P_INT, PI_ONOFF, (void *)&BackgroundExtViewer,
      //  CMT_BGEXTVIEW, NULL},
  };

  std::list<param_ptr> params7 = {
      // {"ssl_forbid_method", P_STRING, PI_TEXT, (void *)&ssl_forbid_method,
      //  CMT_SSL_FORBID_METHOD, NULL},
      // {"ssl_min_version", P_STRING, PI_TEXT, (void *)&ssl_min_version,
      //  CMT_SSL_MIN_VERSION, NULL},
      // {"ssl_cipher", P_STRING, PI_TEXT, (void *)&ssl_cipher, CMT_SSL_CIPHER,
      //  NULL},
      // {"ssl_verify_server", P_INT, PI_ONOFF, (void *)&ssl_verify_server,
      //  CMT_SSL_VERIFY_SERVER, NULL},
      // {"ssl_cert_file", P_SSLPATH, PI_TEXT, (void *)&ssl_cert_file,
      //  CMT_SSL_CERT_FILE, NULL},
      // {"ssl_key_file", P_SSLPATH, PI_TEXT, (void *)&ssl_key_file,
      //  CMT_SSL_KEY_FILE, NULL},
      // {"ssl_ca_path", P_SSLPATH, PI_TEXT, (void *)&ssl_ca_path,
      // CMT_SSL_CA_PATH,
      //  NULL},
      // {"ssl_ca_file", P_SSLPATH, PI_TEXT, (void *)&ssl_ca_file,
      // CMT_SSL_CA_FILE,
      //  NULL},
      // {"ssl_ca_default", P_INT, PI_ONOFF, (void *)&ssl_ca_default,
      //  CMT_SSL_CA_DEFAULT, NULL},
  };

  std::list<param_ptr> params8 = {
      // {"use_cookie", P_INT, PI_ONOFF, (void *)&use_cookie, CMT_USECOOKIE,
      // NULL},
      // {"show_cookie", P_INT, PI_ONOFF, (void *)&show_cookie, CMT_SHOWCOOKIE,
      //  NULL},
      // {"accept_cookie", P_INT, PI_ONOFF, (void *)&accept_cookie,
      //  CMT_ACCEPTCOOKIE, NULL},
      // {"accept_bad_cookie", P_INT, PI_SEL_C, (void *)&accept_bad_cookie,
      //  CMT_ACCEPTBADCOOKIE, (void *)badcookiestr},
      // {"cookie_reject_domains", P_STRING, PI_TEXT,
      //  (void *)&cookie_reject_domains, CMT_COOKIE_REJECT_DOMAINS, NULL},
      // {"cookie_accept_domains", P_STRING, PI_TEXT,
      //  (void *)&cookie_accept_domains, CMT_COOKIE_ACCEPT_DOMAINS, NULL},
      // {"cookie_avoid_wrong_number_of_dots", P_STRING, PI_TEXT,
      //  (void *)&cookie_avoid_wrong_number_of_dots,
      //  CMT_COOKIE_AVOID_WONG_NUMBER_OF_DOTS, NULL},
  };

  std::list<param_ptr> params9 = {
      // {"passwd_file", P_STRING, PI_TEXT, (void *)&passwd_file,
      // CMT_PASSWDFILE,
      //  NULL},
      // {"disable_secret_security_check", P_INT, PI_ONOFF,
      //  (void *)&disable_secret_security_check,
      //  CMT_DISABLE_SECRET_SECURITY_CHECK, NULL},
      // {"ftppasswd", P_STRING, PI_TEXT, (void *)&ftppasswd, CMT_FTPPASS,
      // NULL},
      // {"ftppass_hostnamegen", P_INT, PI_ONOFF, (void *)&ftppass_hostnamegen,
      //  CMT_FTPPASS_HOSTNAMEGEN, NULL},
      // {"siteconf_file", P_STRING, PI_TEXT, (void *)&siteconf_file,
      //  CMT_SITECONF_FILE, NULL},
      // {"user_agent", P_STRING, PI_TEXT, (void *)&UserAgent, CMT_USERAGENT,
      //  NULL},
      // {"no_referer", P_INT, PI_ONOFF, (void *)&NoSendReferer,
      // CMT_NOSENDREFERER,
      //  NULL},
      // {"cross_origin_referer", P_INT, PI_ONOFF, (void *)&CrossOriginReferer,
      //  CMT_CROSSORIGINREFERER, NULL},
      // {"accept_language", P_STRING, PI_TEXT, (void *)&AcceptLang,
      //  CMT_ACCEPTLANG, NULL},
      // {"accept_encoding", P_STRING, PI_TEXT, (void *)&AcceptEncoding,
      //  CMT_ACCEPTENCODING, NULL},
      // {"accept_media", P_STRING, PI_TEXT, (void *)&AcceptMedia,
      // CMT_ACCEPTMEDIA,
      //  NULL},
      // {"argv_is_url", P_CHARINT, PI_ONOFF, (void *)&ArgvIsURL,
      // CMT_ARGV_IS_URL,
      //  NULL},
      // {"retry_http", P_INT, PI_ONOFF, (void *)&retryAsHttp, CMT_RETRY_HTTP,
      //  NULL},
      // {"default_url", P_INT, PI_SEL_C, (void *)&DefaultURLString,
      //  CMT_DEFAULT_URL, (void *)defaulturls},
      // {"follow_redirection", P_INT, PI_TEXT, &FollowRedirection,
      //  CMT_FOLLOW_REDIRECTION, NULL},
      // {"meta_refresh", P_CHARINT, PI_ONOFF, (void *)&MetaRefresh,
      //  CMT_META_REFRESH, NULL},
      // {"localhost_only", P_CHARINT, PI_ONOFF, (void *)&LocalhostOnly,
      //  CMT_LOCALHOST_ONLY, NULL},
      // {"dns_order", P_INT, PI_SEL_C, (void *)&DNS_order, CMT_DNS_ORDER,
      //  (void *)dnsorders},
  };

  sections = {
      {N_("Display Settings"), params1},
      {N_("Miscellaneous Settings"), params3},
      {N_("Directory Settings"), params5},
      {N_("External Program Settings"), params6},
      {N_("Network Settings"), params9},
      {N_("Proxy Settings"), params4},
      {N_("SSL Settings"), params7},
      {N_("Cookie Settings"), params8},
  };

  create_option_search_table();
}

void Option::create_option_search_table() {
  for (auto &section : sections) {
    for (auto &param : section.params) {
      RC_search_table.insert({param.name, &param});
    }
  }
}

Option::~Option() {}

Option &Option::instance() {
  static Option s_instance;
  return s_instance;
}

param_ptr *Option::search_param(const std::string &name) {
  auto found = RC_search_table.find(name);
  if (found != RC_search_table.end()) {
    return found->second;
  }
  return nullptr;
}

int Option::set_param(const std::string &name, const std::string &value) {
  if (value.empty()) {
    return 0;
  }

  auto p = search_param(name.c_str());
  if (p == NULL) {
    return 0;
  }

  return p->setParseValue(value);
}

#define W3MHELPERPANEL_CMDNAME "w3mhelperpanel"

static char optionpanel_src1[] =
    "<html><head><title>Option Setting Panel</title></head><body>\
<h1 align=center>Option Setting Panel<br>(w3m version %s)</b></h1>\
<form method=post action=\"file:///$LIB/" W3MHELPERPANEL_CMDNAME "\">\
<input type=hidden name=mode value=panel>\
<input type=hidden name=cookie value=\"%s\">\
<input type=submit value=\"%s\">\
</form><br>\
<form method=internal action=option>";

std::string Option::load_option_panel() {
  std::stringstream src;
  src << "<table><tr><td>";

  for (auto &section : sections) {
    src << "<h1>" << section.name << "</h1>";
    src << "<table width=100% cellpadding=0>";
    for (auto &p : section.params) {
      src << "<tr><td>" << p.comment;
      src << Sprintf("</td><td width=%d>", (int)(28 * pixel_per_char))->ptr;

      switch (p.inputtype) {
      case PI_TEXT:
        src << "<input type=text name=" << p.name << " value=\""
            << html_quote(p.to_str()) << "\">";
        break;

      case PI_ONOFF: {
        auto x = atoi(p.to_str().c_str());
        src << "<input type=radio name=" << p.name << " value=1"
            << (x ? " checked" : "")
            << ">YES&nbsp;&nbsp;<input type=radio name=" << p.name << " value=0"
            << (x ? "" : " checked") << ">NO";
        break;
      }

      case PI_SEL_C: {
        auto tmp = p.to_str();
        src << "<select name=" << p.name << ">";
        for (auto s = (struct sel_c *)p.select; s->text != NULL; s++) {
          src << "<option value=";
          src << Sprintf("%s\n", s->cvalue)->ptr;
          if ((p.type != P_CHAR && s->value == atoi(tmp.c_str())) ||
              (p.type == P_CHAR && (char)s->value == *(tmp.c_str()))) {
            src << " selected";
          }
          src << '>';
          src << s->text;
        }
        src << "</select>";
        break;
      }
      }
      src << "</td></tr>\n";
    }
    src << "<tr><td></td><td><p><input type=submit value=\"OK\"></td></tr>"
        << "</table><hr width=50%>";
  }
  src << "</table></form></body></html>";
  return src.str();
}
