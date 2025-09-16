#pragma once
#include "geometry.h"
#include "Image.h"

extern const char* image_source;
extern char* Imgdisplay;
extern int useExtImageViewer;
extern int maxLoadImage;
extern int image_map_list;

void initImage();
void termImage();
void addImage(struct ImageCache* cache, int x, int y, int sx, int sy, int w, int h);
void drawImage();
void clearImage();

struct Document;
void deleteImage(struct Document* doc);
void getAllImage(struct Document* doc);

enum ImageLoadFlag {
    IMG_FLAG_START = 0,
    IMG_FLAG_STOP = 1,
    IMG_FLAG_NEXT = 2,
};
void loadImage(struct UI ui, struct Document* doc, enum ImageLoadFlag flag, bool do_download);
struct ImageCache* getImageCache(struct Image* image, struct Url* current, enum ImageGetFlag flag);

int getImageSize(struct ImageCache* cache);

void put_image_osc5379(
    const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh);
void put_image_sixel(
    const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int n_terminal_image);
void put_image_iterm2(
    const char* url, int x, int y, int w, int h);
void put_image_kitty(
    const char* url, int x, int y, int w, int h, int sx, int sy, int sw, int sh, int c, int r);
