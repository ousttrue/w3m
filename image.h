#pragma once

struct _Buffer;
extern void deleteImage(struct _Buffer* buf);
extern void getAllImage(struct _Buffer* buf);
extern void loadImage(struct _Buffer* buf, int flag);
struct _image;
struct _ParsedURL;
extern struct _imageCache* getImage(struct _image* image, struct _ParsedURL* current, int flag);
extern int getImageSize(struct _imageCache* cache);


