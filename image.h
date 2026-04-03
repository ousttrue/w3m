#pragma once

struct _Buffer;
struct _image;
struct _ParsedURL;
struct _imageCache;

extern void initImage(void);
extern void termImage(void);
extern void addImage(struct _imageCache* cache, int x, int y, int sx, int sy, int w, int h);
extern void drawImage(void);
extern void clearImage(void);
extern void deleteImage(struct _Buffer* buf);
extern void getAllImage(struct _Buffer* buf);
extern void loadImage(struct _Buffer* buf, int flag);
extern struct _imageCache* getImage(struct _image* image, struct _ParsedURL* current, int flag);
extern int getImageSize(struct _imageCache* cache);
