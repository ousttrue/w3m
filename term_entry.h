#pragma once
#include <stdbool.h>

struct TermEntry {
    char funcstr[256];
    char* cd;
    char* ce;
    char* kr;
    char* kl;
    char* cr;
    char* bt;
    char* ta;
    char* sc;
    char* rc;
    char* so;
    char* se;
    char* us;
    char* ue;
    char* cl;
    char* cm;
    char* al;
    char* sr;
    char* md;
    char* me;
    char* ti;
    char* te;
    char* nd;
    char* as;
    char* ae;
    char* eA;
    char* ac;
    char* op;
};

struct TermEntry* initTerm();
struct TermEntry* getTermEntry();
const char* getMoveXY(int x, int y);
