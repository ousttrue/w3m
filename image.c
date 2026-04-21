#include "image.h"
#include "image_cache.h"
#include "global.h"
#include "constants.h"
#include "alloc.h"
#include "terms.h"
#include "buffer.h"
#include "anchor.h"
#include <unistd.h>
#include <sys/stat.h>

struct TerminalImage {
    struct ImageCache* cache;
    short x;
    short y;
    short sx;
    short sy;
    short width;
    short height;
};

static struct TerminalImage* terminal_image = NULL;
static int n_terminal_image = 0;
static int max_terminal_image = 0;

void termImage()
{
    if (!activeImage)
        return;
    clearImage();
}

void clearImage()
{
    if (!activeImage)
        return;
    if (!n_terminal_image)
        return;
    n_terminal_image = 0;
    return;
}

void addImage(struct ImageCache* cache, int x, int y, int sx, int sy, int w, int h)
{
    if (!activeImage)
        return;

    if (n_terminal_image >= max_terminal_image) {
        max_terminal_image = max_terminal_image ? (2 * max_terminal_image) : 8;
        terminal_image = New_Reuse(struct TerminalImage, terminal_image,
            max_terminal_image);
    }

    struct TerminalImage* i = &terminal_image[n_terminal_image];
    *i = (struct TerminalImage) {
        .cache = cache,
        .x = x,
        .y = y,
        .sx = sx,
        .sy = sy,
        .width = w,
        .height = h,
    };
    n_terminal_image++;
}

void drawImage(void)
{
    if (!activeImage)
        return;
    if (!n_terminal_image)
        return;

    for (int j = 0; j < n_terminal_image; j++) {
        struct TerminalImage* i = &terminal_image[j];
        if (enable_inline_image) {
            /*
             * So this shouldn't ever happen, but if it does then at least let's
             * not have external programs fetch images from the Internet...
             */
            struct stat st;
            if (!i->cache->touch || stat(i->cache->file, &st))
                return;

            const char* url = i->cache->file;

            int x = i->x / pixel_per_char_i;
            int y = i->y / pixel_per_line_i;

            int w = i->cache->a_width > 0 ? (
                                                (i->cache->width + i->x % pixel_per_char_i + pixel_per_char_i - 1) / pixel_per_char_i)
                                          : 0;
            int h = i->cache->a_height > 0 ? (
                                                 (i->cache->height + i->y % pixel_per_line_i + pixel_per_line_i - 1) / pixel_per_line_i)
                                           : 0;

            int sx = i->sx / pixel_per_char_i;
            int sy = i->sy / pixel_per_line_i;

            int sw = (i->width + i->sx % pixel_per_char_i + pixel_per_char_i - 1) / pixel_per_char_i;
            int sh = (i->height + i->sy % pixel_per_line_i + pixel_per_line_i - 1) / pixel_per_line_i;

            if (enable_inline_image == INLINE_IMG_SIXEL) {
                w = i->cache->a_width > 0 ? i->width : 0;
                h = i->cache->a_height > 0 ? i->height : 0;
                put_image_sixel(url, x, y, w, h, i->sx, i->sy, sw * pixel_per_char, sh * pixel_per_line_i, n_terminal_image);
            } else if (enable_inline_image == INLINE_IMG_OSC5379) {
                put_image_osc5379(url, x, y, w, h, sx, sy, sw, sh);
            } else if (enable_inline_image == INLINE_IMG_ITERM2) {
                put_image_iterm2(url, x, y, sw, sh);
            } else if (enable_inline_image == INLINE_IMG_KITTY) {
                put_image_kitty(url, x, y, i->width, i->height, i->sx, i->sy, sw * pixel_per_char, sh * pixel_per_line_i, sw, sh);
            }

            continue;
        }
    }

    n_terminal_image = 0;

    touch_cursor();
    refresh();
}
