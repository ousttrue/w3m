#pragma once
#include <sys/types.h>

typedef struct _imageCache {
    char* url;
    struct _ParsedURL* current;
    char* file;
    char* touch;
    pid_t pid;
    char loaded;
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
void loadImage(struct _Buffer* buf, int flag);
ImageCache* getImage(Image* image, struct _ParsedURL* current, int flag);
int getImageSize(ImageCache* cache);

void put_image_osc5379(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
void put_image_sixel(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
void put_image_iterm2(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h);
void put_image_kitty(int cursorX, int cursorY,
    char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);
