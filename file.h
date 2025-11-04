#pragma once
#include "Url.h"
#include "istream.h"

struct _Buffer;
struct form_list;
struct _Buffer* loadGeneralFile(char* path, struct Url* current, char* referer, enum LoadGeneralFlags flag, struct form_list* request);
