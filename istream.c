#include "Url.h"
#include "signal_jmp.h"
#include "tui.h"
#include "local_cgi.h"
#include "etc.h"
#include "file.h"
#include "fm.h"
#include "form.h"
#include "config.h"
#include "terms.h"
#include "file.h"
#include "display.h"
#include "html.h"
#include "istream.h"
#include "indep.h"
#include "mimehead.h"
#include "dns_order.h"
#include <gcstr/myctype.h>
#include <signal.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <setjmp.h>
#include <openssl/x509v3.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/crypto.h> /* SSLEAY_VERSION_NUMBER may be here */
#include <openssl/err.h>

char PermitSaveToPipe = (FALSE);

#define uchar unsigned char

#define STREAM_BUF_SIZE 8192
#define SSL_BUF_SIZE 1536

#define MUST_BE_UPDATED(bs) ((bs)->stream.cur == (bs)->stream.next)

#define POP_CHAR(bs) ((bs)->iseos ? '\0' : (bs)->stream.buf[(bs)->stream.cur++])

static JMP_BUF AbortLoading;

static void
KeyAbort(int _)
{
    LONGJMP(AbortLoading, 1);
}

static void basic_close(int* handle);
static int basic_read(int* handle, char* buf, int len);

static void file_close(struct io_file_handle* handle);
static int file_read(struct io_file_handle* handle, char* buf, int len);

static int str_read(Str handle, char* buf, int len);

static void ssl_close(struct ssl_handle* handle);
static int ssl_read(struct ssl_handle* handle, char* buf, int len);

static int ens_read(struct ens_handle* handle, char* buf, int len);
static void ens_close(struct ens_handle* handle);

static void memchop(char* p, int* len);

static void
do_update(BaseStream base)
{
    int len;
    base->stream.cur = base->stream.next = 0;
    len = (*base->read)(base->handle, base->stream.buf, base->stream.size);
    if (len <= 0)
        base->iseos = TRUE;
    else
        base->stream.next += len;
}

static int
buffer_read(StreamBuffer sb, char* obuf, int count)
{
    int len = sb->next - sb->cur;
    if (len > 0) {
        if (len > count)
            len = count;
        bcopy((const void*)&sb->buf[sb->cur], obuf, len);
        sb->cur += len;
    }
    return len;
}

static void
init_buffer(BaseStream base, char* buf, int bufsize)
{
    StreamBuffer sb = &base->stream;
    sb->size = bufsize;
    sb->cur = 0;
    sb->buf = NewWithoutGC_N(uchar, bufsize);
    if (buf) {
        memcpy(sb->buf, buf, bufsize);
        sb->next = bufsize;
    } else {
        sb->next = 0;
    }
    base->iseos = FALSE;
}

static void
init_base_stream(BaseStream base, int bufsize)
{
    init_buffer(base, NULL, bufsize);
}

static void
init_str_stream(BaseStream base, Str s)
{
    init_buffer(base, s->ptr, s->length);
}

InputStream
newInputStream(int des)
{
    InputStream stream;
    if (des < 0)
        return NULL;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->base.type = IST_BASIC;
    stream->base.handle = NewWithoutGC(int);
    *(int*)stream->base.handle = des;
    stream->base.read = (int (*)(void*, void*, int))basic_read;
    stream->base.close = (void (*)(void*))basic_close;
    return stream;
}

InputStream
newFileStream(FILE* f, void (*closep)())
{
    InputStream stream;
    if (f == NULL)
        return NULL;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->file.type = IST_FILE;
    stream->file.handle = NewWithoutGC(struct io_file_handle);
    stream->file.handle->f = f;
    if (closep)
        stream->file.handle->close = (void (*)(void*))closep;
    else
        stream->file.handle->close = (void (*)(void*))fclose;
    stream->file.read = (int (*)())file_read;
    stream->file.close = (void (*)())file_close;
    return stream;
}

InputStream
newStrStream(Str s)
{
    InputStream stream;
    if (s == NULL)
        return NULL;
    stream = NewWithoutGC(union input_stream);
    init_str_stream(&stream->base, s);
    stream->str.type = IST_STR;
    stream->str.handle = NULL;
    stream->str.read = (int (*)())str_read;
    stream->str.close = NULL;
    return stream;
}

InputStream
newSSLStream(SSL* ssl, int sock)
{
    InputStream stream;
    if (sock < 0)
        return NULL;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, SSL_BUF_SIZE);
    stream->ssl.type = IST_SSL;
    stream->ssl.handle = NewWithoutGC(struct ssl_handle);
    stream->ssl.handle->ssl = ssl;
    stream->ssl.handle->sock = sock;
    stream->ssl.read = (int (*)())ssl_read;
    stream->ssl.close = (void (*)())ssl_close;
    return stream;
}

InputStream
newEncodedStream(InputStream is, char encoding)
{
    InputStream stream;
    if (is == NULL || (encoding != ENC_QUOTE && encoding != ENC_BASE64 && encoding != ENC_UUENCODE))
        return is;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->ens.type = IST_ENCODED;
    stream->ens.handle = NewWithoutGC(struct ens_handle);
    stream->ens.handle->is = is;
    stream->ens.handle->pos = 0;
    stream->ens.handle->encoding = encoding;
    growbuf_init_without_GC(&stream->ens.handle->gb);
    stream->ens.read = (int (*)())ens_read;
    stream->ens.close = (void (*)())ens_close;
    return stream;
}

int ISclose(InputStream stream)
{
    MySignalHandler prevtrap;
    if (stream == NULL)
        return -1;
    if (stream->base.close != NULL) {
        if (stream->base.type & IST_UNCLOSE) {
            return -1;
        }
        prevtrap = mySignal(SIGINT, SIG_IGN);
        stream->base.close(stream->base.handle);
        mySignal(SIGINT, prevtrap);
    }
    xfree(stream->base.stream.buf);
    xfree(stream);
    return 0;
}

int ISgetc(InputStream stream)
{
    BaseStream base;
    if (stream == NULL)
        return '\0';
    base = &stream->base;
    if (!base->iseos && MUST_BE_UPDATED(base))
        do_update(base);
    return POP_CHAR(base);
}

int ISundogetc(InputStream stream)
{
    StreamBuffer sb;
    if (stream == NULL)
        return -1;
    sb = &stream->base.stream;
    if (sb->cur > 0) {
        sb->cur--;
        return 0;
    }
    return -1;
}

Str StrISgets2(InputStream stream, char crnl)
{
    struct growbuf gb;

    if (stream == NULL)
        return NULL;
    growbuf_init(&gb);
    ISgets_to_growbuf(stream, &gb, crnl);
    return growbuf_to_Str(&gb);
}

void ISgets_to_growbuf(InputStream stream, struct growbuf* gb, char crnl)
{
    BaseStream base = &stream->base;
    StreamBuffer sb = &base->stream;
    int i;

    gb->length = 0;

    while (!base->iseos) {
        if (MUST_BE_UPDATED(base)) {
            do_update(base);
            continue;
        }
        if (crnl && gb->length > 0 && gb->ptr[gb->length - 1] == '\r') {
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
        if (gb->length > 0 && gb->ptr[gb->length - 1] == '\n')
            break;
    }

    growbuf_reserve(gb, gb->length + 1);
    gb->ptr[gb->length] = '\0';
    return;
}

#ifdef unused
int ISread(InputStream stream, Str buf, int count)
{
    int len;

    if (count + 1 > buf->area_size) {
        char* newptr = GC_MALLOC_ATOMIC(count + 1);
        memcpy(newptr, buf->ptr, buf->length);
        newptr[buf->length] = '\0';
        buf->ptr = newptr;
        buf->area_size = count + 1;
    }
    len = ISread_n(stream, buf->ptr, count);
    buf->length = (len > 0) ? len : 0;
    buf->ptr[buf->length] = '\0';
    return (len > 0) ? 1 : 0;
}
#endif

int ISread_n(InputStream stream, char* dst, int count)
{
    int len, l;
    BaseStream base;

    if (stream == NULL || count <= 0)
        return -1;
    if ((base = &stream->base)->iseos)
        return 0;

    len = buffer_read(&base->stream, dst, count);
    if (MUST_BE_UPDATED(base)) {
        l = (*base->read)(base->handle, &dst[len], count - len);
        if (l <= 0) {
            base->iseos = TRUE;
        } else {
            len += l;
        }
    }
    return len;
}

int ISfileno(InputStream stream)
{
    if (stream == NULL)
        return -1;
    switch (IStype(stream) & ~IST_UNCLOSE) {
    case IST_BASIC:
        return *(int*)stream->base.handle;
    case IST_FILE:
        return fileno(stream->file.handle->f);
    case IST_SSL:
        return stream->ssl.handle->sock;
    case IST_ENCODED:
        return ISfileno(stream->ens.handle->is);
    default:
        return -1;
    }
}

int ISeos(InputStream stream)
{
    BaseStream base = &stream->base;
    if (!base->iseos && MUST_BE_UPDATED(base))
        do_update(base);
    return base->iseos;
}

static Str accept_this_site;

void ssl_accept_this_site(char* hostname)
{
    if (hostname)
        accept_this_site = Strnew_charp(hostname);
    else
        accept_this_site = NULL;
}

static int
ssl_match_cert_ident(char* ident, int ilen, char* hostname)
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
        return TRUE;

    for (i = 0; i < ilen; i++) {
        if (ident[i] == '*' && ident[i + 1] == '.') {
            while ((c = *hostname++) != '\0')
                if (c == '.')
                    break;
            i++;
        } else {
            if (ident[i] != *hostname++)
                return FALSE;
        }
    }
    return *hostname == '\0';
}

static Str
ssl_check_cert_ident(X509* x, char* hostname)
{
    int i;
    Str ret = NULL;
    int match_ident = FALSE;
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
            Str seen_dnsname = NULL;

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
                    char* asn = w3m_GC_alloc(sl + 1);
                    if (!asn)
                        exit(1);
                    bcopy(sn, asn, sl);
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
                match_ident = TRUE;
            else if (seen_dnsname)
                /* FIXME: gettextize? */
                ret = Sprintf("Bad cert ident from %s: dNSName=%s", hostname,
                    seen_dnsname->ptr);
        }
    }

    if (match_ident == FALSE && ret == NULL) {
        X509_NAME* xn;
        char buf[2048];
        int slen;

        xn = X509_get_subject_name(x);

        slen = X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf));
        if (slen == -1)
            /* FIXME: gettextize? */
            ret = Strnew_charp("Unable to get common name from peer cert");
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
            /* FIXME: gettextize? */
            ret = Sprintf("Bad cert ident %s from %s", buf, hostname);
        }
    }
    return ret;
}

Str ssl_get_certificate(SSL* ssl, char* hostname)
{
    BIO* bp;
    X509* x;
    X509_NAME* xn;
    char* p;
    int len;
    Str s;
    char buf[2048];
    Str amsg = NULL;
    Str emsg;
    char* ans;

    if (ssl == NULL)
        return NULL;
    x = SSL_get_peer_certificate(ssl);
    if (x == NULL) {
        if (accept_this_site
            && strcasecmp(accept_this_site->ptr, hostname) == 0)
            ans = "y";
        else {
            /* FIXME: gettextize? */
            emsg = Strnew_charp("No SSL peer certificate: accept? (y/n)");
            ans = inputAnswer(emsg->ptr);
        }
        if (ans && TOLOWER(*ans) == 'y')
            /* FIXME: gettextize? */
            amsg = Strnew_charp("Accept SSL session without any peer certificate");
        else {
            /* FIXME: gettextize? */
            char* e = "This SSL session was rejected "
                      "to prevent security violation: no peer certificate";
            tui_disp_err_message(e, FALSE);
            free_ssl_ctx();
            return NULL;
        }
        if (amsg)
            tui_disp_err_message(amsg->ptr, FALSE);
        ssl_accept_this_site(hostname);
        /* FIXME: gettextize? */
        s = amsg ? amsg : Strnew_charp("valid certificate");
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
                ans = "y";
            else {
                /* FIXME: gettextize? */
                emsg = Sprintf("%s: accept? (y/n)", em);
                ans = inputAnswer(emsg->ptr);
            }
            if (ans && TOLOWER(*ans) == 'y') {
                /* FIXME: gettextize? */
                amsg = Sprintf("Accept unsecure SSL session: "
                               "unverified: %s",
                    em);
            } else {
                /* FIXME: gettextize? */
                char* e = Sprintf("This SSL session was rejected: %s", em)->ptr;
                tui_disp_err_message(e, FALSE);
                free_ssl_ctx();
                return NULL;
            }
        }
    }
    emsg = ssl_check_cert_ident(x, hostname);
    if (emsg != NULL) {
        if (accept_this_site
            && strcasecmp(accept_this_site->ptr, hostname) == 0)
            ans = "y";
        else {
            Str ep = Strdup(emsg);
            if (ep->length > COLS - 16)
                Strshrink(ep, ep->length - (COLS - 16));
            Strcat_charp(ep, ": accept? (y/n)");
            ans = inputAnswer(ep->ptr);
        }
        if (ans && TOLOWER(*ans) == 'y') {
            /* FIXME: gettextize? */
            amsg = Strnew_charp("Accept unsecure SSL session:");
            Strcat(amsg, emsg);
        } else {
            /* FIXME: gettextize? */
            char* e = "This SSL session was rejected "
                      "to prevent security violation";
            tui_disp_err_message(e, FALSE);
            free_ssl_ctx();
            return NULL;
        }
    }
    if (amsg)
        tui_disp_err_message(amsg->ptr, FALSE);
    ssl_accept_this_site(hostname);
    /* FIXME: gettextize? */
    s = amsg ? amsg : Strnew_charp("valid certificate");
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

/* Raw level input stream functions */

static void
basic_close(int* handle)
{
    close(*(int*)handle);
    xfree(handle);
}

static int
basic_read(int* handle, char* buf, int len)
{
    return read(*(int*)handle, buf, len);
}

static void
file_close(struct io_file_handle* handle)
{
    handle->close(handle->f);
    xfree(handle);
}

static int
file_read(struct io_file_handle* handle, char* buf, int len)
{
    return fread(buf, 1, len, handle->f);
}

static int
str_read(Str handle, char* buf, int len)
{
    return 0;
}

static void
ssl_close(struct ssl_handle* handle)
{
    close(handle->sock);
    if (handle->ssl)
        SSL_free(handle->ssl);
    xfree(handle);
}

static int
ssl_read(struct ssl_handle* handle, char* buf, int len)
{
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
    } else
        status = read(handle->sock, buf, len);
    return status;
}

static void
ens_close(struct ens_handle* handle)
{
    ISclose(handle->is);
    growbuf_clear(&handle->gb);
    xfree(handle);
}

static int
ens_read(struct ens_handle* handle, char* buf, int len)
{
    if (handle->pos == handle->gb.length) {
        char* p;
        struct growbuf gbtmp;

        ISgets_to_growbuf(handle->is, &handle->gb, TRUE);
        if (handle->gb.length == 0)
            return 0;
        if (handle->encoding == ENC_BASE64)
            memchop(handle->gb.ptr, &handle->gb.length);
        else if (handle->encoding == ENC_UUENCODE) {
            if (handle->gb.length >= 5 && !strncmp(handle->gb.ptr, "begin", 5))
                ISgets_to_growbuf(handle->is, &handle->gb, TRUE);
            memchop(handle->gb.ptr, &handle->gb.length);
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

    if (len > handle->gb.length - handle->pos)
        len = handle->gb.length - handle->pos;

    memcpy(buf, &handle->gb.ptr[handle->pos], len);
    handle->pos += len;
    return len;
}

static void
memchop(char* p, int* len)
{
    char* q;

    for (q = p + *len; q > p; --q) {
        if (q[-1] != '\n' && q[-1] != '\r')
            break;
    }
    if (q != p + *len)
        *q = '\0';
    *len = q - p;
    return;
}

int checkSaveFile(InputStream stream, char* path2)
{
    struct stat st1, st2;
    int des = ISfileno(stream);

    if (des < 0)
        return 0;
    if (*path2 == '|' && PermitSaveToPipe)
        return 0;
    if ((fstat(des, &st1) == 0) && (stat(path2, &st2) == 0))
        if (st1.st_ino == st2.st_ino)
            return -1;
    return 0;
}

int openSocket(char* const hostname,
    char* remoteport_name, unsigned short remoteport_num)
{
    volatile int sock = -1;
    int* af;
    struct addrinfo hints, *res0, *res;
    int error;
    char* hname;
    MySignalHandler prevtrap = NULL;

    tui_message(Sprintf("Opening socket...")->ptr);

    if (SETJMP(AbortLoading) != 0) {
        if (sock >= 0)
            close(sock);
        goto error;
    }
    TRAP_ON;
    if (hostname == NULL) {
        goto error;
    }

    /* rfc2732 compliance */
    hname = hostname;
    if (hname != NULL && hname[0] == '[' && hname[strlen(hname) - 1] == ']') {
        hname = allocStr(hostname + 1, -1);
        hname[strlen(hname) - 1] = '\0';
        if (strspn(hname, "0123456789abcdefABCDEF:.") != strlen(hname))
            goto error;
    }
    for (af = ai_family_order_table[DNS_order];; af++) {
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = *af;
        hints.ai_socktype = SOCK_STREAM;
        if (remoteport_num != 0) {
            Str portbuf = Sprintf("%d", remoteport_num);
            error = getaddrinfo(hname, portbuf->ptr, &hints, &res0);
        } else {
            error = -1;
        }
        if (error && remoteport_name && remoteport_name[0] != '\0') {
            /* try default port */
            error = getaddrinfo(hname, remoteport_name, &hints, &res0);
        }
        if (error) {
            if (*af == PF_UNSPEC) {
                goto error;
            }
            /* try next ai family */
            continue;
        }
        sock = -1;
        for (res = res0; res; res = res->ai_next) {
            sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (sock < 0) {
                continue;
            }
            if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
                close(sock);
                sock = -1;
                continue;
            }
            break;
        }
        if (sock < 0) {
            freeaddrinfo(res0);
            if (*af == PF_UNSPEC) {
                goto error;
            }
            /* try next ai family */
            continue;
        }
        freeaddrinfo(res0);
        break;
    }

    TRAP_OFF;
    return sock;
error:
    TRAP_OFF;
    return -1;
}

SSL_CTX* ssl_ctx = NULL;

void free_ssl_ctx(void)
{
    if (ssl_ctx != NULL)
        SSL_CTX_free(ssl_ctx);
    ssl_ctx = NULL;
    ssl_accept_this_site(NULL);
}

static int
domain_match(char* pat, char* domain)
{
    if (domain == NULL)
        return 0;
    if (*pat == '.')
        pat++;
    for (;;) {
        if (!strcasecmp(pat, domain))
            return 1;
        domain = strchr(domain, '.');
        if (domain == NULL)
            return 0;
        domain++;
    }
}

int check_no_proxy(char* domain)
{
    TextListItem* tl;
    volatile int ret = 0;
    MySignalHandler prevtrap = NULL;

    if (NO_proxy_domains == NULL || NO_proxy_domains->nitem == 0 || domain == NULL)
        return 0;
    for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next) {
        if (domain_match(tl->ptr, domain))
            return 1;
    }
    if (!NOproxy_netaddr) {
        return 0;
    }
    /*
     * to check noproxy by network addr
     */
    if (SETJMP(AbortLoading) != 0) {
        ret = 0;
        goto end;
    }
    TRAP_ON;
    {
        int error;
        struct addrinfo hints;
        struct addrinfo *res, *res0;
        char addr[4 * 16];
        int* af;

        for (af = ai_family_order_table[DNS_order];; af++) {
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = *af;
            error = getaddrinfo(domain, NULL, &hints, &res0);
            if (error) {
                if (*af == PF_UNSPEC) {
                    break;
                }
                /* try next */
                continue;
            }
            for (res = res0; res != NULL; res = res->ai_next) {
                switch (res->ai_family) {
                case AF_INET:
                    inet_ntop(AF_INET,
                        &((struct sockaddr_in*)res->ai_addr)->sin_addr,
                        addr, sizeof(addr));
                    break;
                case AF_INET6:
                    inet_ntop(AF_INET6,
                        &((struct sockaddr_in6*)res->ai_addr)->sin6_addr, addr, sizeof(addr));
                    break;
                default:
                    /* unknown */
                    continue;
                }
                for (tl = NO_proxy_domains->first; tl != NULL; tl = tl->next) {
                    if (strncmp(tl->ptr, addr, strlen(tl->ptr)) == 0) {
                        freeaddrinfo(res0);
                        ret = 1;
                        goto end;
                    }
                }
            }
            freeaddrinfo(res0);
            if (*af == PF_UNSPEC) {
                break;
            }
        }
    }
end:
    TRAP_OFF;
    return ret;
}

/* add index_file if exists */
static void
add_index_file(struct Url* pu, struct URLFile* uf)
{
    char *p, *q;
    TextList* index_file_list = NULL;
    TextListItem* ti;

    if (non_null(index_file))
        index_file_list = make_domain_list(index_file);
    if (index_file_list == NULL) {
        uf->stream = NULL;
        return;
    }
    for (ti = index_file_list->first; ti; ti = ti->next) {
        p = Strnew_m_charp(pu->file, "/", file_quote(ti->ptr), NULL)->ptr;
        p = cleanupName(p);
        q = cleanupName(file_unquote(p));
        examineFile(q, uf);
        if (uf->stream != NULL) {
            pu->file = p;
            pu->real_file = q;
            return;
        }
    }
}

static int
str_to_ssl_version(const char* name)
{
    if (!strcasecmp(name, "all"))
        return 0;
    if (!strcasecmp(name, "none"))
        return 0;
#ifdef TLS1_3_VERSION
    if (!strcasecmp(name, "TLSv1.3"))
        return TLS1_3_VERSION;
#endif
#ifdef TLS1_2_VERSION
    if (!strcasecmp(name, "TLSv1.2"))
        return TLS1_2_VERSION;
#endif
#ifdef TLS1_1_VERSION
    if (!strcasecmp(name, "TLSv1.1"))
        return TLS1_1_VERSION;
#endif
    if (!strcasecmp(name, "TLSv1.0"))
        return TLS1_VERSION;
    if (!strcasecmp(name, "TLSv1"))
        return TLS1_VERSION;
    if (!strcasecmp(name, "SSLv3.0"))
        return SSL3_VERSION;
    if (!strcasecmp(name, "SSLv3"))
        return SSL3_VERSION;
    return -1;
}

static void
init_PRNG(void)
{
    char buffer[256];
    const char* file;
    long l;
    if (RAND_status())
        return;
    if ((file = RAND_file_name(buffer, sizeof(buffer)))) {
#ifdef USE_EGD
        if (RAND_egd(file) > 0)
            return;
#endif
        RAND_load_file(file, -1);
    }
    if (RAND_status())
        goto seeded;
    srand48((long)time(NULL));
    while (!RAND_status()) {
        l = lrand48();
        RAND_seed((unsigned char*)&l, sizeof(long));
    }
seeded:
    if (file)
        RAND_write_file(file);
}

static SSL*
openSSLHandle(int sock, char* hostname, char** p_cert)
{
    SSL* handle = NULL;
    static char* old_ssl_forbid_method = NULL;
    static int old_ssl_verify_server = -1;

    if (old_ssl_forbid_method != ssl_forbid_method
        && (!old_ssl_forbid_method || !ssl_forbid_method || strcmp(old_ssl_forbid_method, ssl_forbid_method))) {
        old_ssl_forbid_method = ssl_forbid_method;
        ssl_path_modified = 1;
    }
    if (old_ssl_verify_server != ssl_verify_server) {
        old_ssl_verify_server = ssl_verify_server;
        ssl_path_modified = 1;
    }
    if (ssl_path_modified) {
        free_ssl_ctx();
        ssl_path_modified = 0;
    }
    if (ssl_ctx == NULL) {
        int option;
#if OPENSSL_VERSION_NUMBER < 0x0800
        ssl_ctx = SSL_CTX_new();
        X509_set_default_verify_paths(ssl_ctx->cert);
#else /* SSLEAY_VERSION_NUMBER >= 0x0800 */
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
        SSLeay_add_ssl_algorithms();
        SSL_load_error_strings();
#else
        OPENSSL_init_ssl(0, NULL);
#endif
        if (!(ssl_ctx = SSL_CTX_new(SSLv23_client_method())))
            goto eend;
#ifdef SSL_CTX_set_min_proto_version
        if (ssl_min_version && *ssl_min_version != '\0') {
            int sslver;
            sslver = str_to_ssl_version(ssl_min_version);
            if (sslver < 0
                || !SSL_CTX_set_min_proto_version(ssl_ctx, sslver)) {
                free_ssl_ctx();
                goto eend;
            }
        }
#endif
        if (ssl_cipher && *ssl_cipher != '\0')
            if (!SSL_CTX_set_cipher_list(ssl_ctx, ssl_cipher)) {
                free_ssl_ctx();
                goto eend;
            }
        option = SSL_OP_ALL;
        if (ssl_forbid_method) {
            if (strchr(ssl_forbid_method, '2'))
                option |= SSL_OP_NO_SSLv2;
            if (strchr(ssl_forbid_method, '3'))
                option |= SSL_OP_NO_SSLv3;
            if (strchr(ssl_forbid_method, 't'))
                option |= SSL_OP_NO_TLSv1;
            if (strchr(ssl_forbid_method, 'T'))
                option |= SSL_OP_NO_TLSv1;
            if (strchr(ssl_forbid_method, '4'))
                option |= SSL_OP_NO_TLSv1;
#ifdef SSL_OP_NO_TLSv1_1
            if (strchr(ssl_forbid_method, '5'))
                option |= SSL_OP_NO_TLSv1_1;
#endif
#ifdef SSL_OP_NO_TLSv1_2
            if (strchr(ssl_forbid_method, '6'))
                option |= SSL_OP_NO_TLSv1_2;
#endif
#ifdef SSL_OP_NO_TLSv1_3
            if (strchr(ssl_forbid_method, '7'))
                option |= SSL_OP_NO_TLSv1_3;
#endif
        }
#ifdef SSL_OP_NO_COMPRESSION
        option |= SSL_OP_NO_COMPRESSION;
#endif
        SSL_CTX_set_options(ssl_ctx, option);

#ifdef SSL_MODE_RELEASE_BUFFERS
        SSL_CTX_set_mode(ssl_ctx, SSL_MODE_RELEASE_BUFFERS);
#endif

        /* derived from openssl-0.9.5/apps/s_{client,cb}.c */
#if 1 /* use SSL_get_verify_result() to verify cert */
        SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
#else
        SSL_CTX_set_verify(ssl_ctx,
            ssl_verify_server ? SSL_VERIFY_PEER : SSL_VERIFY_NONE, NULL);
#endif
        if (ssl_cert_file != NULL && *ssl_cert_file != '\0') {
            int ng = 1;
            if (SSL_CTX_use_certificate_file(ssl_ctx, ssl_cert_file, SSL_FILETYPE_PEM) > 0) {
                char* key_file = (ssl_key_file == NULL
                                     || *ssl_key_file == '\0')
                    ? ssl_cert_file
                    : ssl_key_file;
                if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) > 0)
                    if (SSL_CTX_check_private_key(ssl_ctx))
                        ng = 0;
            }
            if (ng) {
                free_ssl_ctx();
                goto eend;
            }
        }
        if (ssl_verify_server) {
            char *file = NULL, *path = NULL;
            if (ssl_ca_file && *ssl_ca_file != '\0')
                file = ssl_ca_file;
            if (ssl_ca_path && *ssl_ca_path != '\0')
                path = ssl_ca_path;
            if ((file || path)
                && !SSL_CTX_load_verify_locations(ssl_ctx, file, path)) {
                free_ssl_ctx();
                goto eend;
            }
            if (ssl_ca_default)
                SSL_CTX_set_default_verify_paths(ssl_ctx);
        }
#endif /* SSLEAY_VERSION_NUMBER >= 0x0800 */
    }
    handle = SSL_new(ssl_ctx);
    SSL_set_fd(handle, sock);
#if SSLEAY_VERSION_NUMBER >= 0x00905100
    init_PRNG();
#endif /* SSLEAY_VERSION_NUMBER >= 0x00905100 */
#if (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT)
    SSL_set_tlsext_host_name(handle, hostname);
#endif /* (SSLEAY_VERSION_NUMBER >= 0x00908070) && !defined(OPENSSL_NO_TLSEXT) */
    if (SSL_connect(handle) > 0) {
        Str serv_cert = ssl_get_certificate(handle, hostname);
        if (serv_cert) {
            *p_cert = serv_cert->ptr;
            return handle;
        }
        close(sock);
        SSL_free(handle);
        return NULL;
    }
eend:
    close(sock);
    if (handle)
        SSL_free(handle);
    /* FIXME: gettextize? */
    tui_disp_err_message(Sprintf("SSL error: %s, a workaround might be: w3m -insecure",
                             ERR_error_string(ERR_get_error(), NULL))
                             ->ptr,
        FALSE);
    return NULL;
}

static void
SSL_write_from_file(SSL* ssl, char* file)
{
    FILE* fd;
    int c;
    char buf[1];
    fd = fopen(file, "r");
    if (fd != NULL) {
        while ((c = fgetc(fd)) != EOF) {
            buf[0] = c;
            SSL_write(ssl, buf, 1);
        }
        fclose(fd);
    }
}

static void
write_from_file(int sock, char* file)
{
    FILE* fd;
    int c;
    char buf[1];
    fd = fopen(file, "r");
    if (fd != NULL) {
        while ((c = fgetc(fd)) != EOF) {
            buf[0] = c;
            write(sock, buf, 1);
        }
        fclose(fd);
    }
}

#define IS_DIRECTORY(m) (((m) & S_IFMT) == S_IFDIR)
bool dir_exist(const char* path)
{
    if (path == NULL || *path == '\0')
        return 0;
    struct stat stbuf;
    if (stat(path, &stbuf) == -1)
        return 0;
    return IS_DIRECTORY(stbuf.st_mode);
}

struct URLFile
openURL(char* url, struct Url* pu, struct Url* current,
    struct UrlOption* option, FormList* request, TextList* extra_header,
    struct URLFile* ouf, struct HttpRequest* hr, unsigned char* status)
{
    Str tmp;
    int sock, scheme;
    char *p, *q, *u;
    Str gophertmp;
    char type;
    int n;
    struct URLFile uf;
    struct HttpRequest hr0;
    SSL* sslh = NULL;

    if (hr == NULL)
        hr = &hr0;

    if (ouf) {
        uf = *ouf;
    } else {
        init_stream(&uf, SCM_MISSING, NULL);
    }

    u = url;
    scheme = getURLScheme(&u);
    if (current == NULL && scheme == SCM_MISSING && !ArgvIsURL)
        u = file_to_url(url); /* force to local file */
    else
        u = url;
retry:
    parseURL2(u, pu, current);
    if (pu->scheme == SCM_LOCAL && pu->file == NULL) {
        if (pu->label != NULL) {
            /* #hogege is not a label but a filename */
            Str tmp2 = Strnew_charp("#");
            Strcat_charp(tmp2, pu->label);
            pu->file = tmp2->ptr;
            pu->real_file = cleanupName(file_unquote(pu->file));
            pu->label = NULL;
        } else {
            /* given URL must be null string */
#ifdef SOCK_DEBUG
            sock_log("given URL must be null string\n");
#endif
            return uf;
        }
    }

    if (LocalhostOnly && pu->host && !is_localhost(pu->host))
        pu->host = NULL;

    uf.scheme = pu->scheme;
    uf.url = parsedURL2Str(pu)->ptr;
    pu->is_nocache = (option->flag & RG_NOCACHE);
    uf.ext = filename_extension(pu->file, 1);

    hr->command = HR_COMMAND_GET;
    hr->flag = 0;
    hr->referer = option->referer;
    hr->request = request;

    switch (pu->scheme) {
    case SCM_LOCAL:
    case SCM_LOCAL_CGI:
        if (request && request->body)
            /* local CGI: POST */
            uf.stream = newFileStream(localcgi_post(pu->real_file, pu->query,
                                          request, option->referer),
                (void (*)())fclose);
        else
            /* lodal CGI: GET */
            uf.stream = newFileStream(localcgi_get(pu->real_file, pu->query,
                                          option->referer),
                (void (*)())fclose);
        if (uf.stream) {
            uf.is_cgi = TRUE;
            uf.scheme = pu->scheme = SCM_LOCAL_CGI;
            return uf;
        }
        examineFile(pu->real_file, &uf);
        if (uf.stream == NULL) {
            if (dir_exist(pu->real_file)) {
                add_index_file(pu, &uf);
                if (uf.stream == NULL)
                    return uf;
            } else if (document_root != NULL) {
                tmp = Strnew_charp(document_root);
                if (Strlastchar(tmp) != '/' && pu->file[0] != '/')
                    Strcat_char(tmp, '/');
                Strcat_charp(tmp, pu->file);
                p = cleanupName(tmp->ptr);
                q = cleanupName(file_unquote(p));
                if (dir_exist(q)) {
                    pu->file = p;
                    pu->real_file = q;
                    add_index_file(pu, &uf);
                    if (uf.stream == NULL) {
                        return uf;
                    }
                } else {
                    examineFile(q, &uf);
                    if (uf.stream) {
                        pu->file = p;
                        pu->real_file = q;
                    }
                }
            }
        }
        if (uf.stream == NULL && retryAsHttp && url[0] != '/') {
            if (scheme == SCM_MISSING || scheme == SCM_UNKNOWN) {
                /* retry it as "http://" */
                u = Strnew_m_charp("http://", url, NULL)->ptr;
                goto retry;
            }
        }
        return uf;
    case SCM_FTP:
    case SCM_FTPDIR:
        if (pu->file == NULL)
            pu->file = allocStr("/", -1);
        if (non_null(FTP_proxy) && !Do_not_use_proxy && pu->host != NULL && !check_no_proxy(pu->host)) {
            hr->flag |= HR_FLAG_PROXY;
            sock = openSocket(FTP_proxy_parsed.host,
                schemeNumToName(FTP_proxy_parsed.scheme),
                FTP_proxy_parsed.port);
            if (sock < 0)
                return uf;
            uf.scheme = SCM_HTTP;
            tmp = HTTPrequest(pu, current, hr, extra_header);
            write(sock, tmp->ptr, tmp->length);
        } else {
            uf.stream = openFTPStream(pu, &uf);
            uf.scheme = pu->scheme;
            return uf;
        }
        break;
    case SCM_HTTP:
    case SCM_HTTPS:
        if (pu->file == NULL)
            pu->file = allocStr("/", -1);
        if (request && request->method == FORM_METHOD_POST && request->body)
            hr->command = HR_COMMAND_POST;
        if (request && request->method == FORM_METHOD_HEAD)
            hr->command = HR_COMMAND_HEAD;
        if ((
                (pu->scheme == SCM_HTTPS) ? non_null(HTTPS_proxy) : non_null(HTTP_proxy))
            && !Do_not_use_proxy && pu->host != NULL && !check_no_proxy(pu->host)) {
            hr->flag |= HR_FLAG_PROXY;
            if (pu->scheme == SCM_HTTPS && *status == HTST_CONNECT) {
                sock = ssl_socket_of(ouf->stream);
                if (!(sslh = openSSLHandle(sock, pu->host,
                          &uf.ssl_certificate))) {
                    *status = HTST_MISSING;
                    return uf;
                }
            } else if (pu->scheme == SCM_HTTPS) {
                sock = openSocket(HTTPS_proxy_parsed.host,
                    schemeNumToName(HTTPS_proxy_parsed.scheme),
                    HTTPS_proxy_parsed.port);
                sslh = NULL;
            } else {
                sock = openSocket(HTTP_proxy_parsed.host,
                    schemeNumToName(HTTP_proxy_parsed.scheme),
                    HTTP_proxy_parsed.port);
                sslh = NULL;
            }
            if (sock < 0) {
#ifdef SOCK_DEBUG
                sock_log("Can't open socket\n");
#endif
                return uf;
            }
            if (pu->scheme == SCM_HTTPS) {
                if (*status == HTST_NORMAL) {
                    hr->command = HR_COMMAND_CONNECT;
                    tmp = HTTPrequest(pu, current, hr, extra_header);
                    *status = HTST_CONNECT;
                } else {
                    hr->flag |= HR_FLAG_LOCAL;
                    tmp = HTTPrequest(pu, current, hr, extra_header);
                    *status = HTST_NORMAL;
                }
            } else {
                tmp = HTTPrequest(pu, current, hr, extra_header);
                *status = HTST_NORMAL;
            }
        } else {
            sock = openSocket(pu->host, schemeNumToName(pu->scheme), pu->port);
            if (sock < 0) {
                *status = HTST_MISSING;
                return uf;
            }
            if (pu->scheme == SCM_HTTPS) {
                if (!(sslh = openSSLHandle(sock, pu->host,
                          &uf.ssl_certificate))) {
                    *status = HTST_MISSING;
                    return uf;
                }
            }
            hr->flag |= HR_FLAG_LOCAL;
            tmp = HTTPrequest(pu, current, hr, extra_header);
            *status = HTST_NORMAL;
        }
        if (pu->scheme == SCM_HTTPS) {
            uf.stream = newSSLStream(sslh, sock);
            if (sslh)
                SSL_write(sslh, tmp->ptr, tmp->length);
            else
                write(sock, tmp->ptr, tmp->length);
            if (w3m_reqlog) {
                FILE* ff = fopen(w3m_reqlog, "a");
                if (ff == NULL)
                    return uf;
                if (sslh)
                    fputs("HTTPS: request via SSL\n", ff);
                else
                    fputs("HTTPS: request without SSL\n", ff);
                fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
                fclose(ff);
            }
            if (hr->command == HR_COMMAND_POST && request->enctype == FORM_ENCTYPE_MULTIPART) {
                if (sslh)
                    SSL_write_from_file(sslh, request->body);
                else
                    write_from_file(sock, request->body);
            }
            return uf;
        } else {
            write(sock, tmp->ptr, tmp->length);
            if (w3m_reqlog) {
                FILE* ff = fopen(w3m_reqlog, "a");
                if (ff == NULL)
                    return uf;
                fwrite(tmp->ptr, sizeof(char), tmp->length, ff);
                fclose(ff);
            }
            if (hr->command == HR_COMMAND_POST && request->enctype == FORM_ENCTYPE_MULTIPART)
                write_from_file(sock, request->body);
        }
        break;
    case SCM_GOPHER:
        p = pu->file;
        n = 0;
        while (*p == '/') {
            ++p;
            ++n;
        }
        if (*p != '\0') {
            type = pu->file[n];
            switch (type) {
            case '0':
            case '1':
            case 'm':
            case 's':
            case 'g':
            case 'h':
            case 'I':
            case '5':
            case '7':
            case '9':
                tmp = Strnew_charp(pu->file);
                gophertmp = Strdup(tmp);
                Strdelete(tmp, n, 1);
                pu->file = tmp->ptr;
                break;
            default:
                type = '\0';
                break;
            }
        } else {
            type = '\0';
        }
        if (pu->query != NULL) {
            tmp = Strnew_charp(pu->file);
            Strcat_char(tmp, '\t');
            Strcat_charp(tmp, pu->query);
            pu->file = tmp->ptr;
        }
        if (non_null(GOPHER_proxy) && !Do_not_use_proxy && pu->host != NULL && !check_no_proxy(pu->host)) {
            hr->flag |= HR_FLAG_PROXY;
            sock = openSocket(GOPHER_proxy_parsed.host,
                schemeNumToName(GOPHER_proxy_parsed.scheme),
                GOPHER_proxy_parsed.port);
            if (sock < 0)
                return uf;
            uf.scheme = SCM_HTTP;
            tmp = HTTPrequest(pu, current, hr, extra_header);
        } else {
            sock = openSocket(pu->host, schemeNumToName(pu->scheme), pu->port);
            if (sock < 0)
                return uf;
            if (pu->file == NULL)
                pu->file = "1";
            tmp = Strnew_charp(file_unquote(pu->file));
            Strcat_char(tmp, '\n');
        }
        write(sock, tmp->ptr, tmp->length);
        if (type != '\0') {
            pu->file = gophertmp->ptr;
        }
        break;
    case SCM_NNTP:
    case SCM_NNTP_GROUP:
    case SCM_NEWS:
    case SCM_NEWS_GROUP:
        if (pu->scheme == SCM_NNTP || pu->scheme == SCM_NEWS)
            uf.scheme = SCM_NEWS;
        else
            uf.scheme = SCM_NEWS_GROUP;
        uf.stream = openNewsStream(pu);
        return uf;
    case SCM_DATA:
        if (pu->file == NULL)
            return uf;
        p = Strnew_charp(pu->file)->ptr;
        q = strchr(p, ',');
        if (q == NULL)
            return uf;
        *q++ = '\0';
        tmp = Strnew_charp(q);
        q = strrchr(p, ';');
        if (q != NULL && !strcmp(q, ";base64")) {
            *q = '\0';
            uf.encoding = ENC_BASE64;
        } else
            tmp = Str_url_unquote(tmp, FALSE, FALSE);
        uf.stream = newStrStream(tmp);
        uf.guess_type = (*p != '\0') ? p : "text/plain";
        return uf;
    case SCM_UNKNOWN:
    default:
        return uf;
    }
    uf.stream = newInputStream(sock);
    return uf;
}
