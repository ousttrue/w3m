#pragma once

struct Url {
    int scheme;
    char* user;
    char* pass;
    char* host;
    int port;
    char* file;
    char* real_file;
    char* query;
    char* label;
    int is_nocache;
};

extern void parseURL(char* url, struct Url* p_url, struct Url* current);
extern void copyParsedURL(struct Url* p, const struct Url* q);
extern void parseURL2(char* url, struct Url* pu, struct Url* current);
extern Str parsedURL2Str(struct Url* pu);
extern Str parsedURL2RefererStr(struct Url* pu);
