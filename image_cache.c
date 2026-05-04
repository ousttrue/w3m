#include "image_cache.h"
#include "input_stream.h"
#include <w3m.h>
#include "file.h"
#include "buffer.h"
#include "anchor.h"
#include "constants.h"
#include "alloc.h"
#include "global.h"
#include "Str.h"
#include "hash.h"
#include "textlist.h"
#include "term_tty.h"

#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define MAX_LOAD_IMAGE 8
#define MAX_IMAGE 1000

static Hash_sv* image_file = NULL;
static Hash_sv* image_hash = NULL;
static GeneralList* image_list = NULL;
static int image_index = 0;
static int n_load_image = 0;
static struct ImageCache** image_cache = NULL;
static struct Buffer* image_buffer = NULL;

struct ImageCache* getImage(struct Image* image, struct Url* current, enum GetImageFlag flag)
{
    if (!image_hash)
        image_hash = newHash_sv(100);

    struct ImageCache* cache;
    Str key = NULL;
    if (image->cache)
        cache = image->cache;
    else {
        key = Sprintf("%d;%d;%s", image->width, image->height, image->url);
        cache = (struct ImageCache*)getHash_sv(image_hash, key->ptr, NULL);
    }
    if (cache && cache->index && abs(cache->index) <= image_index - MAX_IMAGE) {
        struct stat st;
        if (stat(cache->file, &st))
            cache->loaded = IMG_FLAG_UNLOADED;
        cache->index = 0;
    }

    if (!cache) {
        if (flag == IMG_FLAG_SKIP)
            return NULL;

        cache = New(struct ImageCache);
        cache->url = image->url;
        cache->current = current;
        cache->file = tmpfname(TMPF_DFL, image->ext);
        cache->index = 0;
        cache->loaded = IMG_FLAG_UNLOADED;
        if (enable_inline_image == INLINE_IMG_OSC5379) {
            if (image->width > 0 && image->width % pixel_per_char_i > 0)
                image->width += (pixel_per_char_i - image->width % pixel_per_char_i);

            if (image->height > 0 && image->height % pixel_per_line_i > 0)
                image->height += (pixel_per_line_i - image->height % pixel_per_line_i);
        }
        cache->touch = tmpfname(TMPF_DFL, NULL);

        cache->width = image->width;
        cache->height = image->height;
        cache->a_width = image->width;
        cache->a_height = image->height;
        putHash_sv(image_hash, key->ptr, (void*)cache);
    }
    if (flag != IMG_FLAG_SKIP) {
        if (cache->loaded == IMG_FLAG_UNLOADED) {
            if (!image_file)
                image_file = newHash_sv(100);
            if (!getHash_sv(image_file, (char*)cache->file, NULL)) {
                putHash_sv(image_file, (char*)cache->file, (void*)cache);
                if (!image_list)
                    image_list = newGeneralList();
                pushValue(image_list, (void*)cache);
            }
        }
        if (!cache->index)
            cache->index = ++image_index;
    }
    if (cache->loaded & IMG_FLAG_LOADED)
        getImageSize(cache);
    return cache;
}

static bool updateCache(struct Buffer* buf)
{
    if (maxLoadImage > MAX_LOAD_IMAGE)
        maxLoadImage = MAX_LOAD_IMAGE;
    else if (maxLoadImage < 1)
        maxLoadImage = 1;
    if (n_load_image == 0)
        n_load_image = maxLoadImage;
    if (!image_cache) {
        image_cache = New_N(struct ImageCache*, MAX_LOAD_IMAGE);
        memset(image_cache, 0, sizeof(struct ImageCache*) * MAX_LOAD_IMAGE);
    }

    bool draw = false;
    for (int i = 0; i < n_load_image; i++) {
        struct ImageCache* cache = image_cache[i];
        if (!cache || !cache->touch)
            continue;
        struct stat st;
        if (lstat(cache->touch, &st))
            continue;
        if (!stat(cache->file, &st)) {
            cache->loaded = IMG_FLAG_LOADED;
            if (getImageSize(cache)) {
                if (image_buffer)
                    image_buffer->need_reshape = true;
            }
            draw = true;
        } else
            cache->loaded = IMG_FLAG_ERROR;
        unlink(cache->touch);
        image_cache[i] = NULL;
    }

    for (int i = (buf != image_buffer) ? 0 : maxLoadImage; i < n_load_image; i++) {
        struct ImageCache* cache = image_cache[i];
        if (!cache || !cache->touch)
            continue;

        /*TODO make sure removing this didn't break anything
        unlink(cache->touch);
        */
        image_cache[i] = NULL;
    }

    return draw;
}

void loadImageStop(struct Buffer* buf)
{
    updateCache(buf);

    image_list = NULL;
    image_file = NULL;
    n_load_image = maxLoadImage;
    image_buffer = NULL;
    return;
}

void loadImageStart(struct Buffer* buf)
{
    bool draw = updateCache(buf);

    if (draw && image_buffer) {
        if (!enable_inline_image)
            drawImage();
        showImageProgress(image_buffer);
    }

    image_buffer = buf;

    if (!image_list)
        return;
    for (int i = 0; i < n_load_image; i++) {
        if (image_cache[i])
            continue;

        struct ImageCache* cache = 0;
        while (1) {
            cache = (struct ImageCache*)popValue(image_list);
            if (!cache) {
                for (i = 0; i < n_load_image; i++) {
                    if (image_cache[i])
                        return;
                }
                image_list = NULL;
                image_file = NULL;
                // if (image_buffer)
                //     displayBuffer(args, image_buffer, B_NORMAL);
                return;
            }
            if (cache->loaded == IMG_FLAG_UNLOADED)
                break;
        }
        image_cache[i] = cache;
        if (!cache->touch) {
            continue;
        }

        // tty_flush();
        // if ((cache->pid = fork()) == 0) {
        //     /*
        //      * setup_child(TRUE, 0, -1);
        //      */
        //     setup_child(FALSE, 0, -1);
        struct HttpClient http = http_get(0, cache->url, cache->current, NULL, NULL, 0, cache->file);
        struct HttpMessageSession* current = http_session_current(&http);
        // load_http(0, &http, cache->file);
        // if (image_source) {
        // struct Buffer* b = NULL;
        ist_save2tmp(current->transport.stream, current->transport.url.scheme, cache->file);
        // b = newBuffer(INIT_BUFFER_WIDTH);
        // b->sourcefile = image_source;
        // b->real_type = current->t;
        UFclose(&current->transport);
        // TRAP_OFF;
        //     return b;
        // }

        /* TODO make sure removing this didn't break anything
        if (!b || !b->real_type || strncasecmp(b->real_type, "image/", 6))
            unlink(cache->file);
        */
        symlink(cache->file, cache->touch);
        //     exit(0);
        // } else if (cache->pid < 0) {
        //     cache->pid = 0;
        //     return;
        // }
    }
}

void getAllImage(struct Buffer* buf)
{
    struct AnchorList* al;
    struct Anchor* a;
    struct Url* current;
    int i;

    image_buffer = buf;
    if (!buf)
        return;
    buf->image_loaded = TRUE;
    al = buf->img;
    if (!al)
        return;
    current = baseURL(buf);
    for (i = 0, a = al->anchors; i < al->nanchor; i++, a++) {
        if (a->image) {
            a->image->cache = getImage(a->image, current, buf->image_flag);
            if (a->image->cache && a->image->cache->loaded == IMG_FLAG_UNLOADED)
                buf->image_loaded = FALSE;
        }
    }
}

void deleteImage(struct Buffer* buf)
{
    if (!buf)
        return;

    struct AnchorList* al = buf->img;
    if (!al)
        return;

    struct Anchor* a = al->anchors;
    for (int i = 0; i < al->nanchor; i++, a++) {
        if (a->image && a->image->cache && a->image->cache->loaded != IMG_FLAG_UNLOADED && !(a->image->cache->loaded & IMG_FLAG_DONT_REMOVE) && a->image->cache->index < 0)
            unlink(a->image->cache->file);
    }
    loadImageStop(NULL);
}
