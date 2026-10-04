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

// 'const char*'
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
inline static void pushText(struct TextList* tl, const char* s)
{
    pushValue((struct GeneralList*)(tl), (void*)allocStr((s)).ptr);
}
inline static const char* popText(struct TextList* tl)
{
    return (const char*)popValue((struct GeneralList*)(tl));
}
inline static const char* rpopText(struct TextList* tl)
{
    return (const char*)rpopValue((struct GeneralList*)(tl));
}
inline static void delText(struct TextList* tl, struct TextListItem* i)
{
    delValue((struct GeneralList*)(tl), (void*)(i));
}
inline static struct TextList* appendTextList(struct TextList* tl, struct TextList* tl2)
{
    return (struct TextList*)appendGeneralList((struct GeneralList*)(tl), (struct GeneralList*)(tl2));
}

// struct TextLine{pSr, int};
struct TextLine {
    pStr line;
    int pos;
};
struct TextLineListItem {
    struct TextLine* ptr;
    struct TextLineListItem* next;
    struct TextLineListItem* prev;
};
struct TextLineList {
    struct TextLineListItem* first;
    struct TextLineListItem* last;
    int nitem;
};

struct TextLine* newTextLine(pStr line, int pos);
void appendTextLine(struct TextLineList* tl, pStr line, int pos);
inline static struct TextLineList* newTextLineList()
{
    return (struct TextLineList*)newGeneralList();
}
inline static void pushTextLine(struct TextLineList* tl, struct TextLine* lbuf)
{
    pushValue((struct GeneralList*)(tl), (void*)(lbuf));
}
inline static struct TextLine* popTextLine(struct TextLineList* tl)
{
    return (struct TextLine*)popValue((struct GeneralList*)(tl));
}
inline static struct TextLine* rpopTextLine(struct TextLineList* tl)
{
    return (struct TextLine*)rpopValue((struct GeneralList*)(tl));
}
inline static struct TextLineList* appendTextLineList(struct TextLineList* tl, struct TextLineList* tl2)
{
    return (struct TextLineList*)appendGeneralList((struct GeneralList*)(tl), (struct GeneralList*)(tl2));
}
