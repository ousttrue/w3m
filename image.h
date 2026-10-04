#pragma once
#include <stdbool.h>
#include <sys/types.h>

extern bool activeImage;
extern const char* image_source;

#define MAX_IMAGE_SIZE 2048

#define IMG_FLAG_SKIP 1
#define IMG_FLAG_AUTO 2

#define IMG_FLAG_START 0
#define IMG_FLAG_STOP 1
#define IMG_FLAG_NEXT 2

#define IMG_FLAG_UNLOADED 0
#define IMG_FLAG_LOADED 1
#define IMG_FLAG_ERROR 2
#define IMG_FLAG_DONT_REMOVE 4

#define INLINE_IMG_NONE 0
#define INLINE_IMG_OSC5379 1
#define INLINE_IMG_SIXEL 2
#define INLINE_IMG_ITERM2 3
#define INLINE_IMG_KITTY 4

typedef struct {
    const char* url;
    struct Url* current;
    const char* file;
    const char* touch;
    pid_t pid;
    char loaded;
    int index;
    short width;
    short height;
    short a_width;
    short a_height;
} ImageCache;

typedef struct {
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
    ImageCache* cache;
} Image;

struct _Buffer;
extern void deleteImage(struct _Buffer* buf);
extern void getAllImage(struct _Buffer* buf);
extern void loadImage(struct _Buffer* buf, int flag);
extern ImageCache* getImage(Image* image, struct Url* current, int flag);
extern int getImageSize(ImageCache* cache);

