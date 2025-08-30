#pragma once

struct Frame;
struct Frame* displayBuffer();

struct VirtualTerm;
struct Frame* screenToFrame(const struct VirtualTerm* vt);

