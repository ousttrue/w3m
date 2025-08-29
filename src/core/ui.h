#pragma once

// key input(blocking) or draw require UI
//
// lineinput
// search
// menu
// message
//
struct VirtualTerm;
struct UI {
    struct VirtualTerm* vt;
};

struct UI getUI();
