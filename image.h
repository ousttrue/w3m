#ifndef W3M_IMAGE_H
#define W3M_IMAGE_H

#include "config.h"
#include "html.h"
#include "http_request.h"

#include <sys/types.h>

#define IMG_FLAG_SKIP	1
#define IMG_FLAG_AUTO	2

#define IMG_FLAG_START	0
#define IMG_FLAG_STOP	1
#define IMG_FLAG_NEXT	2

#define IMG_FLAG_UNLOADED	0
#define IMG_FLAG_LOADED		1
#define IMG_FLAG_ERROR		2
#define IMG_FLAG_DONT_REMOVE	4

#define INLINE_IMG_NONE		0
#define INLINE_IMG_OSC5379	1
#define INLINE_IMG_SIXEL	2
#define INLINE_IMG_ITERM2	3
#define INLINE_IMG_KITTY	4

#ifdef USE_IMAGE
typedef struct {
    char *url;
    ParsedURL *current;
    char *file;
    char *touch;
    pid_t pid;
    char loaded;
    int index;
    short width;
    short height;
    short a_width;
    short a_height;
} ImageCache;

typedef struct {
    char *url;
    char *ext;
    short width;
    short height;
    short xoffset;
    short yoffset;
    short y;
    short rows;
    char *map;
    char ismap;
    int touch;
    ImageCache *cache;
} Image;
#endif

#endif
