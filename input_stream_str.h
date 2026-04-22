#pragma once
#include "Str.h"

struct InputStream;
Str StrISgets2(struct InputStream* stream, char crnl);
#define StrISgets(stream) StrISgets2(stream, false)
#define StrmyISgets(stream) StrISgets2(stream, true)
