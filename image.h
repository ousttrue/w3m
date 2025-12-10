#pragma once
#include "Url.h"
#include <sys/types.h>

#define MAX_IMAGE 1000
#define MAX_IMAGE_SIZE 2048

#define INLINE_IMG_NONE 0
#define INLINE_IMG_OSC5379 1
#define INLINE_IMG_SIXEL 2
#define INLINE_IMG_ITERM2 3
#define INLINE_IMG_KITTY 4

#define MINIMUM_PIXEL_PER_CHAR 4.0
#define MAXIMUM_PIXEL_PER_CHAR 32.0

extern double pixel_per_char;
extern int pixel_per_char_i;
extern int set_pixel_per_char;
extern double pixel_per_line;
extern int pixel_per_line_i;
extern int set_pixel_per_line;
extern double image_scale;
extern int activeImage;
extern int displayImage;

#define IMG_FLAG_UNLOADED 0
#define IMG_FLAG_LOADED 1
#define IMG_FLAG_ERROR 2
#define IMG_FLAG_DONT_REMOVE 4

struct ImageCache {
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

extern const char* Imgdisplay;

enum ImageGetFlags {
    IMG_FLAG_SKIP = 1,
    IMG_FLAG_AUTO = 2,
};
struct ImageCache* getImage(struct Image* image, struct Url* current, enum ImageGetFlags flag);
int getImageSize(struct ImageCache* cache);
struct Buffer;

enum ImageLoadFlag {
    IMG_FLAG_START = 0,
    IMG_FLAG_STOP = 1,
    IMG_FLAG_NEXT = 2,
};
void loadImage(struct Buffer* buf, enum ImageLoadFlag flag);

void initImage(void);
void termImage(void);
void addImage(struct ImageCache* cache, int x, int y, int sx, int sy, int w,
    int h);
void drawImage(void);
void clearImage(void);

void put_image_osc5379(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
void put_image_sixel(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
void put_image_iterm2(const char* url, int x, int y, int w, int h);
void put_image_kitty(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);
