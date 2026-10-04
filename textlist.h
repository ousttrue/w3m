#pragma once
#include "Str.h"

#define GENERAL_LIST_MAX (INT_MAX / 32)

// General doubly linked list
struct ListItem {
    void* ptr;
    struct ListItem* next;
    struct ListItem* prev;
};

struct GeneralList {
    struct ListItem* first;
    struct ListItem* last;
    int nitem;
};

extern struct ListItem* newListItem(void* s, struct ListItem* n, struct ListItem* p);
extern struct GeneralList* newGeneralList(void);
extern void pushValue(struct GeneralList* tl, void* s);
extern void* popValue(struct GeneralList* tl);
extern void* rpopValue(struct GeneralList* tl);
extern void delValue(struct GeneralList* tl, struct ListItem* it);
extern struct GeneralList* appendGeneralList(struct GeneralList*, struct GeneralList*);

// Text list
struct TextListItem {
    const char* ptr;
    struct TextListItem* next;
    struct TextListItem* prev;
};

struct TextList {
    struct TextListItem* first;
    struct TextListItem* last;
    int nitem;
};

inline static struct TextList* newTextList()
{
    return ((struct TextList*)newGeneralList());
}
#define pushText(tl, s) pushValue((struct GeneralList*)(tl), (void*)allocStr((s)).ptr)
#define popText(tl) ((char*)popValue((struct GeneralList*)(tl)))
#define rpopText(tl) ((char*)rpopValue((struct GeneralList*)(tl)))
#define delText(tl, i) delValue((struct GeneralList*)(tl), (void*)(i))
#define appendTextList(tl, tl2) ((struct TextList*)appendGeneralList((struct GeneralList*)(tl), (struct GeneralList*)(tl2)))

/* Line text list */

typedef struct _TextLine {
    pStr line;
    int pos;
} TextLine;

typedef struct _textlinelistitem {
    TextLine* ptr;
    struct _textlinelistitem* next;
    struct _textlinelistitem* prev;
} TextLineListItem;

typedef struct _textlinelist {
    TextLineListItem* first;
    TextLineListItem* last;
    int nitem;
} TextLineList;

extern TextLine* newTextLine(pStr line, int pos);
extern void appendTextLine(TextLineList* tl, pStr line, int pos);
#define newTextLineList() ((TextLineList*)newGeneralList())
#define pushTextLine(tl, lbuf) pushValue((struct GeneralList*)(tl), (void*)(lbuf))
#define popTextLine(tl) ((TextLine*)popValue((struct GeneralList*)(tl)))
#define rpopTextLine(tl) ((TextLine*)rpopValue((struct GeneralList*)(tl)))
#define appendTextLineList(tl, tl2) ((TextLineList*)appendGeneralList((struct GeneralList*)(tl), (struct GeneralList*)(tl2)))
