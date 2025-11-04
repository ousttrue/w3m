#pragma once
#include "Url.h"
#include <sys/types.h>

#define IMG_FLAG_UNLOADED 0
#define IMG_FLAG_LOADED 1
#define IMG_FLAG_ERROR 2
#define IMG_FLAG_DONT_REMOVE 4

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

extern const char* Imgdisplay;

enum ImageGetFlags {
    IMG_FLAG_SKIP = 1,
    IMG_FLAG_AUTO = 2,
};
struct ImageCache* getImage(struct Image* image, struct Url* current, enum ImageGetFlags flag);
int getImageSize(struct ImageCache* cache);
struct _Buffer;

enum ImageLoadFlag {
    IMG_FLAG_START = 0,
    IMG_FLAG_STOP = 1,
    IMG_FLAG_NEXT = 2,
};
void loadImage(struct _Buffer* buf, enum ImageLoadFlag flag);

void initImage(void);
void termImage(void);
void addImage(struct ImageCache* cache, int x, int y, int sx, int sy, int w,
    int h);
void drawImage(void);
void clearImage(void);
