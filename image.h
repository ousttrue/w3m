#pragma once
#include "Str.h"

struct _Buffer;
struct _imageCache;
void initImage(void);
void termImage(void);
void addImage(struct _imageCache* cache, int x, int y, int sx, int sy, int w, int h);
void drawImage(struct _Buffer* currentbuf);
void clearImage(void);

struct _Buffer;
extern void deleteImage(struct _Buffer* buf);
extern void getAllImage(struct _Buffer* buf);

#define IMG_FLAG_START 0
#define IMG_FLAG_STOP 1
#define IMG_FLAG_NEXT 2
extern void loadImage(struct _Buffer* buf, int flag);

struct _image;
struct _ParsedURL;
struct _imageCache* getImage(struct _image* image, struct _ParsedURL* current, int flag);
int getImageSize(struct _imageCache* cache);

