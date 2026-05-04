#include "image_cache.h"
#include <w3m.h>
#include "file.h"
#include "buffer.h"
#include "anchor.h"
#include "constants.h"
#include "alloc.h"
#include "etc.h"
#include "global.h"
#include "Str.h"
#include "indep.h"
#include "hash.h"
#include "textlist.h"
#include "term_tty.h"

#include <w3m.h>

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
    if (!activeImage)
        return NULL;
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
        cache->pid = 0;
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
            if (!getHash_sv(image_file, cache->file, NULL)) {
                putHash_sv(image_file, cache->file, (void*)cache);
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

static bool parseImageHeader(const char* path, uint32_t* width, uint32_t* height)
{
    FILE* fp = fopen(path, "r");
    if (!fp)
        return false;

    uint8_t buf[8];
    if (fread(buf, 1, 2, fp) != 2)
        goto error;

    if (memcmp(buf, "\xff\xd8", 2) == 0) {
        /* JPEG */
        if (fseek(fp, 2, SEEK_CUR) < 0)
            goto error; /* 0xffe0 */
        while (fread(buf, 1, 2, fp) == 2) {
            size_t len = ((buf[0] << 8) | buf[1]) - 2;
            if (fseek(fp, len, SEEK_CUR) < 0)
                goto error;
            if (fread(buf, 1, 2, fp) == 2 &&
                /* SOF0 or SOF2 */
                (memcmp(buf, "\xff\xc0", 2) == 0 || memcmp(buf, "\xff\xc2", 2) == 0)) {
                fseek(fp, 3, SEEK_CUR);
                if (fread(buf, 1, 2, fp) == 2) {
                    *height = (buf[0] << 8) | buf[1];
                    if (fread(buf, 1, 2, fp) == 2) {
                        *width = (buf[0] << 8) | buf[1];
                        goto success;
                    }
                }
                break;
            }
        }
        goto error;
    }

    if (fread(buf + 2, 1, 1, fp) != 1)
        goto error;

    if (memcmp(buf, "GIF", 3) == 0) {
        /* GIF */
        if (fseek(fp, 3, SEEK_CUR) < 0)
            goto error;
        if (fread(buf, 1, 2, fp) == 2) {
            *width = (buf[1] << 8) | buf[0];
            if (fread(buf, 1, 2, fp) == 2) {
                *height = (buf[1] << 8) | buf[0];
                goto success;
            }
        }
        goto error;
    }

    if (fread(buf + 3, 1, 5, fp) != 5)
        goto error;

    if (memcmp(buf, "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a", 8) == 0) {
        /* PNG */
        if (fseek(fp, 8, SEEK_CUR) < 0)
            goto error;
        if (fread(buf, 1, 4, fp) == 4) {
            *width = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
            if (fread(buf, 1, 4, fp) == 4) {
                *height = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
                goto success;
            }
        }
        goto error;
    }

error:
    fclose(fp);
    return false;

success:
    fclose(fp);
    return true;
}

bool getImageSize(struct ImageCache* cache)
{
    unsigned int w = 0, h = 0;

    if (!activeImage)
        return false;
    if (!cache || !(cache->loaded & IMG_FLAG_LOADED) || (cache->width > 0 && cache->height > 0))
        return false;

    if (parseImageHeader(cache->file, &w, &h)) {
    } else {

        Str tmp = Strnew();
        if (!strchr(Imgdisplay, '/'))
            Strcat_m_charp(tmp, w3m_auxbin_dir(), "/", NULL);
        Strcat_m_charp(tmp, Imgdisplay, " -size ", shell_quote(cache->file), NULL);

        FILE* f = popen(tmp->ptr, "r");
        if (!f)
            return false;
        while (fscanf(f, "%u %u", &w, &h) < 0) {
            if (feof(f))
                break;
        }
        pclose(f);

        if (!(w > 0 && h > 0))
            return false;
    }

    w = (int)(w * image_scale / 100 + 0.5);
    if (w == 0)
        w = 1;
    h = (int)(h * image_scale / 100 + 0.5);
    if (h == 0)
        h = 1;
    if (cache->width < 0 && cache->height < 0) {
        cache->width = (w > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : w;
        cache->height = (h > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : h;
    } else if (cache->width < 0) {
        int tmp = (int)((double)cache->height * w / h + 0.5);
        cache->a_width = cache->width = (tmp > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : tmp;
    } else if (cache->height < 0) {
        int tmp = (int)((double)cache->width * h / w + 0.5);
        cache->a_height = cache->height = (tmp > MAX_IMAGE_SIZE) ? MAX_IMAGE_SIZE : tmp;
    }
    if (cache->width == 0)
        cache->width = 1;
    if (cache->height == 0)
        cache->height = 1;

    Str tmp = Sprintf("%d;%d;%s", cache->width, cache->height, cache->url);
    putHash_sv(image_hash, tmp->ptr, (void*)cache);
    return true;
}

void loadImage(struct Buffer* buf, enum ImageLoadFlag flag)
{
    /* int wait_st; */

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
        if (cache->pid) {
            kill(cache->pid, SIGKILL);
            /*
             * #ifdef HAVE_WAITPID
             * waitpid(cache->pid, &wait_st, 0);
             * #else
             * wait(&wait_st);
             * #endif
             */
            cache->pid = 0;
        }
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
        if (cache->pid) {
            kill(cache->pid, SIGKILL);
            /*
             * #ifdef HAVE_WAITPID
             * waitpid(cache->pid, &wait_st, 0);
             * #else
             * wait(&wait_st);
             * #endif
             */
            cache->pid = 0;
        }
        /*TODO make sure removing this didn't break anything
        unlink(cache->touch);
        */
        image_cache[i] = NULL;
    }

    if (flag == IMG_FLAG_STOP) {
        image_list = NULL;
        image_file = NULL;
        n_load_image = maxLoadImage;
        image_buffer = NULL;
        return;
    }

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

        tty_flush();
        // if ((cache->pid = fork()) == 0) {
        //     /*
        //      * setup_child(TRUE, 0, -1);
        //      */
        //     setup_child(FALSE, 0, -1);
            image_source = cache->file;
            struct HttpClient http = http_get(0, cache->url, cache->current, NULL, NULL, 0);
            load_http(0, &http);
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
    loadImage(NULL, IMG_FLAG_STOP);
}
