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

extern struct ListItem* GeneralList_newItem(void* s, struct ListItem* n, struct ListItem* p);
extern struct GeneralList* GeneralList_new(void);
extern void GeneralList_push(struct GeneralList* tl, void* s);
extern void* GeneralList_unshift(struct GeneralList* tl);
extern void* GeneralList_pop(struct GeneralList* tl);
extern void GeneralList_remove(struct GeneralList* tl, struct ListItem* it);
extern struct GeneralList* GeneralList_concat(struct GeneralList*, struct GeneralList*);

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

inline static struct TextList* TextList_new()
{
    return ((struct TextList*)GeneralList_new());
}
inline static void TextList_push(struct TextList* tl, const char* s)
{
    GeneralList_push((struct GeneralList*)(tl), (void*)allocStr((s)).ptr);
}
inline static const char* TextList_unshift(struct TextList* tl)
{
    return (const char*)GeneralList_unshift((struct GeneralList*)(tl));
}
inline static const char* TextList_pop(struct TextList* tl)
{
    return (const char*)GeneralList_pop((struct GeneralList*)(tl));
}
inline static void TextList_remove(struct TextList* tl, struct TextListItem* i)
{
    GeneralList_remove((struct GeneralList*)(tl), (void*)(i));
}
inline static struct TextList* TextList_concat(struct TextList* tl, struct TextList* tl2)
{
    return (struct TextList*)GeneralList_concat((struct GeneralList*)(tl), (struct GeneralList*)(tl2));
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

struct TextLine* TextLine_new(pStr line, int pos);
void TextLineList_append(struct TextLineList* tl, pStr line, int pos);
inline static struct TextLineList* TextLineList_new()
{
    return (struct TextLineList*)GeneralList_new();
}
inline static void TextLineList_push(struct TextLineList* tl, struct TextLine* lbuf)
{
    GeneralList_push((struct GeneralList*)(tl), (void*)(lbuf));
}
inline static struct TextLine* TextLineList_unshift(struct TextLineList* tl)
{
    return (struct TextLine*)GeneralList_unshift((struct GeneralList*)(tl));
}
inline static struct TextLine* TextLineList_pop(struct TextLineList* tl)
{
    return (struct TextLine*)GeneralList_pop((struct GeneralList*)(tl));
}
inline static struct TextLineList* TextLineList_concat(struct TextLineList* tl, struct TextLineList* tl2)
{
    return (struct TextLineList*)GeneralList_concat((struct GeneralList*)(tl), (struct GeneralList*)(tl2));
}
