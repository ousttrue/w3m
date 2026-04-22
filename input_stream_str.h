#pragma once
#include "Str.h"

extern Str StrISgets2(InputStream stream, char crnl);
#define StrISgets(stream) StrISgets2(stream, false)
#define StrmyISgets(stream) StrISgets2(stream, true)
