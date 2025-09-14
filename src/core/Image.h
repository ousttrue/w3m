#pragma once
#include <sys/types.h>

#define MAX_IMAGE_SIZE 2048

enum InlineImageType {
    INLINE_IMG_NONE = 0,
    INLINE_IMG_OSC5379 = 1,
    INLINE_IMG_SIXEL = 2,
    INLINE_IMG_ITERM2 = 3,
    INLINE_IMG_KITTY = 4,
};

enum ImageCacheFlags {
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
    pid_t pid;
    enum ImageCacheFlags loaded;
    int index;
    short width;
    short height;
    short a_width;
    short a_height;
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

enum ImageGetFlag {
    IMG_FLAG_SKIP = 1,
    IMG_FLAG_AUTO = 2,
};
