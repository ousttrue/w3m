#include "input_stream.h"
#include "alloc.h"
#include "str_const.h"
#include "w3m.h"
#include "w3m_tty.h"
#include "gettext_helper.h"
#include "proto.h"
#include "http_request.h"
#include "linein.h"
#include <signal.h>
#include <openssl/x509v3.h>
#include <sys/stat.h>

#define STREAM_BUF_SIZE 8192
#define SSL_BUF_SIZE 1536

static bool MUST_BE_UPDATED(struct input_stream* is)
{
    return is->stream.cur == is->stream.next;
}

static void
do_update(struct input_stream* is)
{
    is->stream.cur = is->stream.next = 0;
    int len = (*is->read)(is->handle, is->stream.buf, is->stream.size);
    if (len <= 0)
        is->iseos = true;
    else
        is->stream.next += len;
}

static int
buffer_read(struct stream_buffer* sb, unsigned char* obuf, int count)
{
    int len = sb->next - sb->cur;
    if (len > 0) {
        if (len > count)
            len = count;
        memmove(obuf, (const void*)&sb->buf[sb->cur], len);
        sb->cur += len;
    }
    return len;
}

static void
init_buffer(struct input_stream* is, const char* buf, int bufsize)
{
    struct stream_buffer* sb = &is->stream;
    sb->size = bufsize;
    sb->cur = 0;
    sb->buf = NewWithoutGC_N(unsigned char, bufsize);
    if (buf) {
        memcpy(sb->buf, buf, bufsize);
        sb->next = bufsize;
    } else {
        sb->next = 0;
    }
    is->iseos = false;
}

static void
init_base_stream(struct input_stream* is, int bufsize)
{
    init_buffer(is, NULL, bufsize);
}

static void
init_str_stream(struct input_stream* is, pStr s)
{
    init_buffer(is, s->ptr, s->len);
}

// int file descriptor
static int
basic_read(void* handle, unsigned char* buf, int len)
{
    int* fd = (int*)handle;
    return read(*fd, buf, len);
}

static void
basic_close(void* handle)
{
    int* fd = (int*)handle;
    close(*fd);
    free(handle);
}

struct input_stream*
newInputStream(int des)
{
    if (des < 0)
        return NULL;
    struct input_stream* is = NewWithoutGC(struct input_stream);
    init_base_stream(is, STREAM_BUF_SIZE);
    is->type = IST_BASIC;
    int* fd = NewWithoutGC(int);
    is->handle = fd;
    /* TODO(rkta): Check cast from int to void ptr */
    *fd = des;
    is->read = basic_read;
    is->close = basic_close;
    return is;
}

// FILE*
static int
file_read(void* handle, unsigned char* buf, int len)
{
    FILE* f = (FILE*)handle;
    return fread(buf, 1, len, f);
}

struct input_stream*
newFileStream(FILE* f, int (*closep)(FILE*))
{
    if (f == NULL)
        return NULL;
    struct input_stream* is = NewWithoutGC(struct input_stream);
    init_base_stream(is, STREAM_BUF_SIZE);
    is->type = IST_FILE;
    is->handle = f;
    is->close = (InputStreamCloseFunc)closep;
    is->read = file_read;
    return is;
}

// pStr
static int
str_read(void*, unsigned char* buf, int len)
{
    // NOP
    return 0;
}

struct input_stream*
newStrStream(pStr s)
{
    if (s == NULL)
        return NULL;
    struct input_stream* is = NewWithoutGC(struct input_stream);
    init_str_stream(is, s);
    is->type = IST_STR;
    is->handle = NULL;
    is->read = str_read;
    is->close = NULL;
    return is;
}

// SSL*
static void
ssl_close(void* _handle)
{
    struct ssl_handle* handle = (struct ssl_handle*)_handle;
    close(handle->sock);
    if (handle->ssl)
        SSL_free(handle->ssl);
    free(handle);
}

static int
ssl_read(void* _handle, unsigned char* buf, int len)
{
    struct ssl_handle* handle = (struct ssl_handle*)_handle;
    int status;
    if (handle->ssl) {
        for (;;) {
            status = SSL_read(handle->ssl, buf, len);
            if (status > 0)
                break;
            switch (SSL_get_error(handle->ssl, status)) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE: /* reads can trigger write errors; see SSL_get_error(3) */
                continue;
            default:
                break;
            }
            break;
        }
    } else {
        status = read(handle->sock, buf, len);
    }
    return status;
}

struct input_stream*
newSSLStream(SSL* ssl, int sock)
{
    if (sock < 0)
        return NULL;
    struct input_stream* is = NewWithoutGC(struct input_stream);
    init_base_stream(is, SSL_BUF_SIZE);
    is->type = IST_SSL;
    struct ssl_handle* handle = NewWithoutGC(struct ssl_handle);
    is->handle = handle;
    handle->ssl = ssl;
    handle->sock = sock;
    is->read = ssl_read;
    is->close = ssl_close;
    return is;
}

// encoded(read decoded data)
static void
ens_close(void* _handle)
{
    struct ens_handle* handle = (struct ens_handle*)_handle;
    ISclose(handle->is);
    growbuf_clear(&handle->gb);
    free(handle);
}

static uint32_t
memchop(char* p, uint32_t len)
{
    char* q = p + len;
    for (; q > p; --q) {
        if (q[-1] != '\n' && q[-1] != '\r')
            break;
    }
    if (q != p + len)
        *q = '\0';
    return q - p;
}

static int
ens_read(void* _handle, unsigned char* buf, int len)
{
    struct ens_handle* handle = (struct ens_handle*)_handle;
    if (handle->pos == handle->gb.len) {
        char* p;
        struct growbuf gbtmp;

        ISgets_to_growbuf(handle->is, &handle->gb, true);
        if (handle->gb.len == 0)
            return 0;
        if (handle->encoding == ENC_BASE64)
            handle->gb.len = memchop(handle->gb.ptr, handle->gb.len);
        else if (handle->encoding == ENC_UUENCODE) {
            if (handle->gb.len >= 5 && !strncmp(handle->gb.ptr, "begin", 5))
                ISgets_to_growbuf(handle->is, &handle->gb, true);
            handle->gb.len = memchop(handle->gb.ptr, handle->gb.len);
        }
        growbuf_init_without_GC(&gbtmp);
        p = handle->gb.ptr;
        if (handle->encoding == ENC_QUOTE)
            decodeQP_to_growbuf(&gbtmp, &p);
        else if (handle->encoding == ENC_BASE64)
            decodeB_to_growbuf(&gbtmp, &p);
        else if (handle->encoding == ENC_UUENCODE)
            decodeU_to_growbuf(&gbtmp, &p);
        growbuf_clear(&handle->gb);
        handle->gb = gbtmp;
        handle->pos = 0;
    }

    if (len > handle->gb.len - handle->pos)
        len = handle->gb.len - handle->pos;

    memcpy(buf, &handle->gb.ptr[handle->pos], len);
    handle->pos += len;
    return len;
}

struct input_stream*
newEncodedStream(struct input_stream* inner_stream, enum StreamEncoding encoding)
{
    if (inner_stream == NULL || (encoding != ENC_QUOTE && encoding != ENC_BASE64 && encoding != ENC_UUENCODE))
        return inner_stream;

    struct input_stream* is = NewWithoutGC(struct input_stream);
    init_base_stream(is, STREAM_BUF_SIZE);
    is->type = IST_ENCODED;
    struct ens_handle* handle = NewWithoutGC(struct ens_handle);
    is->handle = handle;
    handle->is = inner_stream;
    handle->pos = 0;
    handle->encoding = encoding;
    growbuf_init_without_GC(&handle->gb);
    is->read = ens_read;
    is->close = ens_close;
    return is;
}

int ISclose(struct input_stream* is)
{
    SigActionFunc prevtrap;
    if (is == NULL)
        return -1;
    if (is->close != NULL) {
        if (is->unclose) {
            return -1;
        }
        prevtrap = mySignal(SIGINT, SIG_IGN);
        is->close(is->handle);
        mySignal(SIGINT, prevtrap);
    }
    free(is->stream.buf);
    free(is);
    return 0;
}

#define POP_CHAR(bs) ((bs)->iseos ? '\0' : (bs)->stream.buf[(bs)->stream.cur++])

int ISgetc(struct input_stream* is)
{
    if (is == NULL)
        return '\0';
    if (!is->iseos && MUST_BE_UPDATED(is))
        do_update(is);
    return POP_CHAR(is);
}

int ISundogetc(struct input_stream* is)
{
    if (is == NULL)
        return -1;
    struct stream_buffer* sb = &is->stream;
    if (sb->cur > 0) {
        sb->cur--;
        return 0;
    }
    return -1;
}

pStr StrISgets2(struct input_stream* is, char crnl)
{
    struct growbuf gb;

    if (is == NULL)
        return NULL;
    growbuf_init(&gb);
    ISgets_to_growbuf(is, &gb, crnl);
    return growbuf_to_Str(&gb);
}

void ISgets_to_growbuf(struct input_stream* is, struct growbuf* gb, char crnl)
{
    struct stream_buffer* sb = &is->stream;
    int i;

    gb->len = 0;

    while (!is->iseos) {
        if (MUST_BE_UPDATED(is)) {
            do_update(is);
            continue;
        }
        if (crnl && gb->len > 0 && gb->ptr[gb->len - 1] == '\r') {
            if (sb->buf[sb->cur] == '\n') {
                GROWBUF_ADD_CHAR(gb, '\n');
                ++sb->cur;
            }
            break;
        }
        for (i = sb->cur; i < sb->next; ++i) {
            if (sb->buf[i] == '\n' || (crnl && sb->buf[i] == '\r')) {
                ++i;
                break;
            }
        }
        growbuf_append(gb, &sb->buf[sb->cur], i - sb->cur);
        sb->cur = i;
        if (gb->len > 0 && gb->ptr[gb->len - 1] == '\n')
            break;
    }

    growbuf_reserve(gb, gb->len + 1);
    gb->ptr[gb->len] = '\0';
    return;
}

int ISread_n(struct input_stream* is, unsigned char* dst, int count)
{
    if (is == NULL || count <= 0)
        return -1;

    if (is->iseos)
        return 0;

    int len = buffer_read(&is->stream, dst, count);
    if (MUST_BE_UPDATED(is)) {
        int l = (*is->read)(is->handle, &dst[len], count - len);
        if (l <= 0) {
            is->iseos = true;
        } else {
            len += l;
        }
    }
    return len;
}

int ISfileno(struct input_stream* is)
{
    if (is == NULL)
        return -1;
    switch (is->type) {
    case IST_BASIC:
        return *(int*)is->handle;
    case IST_FILE:
        return fileno(is->handle);
    case IST_SSL:
        return ((struct ssl_handle*)is->handle)->sock;
    case IST_ENCODED:
        return ISfileno(((struct ens_handle*)is->handle)->is);
    default:
        return -1;
    }
}

static pStr accept_this_site;

void ssl_accept_this_site(const char* hostname)
{
    if (hostname)
        accept_this_site = Strnew_charp(hostname);
    else
        accept_this_site = NULL;
}

static int
ssl_match_cert_ident(const char* ident, int ilen, const char* hostname)
{
    /* RFC2818 3.1.  Server Identity
     * Names may contain the wildcard
     * character * which is considered to match any single domain name
     * component or component fragment. E.g., *.a.com matches foo.a.com but
     * not bar.foo.a.com. f*.com matches foo.com but not bar.com.
     */
    int hlen = strlen(hostname);
    int i, c;

    /* Is this an exact match? */
    if ((ilen == hlen) && strncasecmp(ident, hostname, hlen) == 0)
        return true;

    for (i = 0; i < ilen; i++) {
        if (ident[i] == '*' && ident[i + 1] == '.') {
            while ((c = *hostname++) != '\0')
                if (c == '.')
                    break;
            i++;
        } else {
            if (ident[i] != *hostname++)
                return false;
        }
    }
    return *hostname == '\0';
}

static pStr
ssl_check_cert_ident(X509* x, const char* hostname)
{
    int i;
    pStr ret = NULL;
    int match_ident = false;
    /*
     * All we need to do here is check that the CN matches.
     *
     * From RFC2818 3.1 Server Identity:
     * If a subjectAltName extension of type dNSName is present, that MUST
     * be used as the identity. Otherwise, the (most specific) Common Name
     * field in the Subject field of the certificate MUST be used. Although
     * the use of the Common Name is existing practice, it is deprecated and
     * Certification Authorities are encouraged to use the dNSName instead.
     */
    i = X509_get_ext_by_NID(x, NID_subject_alt_name, -1);
    if (i >= 0) {
        X509_EXTENSION* ex;
        STACK_OF(GENERAL_NAME) * alt;

        ex = X509_get_ext(x, i);
        alt = X509V3_EXT_d2i(ex);
        if (alt) {
            int n;
            GENERAL_NAME* gn;
            pStr seen_dnsname = NULL;

            n = sk_GENERAL_NAME_num(alt);
            for (i = 0; i < n; i++) {
                gn = sk_GENERAL_NAME_value(alt, i);
                if (gn->type == GEN_DNS) {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
                    unsigned char* sn = ASN1_STRING_data(gn->d.ia5);
#else
                    const unsigned char* sn = ASN1_STRING_get0_data(gn->d.ia5);
#endif
                    int sl = ASN1_STRING_length(gn->d.ia5);

                    /*
                     * sn is a pointer to internal data and not guaranteed to
                     * be null terminated. Ensure we have a null terminated
                     * string that we can modify.
                     */
                    char* asn = GC_MALLOC(sl + 1);
                    if (!asn)
                        exit(1);
                    memmove(asn, sn, sl);
                    asn[sl] = '\0';

                    if (!seen_dnsname)
                        seen_dnsname = Strnew();
                    /* replace \0 to make full string visible to user */
                    if (sl != strlen(asn)) {
                        int i;
                        for (i = 0; i < sl; ++i) {
                            if (!asn[i])
                                asn[i] = '!';
                        }
                    }
                    Strcat_m_charp(seen_dnsname, asn, " ", NULL);
                    if (sl == strlen(asn) /* catch \0 in SAN */
                        && ssl_match_cert_ident(asn, sl, hostname))
                        break;
                }
            }
            X509V3_EXT_get(ex);
            sk_GENERAL_NAME_free(alt);
            if (i < n) /* Found a match */
                match_ident = true;
            else if (seen_dnsname)
                ret = Sprintf(_("Bad cert ident from %s: dNSName=%s"), hostname,
                    seen_dnsname->ptr);
        }
    }

    if (match_ident == false && ret == NULL) {
        X509_NAME* xn;
        char buf[2048];
        int slen;

        xn = X509_get_subject_name(x);

        slen = X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf));
        if (slen == -1)
            ret = Strnew_charp(_("Unable to get common name from peer cert"));
        else if (slen != strlen(buf)
            || !ssl_match_cert_ident(buf, strlen(buf), hostname)) {
            /* replace \0 to make full string visible to user */
            if (slen != strlen(buf)) {
                int i;
                for (i = 0; i < slen; ++i) {
                    if (!buf[i])
                        buf[i] = '!';
                }
            }
            ret = Sprintf(_("Bad cert ident %s from %s"), buf, hostname);
        }
    }
    return ret;
}

pStr ssl_get_certificate(SSL* ssl, const char* hostname)
{
    BIO* bp;
    X509* x;
    X509_NAME* xn;
    char* p;
    int len;
    pStr s;
    char buf[2048];
    pStr amsg = NULL;
    pStr emsg;
    int ans;

    if (ssl == NULL)
        return NULL;
    x = SSL_get_peer_certificate(ssl);
    if (x == NULL) {
        if (accept_this_site
            && strcasecmp(accept_this_site->ptr, hostname) == 0)
            ans = 1;
        else
            ans = confirm(_("No SSL peer certificate: accept?"));
        if (ans)
            amsg = Strnew_charp(_("Accept SSL session without any peer certificate"));
        else {
            /* FIXME: gettextize? */
            const char* e = "This SSL session was rejected "
                            "to prevent security violation: no peer certificate";
            disp_err_message(e, false);
            free_ssl_ctx();
            return NULL;
        }
        if (amsg)
            disp_err_message(amsg->ptr, false);
        ssl_accept_this_site(hostname);
        s = amsg ? amsg : Strnew_charp(_("valid certificate"));
        return s;
    }
    /* check the cert chain.
     * The chain length is automatically checked by OpenSSL when we
     * set the verify depth in the ctx.
     */
    if (ssl_verify_server) {
        long verr;
        if ((verr = SSL_get_verify_result(ssl))
            != X509_V_OK) {
            const char* em = X509_verify_cert_error_string(verr);
            if (accept_this_site
                && strcasecmp(accept_this_site->ptr, hostname) == 0)
                ans = 1;
            else {
                /* FIXME: gettextize? */
                ans = confirm(Sprintf("%s: accept?", em)->ptr);
            }
            if (ans) {
                /* FIXME: gettextize? */
                amsg = Sprintf("Accept unsecure SSL session: "
                               "unverified: %s",
                    em);
            } else {
                char* e = Sprintf(_("This SSL session was rejected: %s"), em)->ptr;
                disp_err_message(e, false);
                free_ssl_ctx();
                return NULL;
            }
        }
    }
    emsg = ssl_check_cert_ident(x, hostname);
    if (emsg != NULL) {
        if (accept_this_site
            && strcasecmp(accept_this_site->ptr, hostname) == 0)
            ans = 1;
        else {
            pStr ep = Strdup(emsg);
            if (ep->len > COLS - 16)
                Strshrink(ep, ep->len - (COLS - 16));
            Strcat_charp(ep, ": accept?");
            ans = confirm(ep->ptr);
        }
        if (ans) {
            amsg = Strnew_charp(_("Accept unsecure SSL session:"));
            Strcat(amsg, emsg);
        } else {
            /* FIXME: gettextize? */
            const char* e = "This SSL session was rejected "
                            "to prevent security violation";
            disp_err_message(e, false);
            free_ssl_ctx();
            return NULL;
        }
    }
    if (amsg)
        disp_err_message(amsg->ptr, false);
    ssl_accept_this_site(hostname);
    s = amsg ? amsg : Strnew_charp(_("valid certificate"));
    Strcat_charp(s, "\n");
    xn = X509_get_subject_name(x);
    if (X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf)) == -1)
        Strcat_charp(s, " subject=<unknown>");
    else
        Strcat_m_charp(s, " subject=", buf, NULL);
    xn = X509_get_issuer_name(x);
    if (X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf)) == -1)
        Strcat_charp(s, ": issuer=<unknown>");
    else
        Strcat_m_charp(s, ": issuer=", buf, NULL);
    Strcat_charp(s, "\n\n");

    bp = BIO_new(BIO_s_mem());
    X509_print(bp, x);
    len = (int)BIO_ctrl(bp, BIO_CTRL_INFO, 0, (char*)&p);
    Strcat_charp_n(s, p, len);
    BIO_free_all(bp);
    X509_free(x);
    return s;
}

bool canSaveFile(struct input_stream* stream, const char* path2)
{
    int des = ISfileno(stream);
    if (des < 0)
        // not file
        return true;

    if (*path2 == '|' && PermitSaveToPipe)
        return true;

    struct stat st1, st2;
    if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            // source and dst is same ?
            return false;

    // ok
    return true;
}
