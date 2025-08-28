#pragma once
#include "writer.h"

extern int highIntensityColors;

struct Frame;
void refreshFrame(const struct Writer* writer, struct Frame* frame);

void termBell(const struct Writer* writer);
void termClear(const struct Writer* writer);
void MOVE(const struct Writer* writer, int line, int column);
