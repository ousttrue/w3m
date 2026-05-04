#pragma once
#include <sys/types.h>

enum ImageCacheStatus {
    IMG_FLAG_UNLOADED = 0,
    IMG_FLAG_LOADED = 1,
    IMG_FLAG_ERROR = 2,
    IMG_FLAG_DONT_REMOVE = 4,
};

struct ImageCache {
    const char* url;
    struct Url* current;
    const char* file;
    const char* touch;
    enum ImageCacheStatus loaded;
    int index;
    int width;
    int height;
    int a_width;
    int a_height;
};

struct Image {
    const char* url;
    const char* ext;
    short width;
    short height;
    short xoffset;
    short yoffset;
    short y;
    short rows;
    const char* map;
    char ismap;
    int touch;
    struct ImageCache* cache;
};

enum GetImageFlag {
    IMG_FLAG_SKIP = 1,
    IMG_FLAG_AUTO = 2,
};

void addImage(struct ImageCache* cache, int x, int y, int sx, int sy, int w, int h);

void loadImageStart();
void loadImageStop();
struct ImageCache* getImage(struct Image* image, struct Url* current, enum GetImageFlag flag);
bool getImageSize(struct ImageCache* cache);
struct Buffer;
void getAllImage(struct Buffer* buf);
void deleteImage(struct Buffer* buf);
