#pragma once

extern int LINES, COLS;
#define LASTLINE (LINES - 1)

typedef struct TermCap {
  char bp[1024];
  char *T_cd;
  char *T_ce;
  char *T_kr;
  char *T_kl;
  char *T_cr;
  char *T_bt;
  char *T_ta;
  char *T_sc;
  char *T_rc;
  char *T_so;
  char *T_se;
  char *T_us;
  char *T_ue;
  char *T_cl;
  char *T_cm;
  char *T_al;
  char *T_sr;
  char *T_md;
  char *T_me;
  char *T_ti;
  char *T_te;
  char *T_nd;
  char *T_as;
  char *T_ae;
  char *T_eA;
  char *T_ac;
  char *T_op;
  int LINES;
  int COLS;
} TermCap;

bool termcap_load(const char *ent);
TermCap *termcap_get();
void termcap_setlinescols();

