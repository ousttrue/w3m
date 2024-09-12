#pragma once

/// man 5 termcap
/// https://man.archlinux.org/man/termcap.5
/// https://nxmnpg.lemoda.net/ja/5/termcap
///
/// https://learn.microsoft.com/ja-jp/windows/console/console-virtual-terminal-sequences

typedef struct TermCap {
  /// clear to the end of display
  char *T_cd;
  /// clear to the end of line
  char *T_ce;

  /// cursor right
  char *T_kr;
  /// cursor left
  char *T_kl;
  /// move right one space
  char *T_nd;

  /// carriage return
  char *T_cr;
  // char *T_bt;
  /// tab
  char *T_ta;

  /// save cursor
  char *T_sc;
  /// restore cursor
  char *T_rc;

  /// standout mode
  char *T_so;
  /// standout mode end
  char *T_se;

  /// underline mode
  char *T_us;
  /// underline mode end
  char *T_ue;

  /// clear screen
  char *T_cl;
  /// cursor move
  char *T_cm;
  /// append line
  char *T_al;
  /// scroll reverse
  char *T_sr;

  /// bold mode
  char *T_md;
  /// bold mode end
  char *T_me;

  /// terminal init
  char *T_ti;
  /// terminal end
  char *T_te;

  /// alternative (graphic) charset start
  char *T_as;
  /// alternative (graphic) charset end
  char *T_ae;

  /// enable alternative charset
  char *T_eA;
  /// graphics charset pairs
  char *T_ac;
  /// set default color pair to its original value
  char *T_op;
  /// number of line
  int LINES;
  /// number of column
  int COLS;

  char bp[1024];
  char funcstr[256];
} TermCap;
