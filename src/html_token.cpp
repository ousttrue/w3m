#include "html_token.h"
#include "table.h"
#include "html_feed_env.h"
#include "readtoken.h"
#include "html_tag.h"
#include "html_tag_parse.h"
#include "myctype.h"

std::optional<Token> Tokenizer::getToken(html_feed_environ *h_env,
                                         TableStatus &t, bool internal) {
  while (line.size()) {
    bool is_tag = false;
    auto pre_mode = t.pre_mode(h_env);
    int end_tag = t.end_tag(h_env);
    std::string str;
    if (line[0] == '<' || h_env->status != R_ST_NORMAL) {
      /*
       * Tag processing
       */
      if (h_env->status == R_ST_EOL)
        h_env->status = R_ST_NORMAL;
      else {
        if (h_env->status != R_ST_NORMAL) {
          auto p = line.c_str();
          append_token(h_env->tagbuf, &p, &h_env->status,
                       pre_mode & RB_PREMODE);
          line = p;
        } else {
          auto p = line.c_str();
          if (auto buf =
                  read_token(&p, &h_env->status, pre_mode & RB_PREMODE)) {
            h_env->tagbuf = *buf;
          }
          line = p;
        }
        if (h_env->status != R_ST_NORMAL) {
          // exit
          return {};
        }
      }
      if (h_env->tagbuf.empty())
        continue;
      str = h_env->tagbuf;
      if (str[0] == '<') {
        if (str[1] && REALLY_THE_BEGINNING_OF_A_TAG(str))
          is_tag = true;
        else if (!(pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT |
                               RB_STYLE | RB_TITLE))) {
          line = str.substr(1) + line;
          str = "&lt;";
        }
      }
    } else {
      std::string tokbuf;
      auto p = line.c_str();
      if (auto value =
              read_token(&p, &h_env->status, pre_mode & RB_PREMODE)) {
        tokbuf = *value;
      }
      line = p;
      if (h_env->status != R_ST_NORMAL) /* R_ST_AMP ? */
        h_env->status = R_ST_NORMAL;
      str = tokbuf;
    }

    if (pre_mode & (RB_PLAIN | RB_INTXTA | RB_INSELECT | RB_SCRIPT | RB_STYLE |
                    RB_TITLE)) {
      bool goto_proc_normal = false;
      if (is_tag) {
        auto p = str.c_str();
        if (auto tag = parseHtmlTag(&p, internal)) {
          if (tag->tagid == end_tag ||
              (pre_mode & RB_INSELECT && tag->tagid == HTML_N_FORM) ||
              (pre_mode & RB_TITLE &&
               (tag->tagid == HTML_N_HEAD || tag->tagid == HTML_BODY))) {
            goto_proc_normal = true;
          }
        }
      }
      if (goto_proc_normal) {
        // goto proc_normal;
      } else {
        /* title */
        if (pre_mode & RB_TITLE) {
          h_env->feed_title(str);
          continue;
        }
        /* select */
        if (pre_mode & RB_INSELECT) {
          if (h_env->table_level >= 0) {
            // goto proc_normal;
          } else {
            h_env->parser.feed_select(str);
            continue;
          }
        } else {
          if (is_tag) {
            const char *p;
            if (strncmp(str.c_str(), "<!--", 4) &&
                (p = strchr(str.c_str() + 1, '<'))) {
              str = std::string(str, p - str.c_str());
              line = std::string(p) + line;
            }
            is_tag = false;
            continue;
          }
          if (h_env->table_level >= 0) {
            // goto proc_normal;
          } else {
            /* textarea */
            if (pre_mode & RB_INTXTA) {
              h_env->parser.feed_textarea(str);
              continue;
            }
            /* script */
            if (pre_mode & RB_SCRIPT)
              continue;
            /* style */
            if (pre_mode & RB_STYLE)
              continue;
          }
        }
      }
    }

    return Token{is_tag, str};
  }

  return {};
}
