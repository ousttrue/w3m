#pragma once

struct Url;
struct URLFile;
union input_stream* openFTPStream(struct Url* pu, struct URLFile* uf);

