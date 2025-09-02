#pragma once
#include <sys/types.h>

extern int activeImage;
extern char* image_source;

#define INLINE_IMG_NONE 0
#define INLINE_IMG_OSC5379 1
#define INLINE_IMG_SIXEL 2
#define INLINE_IMG_ITERM2 3
#define INLINE_IMG_KITTY 4

extern int enable_inline_image;

enum ImageCacheFlags {
    IMG_FLAG_UNLOADED = 0,
    IMG_FLAG_LOADED = 1,
    IMG_FLAG_ERROR = 2,
    IMG_FLAG_DONT_REMOVE = 4,
};

typedef struct _imageCache {
    char* url;
    struct _ParsedURL* current;
    char* file;
    char* touch;
    pid_t pid;
    enum ImageCacheFlags loaded;
    int index;
    short width;
    short height;
    short a_width;
    short a_height;
} ImageCache;

typedef struct _image {
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
    ImageCache* cache;
} Image;

void initImage();
void termImage();
void addImage(ImageCache* cache, int x, int y, int sx, int sy, int w, int h);
void drawImage();
void clearImage();

struct _Buffer;
void deleteImage(struct _Buffer* buf);
void getAllImage(struct _Buffer* buf);

enum ImageLoadFlag {
    IMG_FLAG_START = 0,
    IMG_FLAG_STOP = 1,
    IMG_FLAG_NEXT = 2,
};
void loadImage(struct _Buffer* buf, enum ImageLoadFlag flag);

enum ImageGetFlag {
    IMG_FLAG_SKIP = 1,
    IMG_FLAG_AUTO = 2,
};
ImageCache* getImage(Image* image, struct _ParsedURL* current, enum ImageGetFlag flag);

int getImageSize(ImageCache* cache);

void put_image_osc5379(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
void put_image_sixel(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
void put_image_iterm2(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h);
void put_image_kitty(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);
