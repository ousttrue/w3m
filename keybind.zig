const c = @import("c_include.zig").c;

pub const GlobalKeymap = [_]c.DefunFunc{
    //  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g
    c._mark,   c.linbeg,  c.movL,     c.nulcmd,   c.nulcmd,  c.linend,   c.movR,    c.curlno,
    //  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o
    c.ldHist,  c.nextA,   c.followA,  c.cooLst,   c.rdrwSc,  c.followA,  c.movD,    c.nulcmd,
    //  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w
    c.movU,    c.closeT,  c.isrchbak, c.isrchfor, c.tabA,    c.prevA,    c.pgFore,  c.wrapToggle,
    //  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_
    c.nulcmd,  c.nulcmd,  c.susp,     c.escmap,   c.nulcmd,  c.nulcmd,   c.nulcmd,  c.goHome,
    //  SPC     !       "       #       $       %       &       '
    c.pgFore,  c.execsh,  c.reMark,   c.pipesh,   c.linend,  c.nulcmd,   c.nulcmd,  c.nulcmd,
    //  (       )       *       +       ,       -       .       /
    c.undoPos, c.redoPos, c.nulcmd,   c.pgFore,   c.col1L,   c.pgBack,   c.col1R,   c.srchfor,
    //  0       1       2       3       4       5       6       7
    c.nulcmd,  c.nulcmd,  c.nulcmd,   c.nulcmd,   c.nulcmd,  c.nulcmd,   c.nulcmd,  c.nulcmd,
    //  8       9       :       ;       <       =       >       ?
    c.nulcmd,  c.nulcmd,  c.chkURL,   c.chkWORD,  c.shiftl,  c.pginfo,   c.shiftr,  c.srchbak,
    //  @       A       B       C       D       E       F       G
    c.readsh,  c.nulcmd,  c.backBf,   c.nulcmd,   c.ldDL,    c.editBf,   c.rFrame,  c.goLineL,
    //  H       I       J       K       L       M       N       O
    c.ldhelp,  c.followI, c.lup1,     c.ldown1,   c.linkLst, c.extbrz,   c.srchprv, c.nulcmd,
    //  P       Q       R       S       T       U       V       W
    c.nulcmd,  c.quitfm,  c.reload,   c.svBuf,    c.newT,    c.goURL,    c.ldfile,  c.movLW,
    //  X       Y       Z       [       \       ]       ^       _
    c.nulcmd,  c.nulcmd,  c.ctrCsrH,  c.topA,     c.nulcmd,  c.lastA,    c.linbeg,  c.nulcmd,
    //  `       a       b       c       d       e       f       g
    c.nulcmd,  c.svA,     c.pgBack,   c.curURL,   c.nulcmd,  c.nulcmd,   c.nulcmd,  c.goLineF,
    //  h       i       j       k       l       m       n       o
    c.movL,    c.peekIMG, c.movD,     c.movU,     c.movR,    c.msToggle, c.srchnxt, c.ldOpt,
    //  p       q       r       s       t       u       v       w
    c.nulcmd,  c.qquitfm, c.dispVer,  c.selMn,    c.nulcmd,  c.peekURL,  c.vwSrc,   c.movRW,
    //  x       y       z       {       |       }       ~       DEL
    c.nulcmd,  c.nulcmd,  c.ctrCsrV,  c.prevT,    c.pipeBuf, c.nextT,    c.nulcmd,  c.nulcmd,
};

pub const EscKeymap = [_]c.DefunFunc{
    //  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g
    c.nulcmd, c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,    c.nulcmd,  c.nulcmd,
    //  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o
    c.nulcmd, c.prevA,   c.svA,     c.nulcmd,  c.nulcmd,  c.svA,       c.nulcmd,  c.nulcmd,
    //  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w
    c.nulcmd, c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,    c.nulcmd,  c.nulcmd,
    //  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_
    c.nulcmd, c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,    c.nulcmd,  c.nulcmd,
    //  SPC     !       "       #       $       %       &       '
    c.nulcmd, c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,    c.nulcmd,  c.nulcmd,
    //  (       )       *       +       ,       -       .       /
    c.nulcmd, c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,    c.nulcmd,  c.nulcmd,
    //  0       1       2       3       4       5       6       7
    c.nulcmd, c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,    c.nulcmd,  c.nulcmd,
    //  8       9       :       ;       <       =       >       ?
    c.nulcmd, c.nulcmd,  c.chkNMID, c.nulcmd,  c.goLineF, c.nulcmd,    c.goLineL, c.nulcmd,
    //  @       A       B       C       D       E       F       G
    c.nulcmd, c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,    c.nulcmd,  c.nulcmd,
    //  H       I       J       K       L       M       N       O
    c.nulcmd, c.svI,     c.nulcmd,  c.nulcmd,  c.nulcmd,  c.linkbrz,   c.nulcmd,  c.escbmap,
    //  P       Q       R       S       T       U       V       W
    c.nulcmd, c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,    c.nulcmd,  c.dictwordat,
    //  X       Y       Z       [       \       ]       ^       _
    c.nulcmd, c.nulcmd,  c.nulcmd,  c.escbmap, c.nulcmd,  c.nulcmd,    c.nulcmd,  c.nulcmd,
    //  `       a       b       c       d       e       f       g
    c.nulcmd, c.adBmark, c.ldBmark, c.execCmd, c.nulcmd,  c.editScr,   c.nulcmd,  c.goLine,
    //  h       i       j       k       l       m       n       o
    c.nulcmd, c.nulcmd,  c.nulcmd,  c.defKey,  c.listMn,  c.movlistMn, c.nextMk,  c.setOpt,
    //  p       q       r       s       t       u       v       w
    c.prevMk, c.nulcmd,  c.nulcmd,  c.svSrc,   c.tabMn,   c.gorURL,    c.pgBack,  c.dictword,
    //  x       y       z       {       |       }       ~       DEL
    c.nulcmd, c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,  c.nulcmd,    c.nulcmd,  c.nulcmd,
};

pub const EscBKeymap = [_]c.DefunFunc{
    //  C-@     C-a     C-b     C-c     C-d     C-e     C-f     C-g
    c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd, c.nulcmd,   c.nulcmd, c.nulcmd,  c.nulcmd,
    //  C-h     C-i     C-j     C-k     C-l     C-m     C-n     C-o
    c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd, c.nulcmd,   c.nulcmd, c.nulcmd,  c.nulcmd,
    //  C-p     C-q     C-r     C-s     C-t     C-u     C-v     C-w
    c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd, c.nulcmd,   c.nulcmd, c.nulcmd,  c.nulcmd,
    //  C-x     C-y     C-z     C-[     C-\     C-]     C-^     C-_
    c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd, c.nulcmd,   c.nulcmd, c.nulcmd,  c.nulcmd,
    //  SPC     !       "       #       $       %       &       '
    c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd, c.nulcmd,   c.nulcmd, c.nulcmd,  c.nulcmd,
    //  (       )       *       +       ,       -       .       /
    c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd, c.nulcmd,   c.nulcmd, c.nulcmd,  c.nulcmd,
    //  0       1       2       3       4       5       6       7
    c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd, c.nulcmd,   c.nulcmd, c.nulcmd,  c.nulcmd,
    //  8       9       :       ;       <       =       >       ?
    c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd, c.sgrmouse, c.nulcmd, c.nulcmd,  c.nulcmd,
    //  @       A       B       C       D       E       F       G
    c.nulcmd,  c.movU,   c.movD,   c.movR,   c.movL,     c.nulcmd, c.goLineL, c.pgFore,
    //  H       I       J       K       L       M       N       O
    c.goLineF, c.pgBack, c.nulcmd, c.nulcmd, c.nulcmd,   c.mouse,  c.nulcmd,  c.nulcmd,
    //  P       Q       R       S       T       U       V       W
    c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd, c.nulcmd,   c.nulcmd, c.nulcmd,  c.nulcmd,
    //  X       Y       Z       [       \       ]       ^       _
    c.nulcmd,  c.nulcmd, c.prevA,  c.nulcmd, c.nulcmd,   c.nulcmd, c.nulcmd,  c.nulcmd,
    //  `       a       b       c       d       e       f       g
    c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd, c.nulcmd,   c.nulcmd, c.nulcmd,  c.nulcmd,
    //  h       i       j       k       l       m       n       o
    c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd, c.nulcmd,   c.nulcmd, c.nulcmd,  c.nulcmd,
    //  p       q       r       s       t       u       v       w
    c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd, c.nulcmd,   c.nulcmd, c.nulcmd,  c.nulcmd,
    //  x       y       z       {       |       }       ~       DEL
    c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd, c.nulcmd,   c.nulcmd, c.nulcmd,  c.nulcmd,
};

const EscDKeymap = []c.DefunFunc{
    //  0       1       INS     3       4       PgUp,   PgDn    7
    c.nulcmd, c.goLineF, c.mainMn, c.nulcmd, c.goLineL, c.pgBack, c.pgFore, c.nulcmd,
    //  8       9       10      F1      F2      F3      F4      F5
    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,
    //  16      F6      F7      F8      F9      F10     22      23
    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,
    //  24      25      26      27      HELP    29      30      31
    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.mainMn,  c.nulcmd, c.nulcmd, c.nulcmd,

    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,
    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,
    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,
    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,

    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,
    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,
    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,
    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,

    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,
    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,
    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,
    c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,  c.nulcmd, c.nulcmd, c.nulcmd,
};
