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

void write_T_op();
void write_T_ce();
void write_T_ae();
void write_T_me();
void write_T_nd();
void write_T_so();
void write_T_us();
void write_T_md();
void write_T_eA();
void write_T_as();
void write_T_cl();
void MOVE(int line, int column);
