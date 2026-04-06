#pragma once
#include <sys/types.h>

#define MAX_IMAGE 1000
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

typedef struct _imageCache {
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

typedef struct _image {
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

struct Buffer;
struct Url;

extern void initImage(void);
extern void termImage(void);
extern void addImage(struct _imageCache* cache, int x, int y, int sx, int sy, int w, int h);
extern void drawImage(void);
extern void clearImage(void);
extern void deleteImage(struct Buffer* buf);
extern void getAllImage(struct Buffer* buf);
extern void loadImage(struct Buffer* buf, int flag);
extern struct _imageCache* getImage(struct _image* image, struct Url* current, int flag);
extern int getImageSize(struct _imageCache* cache);
