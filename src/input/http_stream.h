#pragma once

extern bool LocalhostOnly;
extern const char *index_file;
extern const char *document_root;
extern bool ArgvIsURL;

struct FormList;
struct TextList;
struct HttpRequest;
union input_stream;
struct HttpResponse *openHttpStream(struct HttpRequest *hr,
                                    union input_stream *ouf);
