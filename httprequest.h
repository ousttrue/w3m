#pragma once

struct Str;
struct ParsedURL;
struct TextList;

#define NO_REFERER ((char *)-1)

#define HR_COMMAND_GET 0
#define HR_COMMAND_POST 1
#define HR_COMMAND_CONNECT 2
#define HR_COMMAND_HEAD 3

#define HR_FLAG_LOCAL 1
#define HR_FLAG_PROXY 2

extern bool override_user_agent;
extern char *UserAgent;
extern char *AcceptLang;
extern char *AcceptEncoding;
extern char *AcceptMedia;
extern bool NoCache;
extern bool NoSendReferer;
extern bool CrossOriginReferer;
extern bool override_content_type;
extern Str *header_string;

struct FormList;
struct HRequest {
  char command;
  char flag;
  char *referer;
  FormList *request;
};

Str *HTTPrequest(ParsedURL *pu, ParsedURL *current, HRequest *hr,
                 TextList *extra);
