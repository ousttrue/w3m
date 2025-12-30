#pragma once
#include <sys/types.h>

#define INLINE_IMG_NONE 0
#define INLINE_IMG_OSC5379 1
#define INLINE_IMG_SIXEL 2
#define INLINE_IMG_ITERM2 3
#define INLINE_IMG_KITTY 4

#define IMG_FLAG_UNLOADED 0
#define IMG_FLAG_LOADED 1
#define IMG_FLAG_ERROR 2
#define IMG_FLAG_DONT_REMOVE 4

#define MAX_IMAGE 1000
#define MAX_IMAGE_SIZE 2048

#define DEFAULT_PIXEL_PER_CHAR 7.0 /* arbitrary */
#define DEFAULT_PIXEL_PER_LINE 14.0 /* arbitrary */

#define MINIMUM_PIXEL_PER_CHAR 4.0
#define MAXIMUM_PIXEL_PER_CHAR 32.0

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

struct Buffer;
void initImage(void);
void termImage(void);
void addImage(struct ImageCache* cache, int x, int y, int sx, int sy, int w, int h);
void drawImage(struct Buffer* currentbuf);
void clearImage(void);

struct Buffer;
extern void deleteImage(struct Buffer* buf);
// extern void getAllImage(struct Buffer* buf);

#define IMG_FLAG_START 0
#define IMG_FLAG_STOP 1
#define IMG_FLAG_NEXT 2
extern void loadImage(struct Buffer* buf, int flag);

enum ImageGetFlags {
    IMG_FLAG_SKIP = 1,
    IMG_FLAG_AUTO = 2,
};
struct ImageCache* getImage(struct Image* image, struct Url* current, enum ImageGetFlags flag);

int getImageSize(struct ImageCache* cache);
