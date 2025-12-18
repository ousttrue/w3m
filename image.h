#pragma once
#include "Str.h"
#include <sys/types.h>

struct ImageCache {
    char* url;
    struct Url* current;
    char* file;
    char* touch;
    pid_t pid;
    char loaded;
    int index;
    short width;
    short height;
    short a_width;
    short a_height;
};

struct Image {
    char* url;
    char* ext;
    short width;
    short height;
    short xoffset;
    short yoffset;
    short y;
    short rows;
    char* map;
    char ismap;
    int touch;
    struct ImageCache* cache;
};

struct _Buffer;
void initImage(void);
void termImage(void);
void addImage(struct ImageCache* cache, int x, int y, int sx, int sy, int w, int h);
void drawImage(struct _Buffer* currentbuf);
void clearImage(void);

struct _Buffer;
extern void deleteImage(struct _Buffer* buf);
extern void getAllImage(struct _Buffer* buf);

#define IMG_FLAG_START 0
#define IMG_FLAG_STOP 1
#define IMG_FLAG_NEXT 2
extern void loadImage(struct _Buffer* buf, int flag);

struct ImageCache* getImage(struct Image* image, struct Url* current, int flag);
int getImageSize(struct ImageCache* cache);
