#include "message.h"
#include "screen.h"

// static GeneralList* message_list = NULL;
//
// void record_err_message(char* s)
// {
//     if (!message_list)
//         message_list = newGeneralList();
//     if (message_list->nitem >= getScreen()->ROWS)
//         popValue(message_list);
//     pushValue(message_list, allocStr(s, -1));
// }

void concatMessageList(Str tmp)
{
    // if (message_list)
    //     for (p = message_list->last; p; p = p->prev)
    //         Strcat_m_charp(tmp, "<tr><td><pre>", html_quote(p->ptr),
    //             "</pre></td></tr>\n", NULL);
    // else
    //     Strcat_charp(tmp, "<tr><td>(no message recorded)</td></tr>\n");
}

void message(enum MessageSeverity severity, const char* s)
{
    struct VirtualTerm* vt = getScreen();
    int row = vt->CurLine;
    int col = vt->CurColumn;
    move(vt, vt->ROWS - 1, 0);
    addnstr(vt, s, vt->COLS - 1);
    clrtoeolx(vt);
    move(vt, row, col);
}

// void disp_err_message(char* s, int redraw_current)
// {
//     record_err_message(s);
//     disp_message(s, redraw_current);
// }
//
// void disp_message_nsec(char* s, int redraw_current, int sec, int purge, int mouse)
// {
//     if (QuietMessage)
//         return;
//     if (Currentbuf != NULL)
//         message(s, Currentbuf->cursorX + Currentbuf->rootX,
//             Currentbuf->cursorY + Currentbuf->rootY);
//     else
//         message(s, getScreen()->ROWS - 1, 0);
//     // refresh(ttyWriter());
//     sleep_till_anykey(sec * 1000, purge);
// }
//
// void disp_message(char* s, int redraw_current)
// {
//     disp_message_nsec(s, redraw_current, 10, FALSE, TRUE);
// }
