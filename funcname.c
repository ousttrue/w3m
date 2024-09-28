#include "cookie.h"

FuncList w3mFuncList[] = {
    /*0*/ {"@@@", nulcmd},
    /*1*/ {"ABORT", quitfm},
    /*2*/ {"ACCESSKEY", accessKey},
    /*3*/ {"ADD_BOOKMARK", adBmark},
    /*4*/ {"ALARM", nulcmd},
    /*5*/ {"BACK", backBf},
    /*6*/ {"BEGIN", goLineF},
    /*7*/ {"BOOKMARK", ldBmark},
    /*8*/ {"CENTER_H", ctrCsrH},
    /*9*/ {"CENTER_V", ctrCsrV},
    /*10*/ {"CHARSET", docCSet},
    /*11*/ {"CLOSE_TAB", closeT},
    /*12*/ {"CLOSE_TAB_MOUSE", closeTMs},
    /*13*/ {"COMMAND", execCmd},
    /*14*/ {"COOKIE", cooLst},
    /*15*/ {"CURSOR_BOTTOM", cursorBottom},
    /*16*/ {"CURSOR_MIDDLE", cursorMiddle},
    /*17*/ {"CURSOR_TOP", cursorTop},
    /*18*/ {"DEFAULT_CHARSET", defCSet},
    /*19*/ {"DEFINE_KEY", defKey},
    /*20*/ {"DELETE_PREVBUF", deletePrevBuf},
    /*21*/ {"DICT_WORD", dictword},
    /*22*/ {"DICT_WORD_AT", dictwordat},
    /*23*/ {"DISPLAY_IMAGE", dispI},
    /*24*/ {"DOWN", ldown1},
    /*25*/ {"DOWNLOAD", svSrc},
    /*26*/ {"DOWNLOAD_LIST", ldDL},
    /*27*/ {"EDIT", editBf},
    /*28*/ {"EDIT_SCREEN", editScr},
    /*29*/ {"END", goLineL},
    /*30*/ {"ESCBMAP", escbmap},
    /*31*/ {"ESCMAP", escmap},
    /*32*/ {"EXEC_SHELL", execsh},
    /*33*/ {"EXIT", quitfm},
    /*34*/ {"EXTERN", extbrz},
    /*35*/ {"EXTERN_LINK", linkbrz},
    /*36*/ {"FRAME", nulcmd},
    /*37*/ {"GOTO", goURL},
    /*38*/ {"GOTO_HOME", goHome},
    /*39*/ {"GOTO_LINE", goLine},
    /*40*/ {"GOTO_LINK", followA},
    /*41*/ {"GOTO_RELATIVE", gorURL},
    /*42*/ {"HELP", ldhelp},
    /*43*/ {"HISTORY", ldHist},
    /*44*/ {"INFO", pginfo},
    /*45*/ {"INTERRUPT", susp},
    /*46*/ {"ISEARCH", isrchfor},
    /*47*/ {"ISEARCH_BACK", isrchbak},
    /*48*/ {"LEFT", col1L},
    /*49*/ {"LINE_BEGIN", linbeg},
    /*50*/ {"LINE_END", linend},
    /*51*/ {"LINE_INFO", curlno},
    /*52*/ {"LINK_BEGIN", topA},
    /*53*/ {"LINK_END", lastA},
    /*54*/ {"LINK_MENU", linkMn},
    /*55*/ {"LINK_N", nthA},
    /*56*/ {"LIST", linkLst},
    /*57*/ {"LIST_MENU", listMn},
    /*58*/ {"LOAD", ldfile},
    /*59*/ {"MAIN_MENU", mainMn},
    /*60*/ {"MARK", _mark},
    /*61*/ {"MARK_MID", chkNMID},
    /*62*/ {"MARK_URL", chkURL},
    /*63*/ {"MARK_WORD", chkWORD},
    /*64*/ {"MENU", mainMn},
    /*65*/ {"MENU_MOUSE", menuMs},
    /*66*/ {"MOUSE", mouse},
    /*67*/ {"MOUSE_TOGGLE", msToggle},
    /*68*/ {"MOVE_DOWN", movD},
    /*69*/ {"MOVE_DOWN1", movD1},
    /*70*/ {"MOVE_LEFT", movL},
    /*71*/ {"MOVE_LEFT1", movL1},
    /*72*/ {"MOVE_LIST_MENU", movlistMn},
    /*73*/ {"MOVE_MOUSE", movMs},
    /*74*/ {"MOVE_RIGHT", movR},
    /*75*/ {"MOVE_RIGHT1", movR1},
    /*76*/ {"MOVE_UP", movU},
    /*77*/ {"MOVE_UP1", movU1},
    /*78*/ {"MSGS", msgs},
    /*79*/ {"MULTIMAP", multimap},
    /*80*/ {"NEW_TAB", newT},
    /*81*/ {"NEXT", nextBf},
    /*82*/ {"NEXT_DOWN", nextD},
    /*83*/ {"NEXT_HALF_PAGE", hpgFore},
    /*84*/ {"NEXT_LEFT", nextL},
    /*85*/ {"NEXT_LEFT_UP", nextLU},
    /*86*/ {"NEXT_LINK", nextA},
    /*87*/ {"NEXT_MARK", nextMk},
    /*88*/ {"NEXT_PAGE", pgFore},
    /*89*/ {"NEXT_RIGHT", nextR},
    /*90*/ {"NEXT_RIGHT_DOWN", nextRD},
    /*91*/ {"NEXT_TAB", nextT},
    /*92*/ {"NEXT_UP", nextU},
    /*93*/ {"NEXT_VISITED", nextVA},
    /*94*/ {"NEXT_WORD", movRW},
    /*95*/ {"NOTHING", nulcmd},
    /*96*/ {"NULL", nulcmd},
    /*97*/ {"OPTIONS", ldOpt},
    /*98*/ {"PCMAP", pcmap},
    /*99*/ {"PEEK", curURL},
    /*100*/ {"PEEK_IMG", peekIMG},
    /*101*/ {"PEEK_LINK", peekURL},
    /*102*/ {"PIPE_BUF", nulcmd},
    /*103*/ {"PIPE_SHELL", nulcmd},
    /*104*/ {"PREV", prevBf},
    /*105*/ {"PREV_HALF_PAGE", hpgBack},
    /*106*/ {"PREV_LINK", prevA},
    /*107*/ {"PREV_MARK", prevMk},
    /*108*/ {"PREV_PAGE", pgBack},
    /*109*/ {"PREV_TAB", prevT},
    /*110*/ {"PREV_VISITED", prevVA},
    /*111*/ {"PREV_WORD", movLW},
    /*112*/ {"PRINT", svBuf},
    /*113*/ {"QUIT", qquitfm},
    /*114*/ {"READ_SHELL", readsh},
    /*115*/ {"REDO", redoPos},
    /*116*/ {"REDRAW", rdrwSc},
    /*117*/ {"REG_MARK", reMark},
    /*118*/ {"REINIT", reinit},
    /*119*/ {"RELOAD", reload},
    /*120*/ {"RESHAPE", reshape},
    /*121*/ {"RIGHT", col1R},
    /*122*/ {"SAVE", svSrc},
    /*123*/ {"SAVE_IMAGE", svI},
    /*124*/ {"SAVE_LINK", svA},
    /*125*/ {"SAVE_SCREEN", svBuf},
    /*126*/ {"SEARCH", srchfor},
    /*127*/ {"SEARCH_BACK", srchbak},
    /*128*/ {"SEARCH_FORE", srchfor},
    /*129*/ {"SEARCH_NEXT", srchnxt},
    /*130*/ {"SEARCH_PREV", srchprv},
    /*131*/ {"SELECT", selBuf},
    /*132*/ {"SELECT_MENU", selMn},
    /*133*/ {"SETENV", setEnv},
    /*134*/ {"SET_OPTION", setOpt},
    /*135*/ {"SGRMOUSE", sgrmouse},
    /*136*/ {"SHELL", execsh},
    /*137*/ {"SHIFT_LEFT", shiftl},
    /*138*/ {"SHIFT_RIGHT", shiftr},
    /*139*/ {"SOURCE", vwSrc},
    /*140*/ {"STOP_IMAGE", stopI},
    /*141*/ {"SUBMIT", submitForm},
    /*142*/ {"SUSPEND", susp},
    /*143*/ {"TAB_GOTO", tabURL},
    /*144*/ {"TAB_GOTO_RELATIVE", tabrURL},
    /*145*/ {"TAB_LEFT", tabL},
    /*146*/ {"TAB_LINK", tabA},
    /*147*/ {"TAB_MENU", tabMn},
    /*148*/ {"TAB_MOUSE", tabMs},
    /*149*/ {"TAB_RIGHT", tabR},
    /*150*/ {"UNDO", undoPos},
    /*151*/ {"UP", lup1},
    /*152*/ {"VERSION", dispVer},
    /*153*/ {"VIEW", vwSrc},
    /*154*/ {"VIEW_BOOKMARK", ldBmark},
    /*155*/ {"VIEW_IMAGE", followI},
    /*156*/ {"WHEREIS", srchfor},
    /*157*/ {"WRAP_TOGGLE", wrapToggle},
    {NULL, NULL}};
