#include "alloc.h"
#include "Str.h"
#include <gc.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GC_warn_proc orig_GC_warn_proc = NULL;

#define GC_WARN_KEEP_MAX (20)

static void
wrap_GC_warn_proc(const char* msg, GC_word arg)
{
    // if (fmInitialized)
    // {
    //     /* *INDENT-OFF* */
    //     static struct {
    //         char* msg;
    //         GC_word arg;
    //     } msg_ring[GC_WARN_KEEP_MAX];
    //     /* *INDENT-ON* */
    //     static int i = 0;
    //     static int n = 0;
    //     static int lock = 0;
    //     int j;
    //
    //     j = (i + n) % (sizeof(msg_ring) / sizeof(msg_ring[0]));
    //     msg_ring[j].msg = msg;
    //     msg_ring[j].arg = arg;
    //
    //     if (n < sizeof(msg_ring) / sizeof(msg_ring[0]))
    //         ++n;
    //     else
    //         ++i;
    //
    //     if (!lock) {
    //         lock = 1;
    //
    //         for (; n > 0; --n, ++i) {
    //             i %= sizeof(msg_ring) / sizeof(msg_ring[0]);
    //
    //             printf(msg_ring[i].msg, (unsigned long)msg_ring[i].arg);
    //             // sleep_till_anykey(1, 1);
    //         }
    //
    //         lock = 0;
    //     }
    // }
    // else if (orig_GC_warn_proc)
    //     orig_GC_warn_proc(msg, arg);
    // else
    fprintf(stderr, msg, (unsigned long)arg);
}

static void*
die_oom(size_t bytes)
{
    fprintf(stderr, "Out of memory: %lu bytes unavailable!\n", (unsigned long)bytes);
    exit(1);
    /*
     * Suppress compiler warning: function might return no value
     * This code is never reached.
     */
    return NULL;
}

void alloc_init()
{
    GC_INIT();
    GC_set_oom_fn(die_oom);

    orig_GC_warn_proc = GC_get_warn_proc();
    GC_set_warn_proc(wrap_GC_warn_proc);
}

size_t z_mult_no_oflow_(size_t n, size_t size)
{
    if (size != 0 && n > ULONG_MAX / size) {
        fprintf(stderr,
            "w3m: overflow in malloc, %lu*%lu\n", (unsigned long)n, (unsigned long)size);
        exit(1);
    }
    return n * size;
}

void* w3m_GC_alloc(size_t size)
{
    return GC_MALLOC(size);
}

void* w3m_GC_alloc_atomic(size_t size)
{
    return GC_MALLOC_ATOMIC(size);
}

void* w3m_GC_realloc(void* ptr, size_t size)
{
    return GC_REALLOC(ptr, size);
}

void* w3m_GC_realloc_atomic(void* ptr, size_t size)
{
    return ptr ? GC_REALLOC(ptr, size) : GC_MALLOC_ATOMIC(size);
}

void w3m_GC_free(void* ptr)
{
    GC_FREE(ptr);
}

char* allocStr(const char* s, int len)
{
    char* ptr;

    if (s == NULL)
        return NULL;
    if (len < 0)
        len = strlen(s);
    if (len < 0 || len >= STR_SIZE_MAX)
        len = STR_SIZE_MAX - 1;
    ptr = NewAtom_N(char, len + 1);
    if (ptr == NULL) {
        fprintf(stderr, "fm: Can't allocate string. Give me more memory!\n");
        exit(-1);
    }
    bcopy(s, ptr, len);
    ptr[len] = '\0';
    return ptr;
}

void* xrealloc(void* ptr, size_t size)
{
    void* newptr = realloc(ptr, size);
    if (newptr == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(-1);
    }
    return newptr;
}

/* Define this as a separate function in case the free() has
 * an incompatible prototype. */
void xfree(void* ptr)
{
    free(ptr);
}

void growbuf_init(struct growbuf* gb)
{
    gb->ptr = NULL;
    gb->length = 0;
    gb->area_size = 0;
    gb->realloc_proc = &w3m_GC_realloc_atomic;
    gb->free_proc = &w3m_GC_free;
}

void growbuf_init_without_GC(struct growbuf* gb)
{
    gb->ptr = NULL;
    gb->length = 0;
    gb->area_size = 0;
    gb->realloc_proc = &xrealloc;
    gb->free_proc = &xfree;
}

void growbuf_clear(struct growbuf* gb)
{
    (*gb->free_proc)(gb->ptr);
    gb->ptr = NULL;
    gb->length = 0;
    gb->area_size = 0;
}

Str growbuf_to_Str(struct growbuf* gb)
{
    Str s;

    if (gb->free_proc == &w3m_GC_free) {
        growbuf_reserve(gb, gb->length + 1);
        gb->ptr[gb->length] = '\0';
        s = New(struct _Str);
        s->ptr = gb->ptr;
        s->length = gb->length;
        s->area_size = gb->area_size;
    } else {
        s = Strnew_charp_n(gb->ptr, gb->length);
        (*gb->free_proc)(gb->ptr);
    }
    gb->ptr = NULL;
    gb->length = 0;
    gb->area_size = 0;
    return s;
}

void growbuf_reserve(struct growbuf* gb, int leastarea)
{
    int newarea;

    if (gb->area_size < leastarea) {
        newarea = gb->area_size * 3 / 2;
        if (newarea < leastarea)
            newarea = leastarea;
        newarea += 16;
        gb->ptr = (*gb->realloc_proc)(gb->ptr, newarea);
        gb->area_size = newarea;
    }
}

void growbuf_append(struct growbuf* gb, const unsigned char* src, int len)
{
    growbuf_reserve(gb, gb->length + len);
    memcpy(&gb->ptr[gb->length], src, len);
    gb->length += len;
}
