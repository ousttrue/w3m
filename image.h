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

struct ImageCache {
    const char* url;
    struct Url* current;
    const char* file;
    const char* touch;
    pid_t pid;
    bool loaded;
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

struct Buffer;
struct Url;
struct CmdArgs;

extern void initImage(void);
extern void termImage(void);
extern void addImage(struct ImageCache* cache, int x, int y, int sx, int sy, int w, int h);
extern void drawImage(void);
extern void clearImage(void);
extern void deleteImage(struct Buffer* buf);
extern void getAllImage(struct Buffer* buf);
extern void loadImage(struct Buffer* buf, int flag);
extern struct ImageCache* getImage(struct Image* image, struct Url* current, int flag);
extern int getImageSize(struct ImageCache* cache);
void put_image_osc5379(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
void put_image_sixel(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
void put_image_iterm2(const char* url, int x, int y, int w, int h);
void put_image_kitty(const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);


