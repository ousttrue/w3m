#pragma once
#include <sys/types.h>

extern int enable_inline_image;
extern int activeImage;
extern const char* image_source;
extern char* Imgdisplay;
extern int useExtImageViewer;
extern int maxLoadImage;
extern int image_map_list;
extern double image_scale;

#define MAX_IMAGE 1000
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

void initImage();
void termImage();
void addImage(struct ImageCache* cache, int x, int y, int sx, int sy, int w, int h);
void drawImage();
void clearImage();

struct Buffer;
void deleteImage(struct Buffer* buf);
void getAllImage(struct Buffer* buf);

enum ImageLoadFlag {
    IMG_FLAG_START = 0,
    IMG_FLAG_STOP = 1,
    IMG_FLAG_NEXT = 2,
};
void loadImage(struct Buffer* buf, enum ImageLoadFlag flag, bool do_download);

enum ImageGetFlag {
    IMG_FLAG_SKIP = 1,
    IMG_FLAG_AUTO = 2,
};
struct ImageCache* getImage(struct Image* image, struct Url* current, enum ImageGetFlag flag);

int getImageSize(struct ImageCache* cache);

void put_image_osc5379(
    const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
void put_image_sixel(
    const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
void put_image_iterm2(
    const char* url, int x, int y, int w, int h);
void put_image_kitty(
    const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);
