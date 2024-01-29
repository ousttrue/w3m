#pragma once

struct ParsedURL;
struct URLFile;
union input_stream;
struct Str;
input_stream *openFTPStream(ParsedURL *pu, URLFile *uf);
Str *loadFTPDir0(ParsedURL *pu);
#define loadFTPDir(pu, charset) loadFTPDir0(pu)
void closeFTP(void);
void disconnectFTP(void);

