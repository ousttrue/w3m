#include "conv.h"
#include "status.h"
#include "ucs.h"
#include "utf8.h"
#include "wtf.h"

// clang-format off
uint8_t WC_UTF8_MAP[ 0x100 ] = {
   8, 8, 8, 8, 8, 8, 8, 8,  8, 8, 8, 8, 8, 8, 8, 8,
   8, 8, 8, 8, 8, 8, 8, 8,  8, 8, 8, 8, 8, 8, 8, 8,
   1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1,  1, 1, 1, 1, 1, 1, 1, 8,

   0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
   2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2,
   2, 2, 2, 2, 2, 2, 2, 2,  2, 2, 2, 2, 2, 2, 2, 2,
   3, 3, 3, 3, 3, 3, 3, 3,  3, 3, 3, 3, 3, 3, 3, 3,
   4, 4, 4, 4, 4, 4, 4, 4,  5, 5, 5, 5, 6, 6, 7, 7,
};
// clang-format on

static uint8_t utf8_buf[7];

size_t
wc_ucs_to_utf8(uint32_t ucs, uint8_t* utf8)
{
    if (ucs < WC_C_UTF8_L2) {
        utf8[0] = ucs;
        utf8[1] = 0;
        return 1;
    } else if (ucs < WC_C_UTF8_L3) {
        utf8[0] = (ucs >> 6) | 0xc0;
        utf8[1] = (ucs & 0x3f) | 0x80;
        utf8[2] = 0;
        return 2;
    } else if (ucs < WC_C_UTF8_L4) {
        utf8[0] = (ucs >> 12) | 0xe0;
        utf8[1] = ((ucs >> 6) & 0x3f) | 0x80;
        utf8[2] = (ucs & 0x3f) | 0x80;
        utf8[3] = 0;
        return 3;
    } else if (ucs < WC_C_UTF8_L5) {
        utf8[0] = (ucs >> 18) | 0xf0;
        utf8[1] = ((ucs >> 12) & 0x3f) | 0x80;
        utf8[2] = ((ucs >> 6) & 0x3f) | 0x80;
        utf8[3] = (ucs & 0x3f) | 0x80;
        utf8[4] = 0;
        return 4;
    } else if (ucs < WC_C_UTF8_L6) {
        utf8[0] = (ucs >> 24) | 0xf8;
        utf8[1] = ((ucs >> 18) & 0x3f) | 0x80;
        utf8[2] = ((ucs >> 12) & 0x3f) | 0x80;
        utf8[3] = ((ucs >> 6) & 0x3f) | 0x80;
        utf8[4] = (ucs & 0x3f) | 0x80;
        utf8[5] = 0;
        return 5;
    } else if (ucs <= WC_C_UCS4_END) {
        utf8[0] = (ucs >> 30) | 0xfc;
        utf8[1] = ((ucs >> 24) & 0x3f) | 0x80;
        utf8[2] = ((ucs >> 18) & 0x3f) | 0x80;
        utf8[3] = ((ucs >> 12) & 0x3f) | 0x80;
        utf8[4] = ((ucs >> 6) & 0x3f) | 0x80;
        utf8[5] = (ucs & 0x3f) | 0x80;
        utf8[6] = 0;
        return 6;
    } else {
        utf8[0] = 0;
        return 0;
    }
}

uint32_t
wc_utf8_to_ucs(const uint8_t* utf8)
{
    uint32_t ucs;

    switch (WC_UTF8_MAP[utf8[0]]) {
    case 1:
        ucs = (uint32_t)utf8[0];
        if (ucs >= WC_C_UTF8_L2)
            break;
        return ucs;
    case 2:
        ucs = ((uint32_t)(utf8[0] & 0x1f) << 6)
            | (uint32_t)(utf8[1] & 0x3f);
        if (ucs < WC_C_UTF8_L2)
            break;
        return ucs;
    case 3:
        ucs = ((uint32_t)(utf8[0] & 0x0f) << 12)
            | ((uint32_t)(utf8[1] & 0x3f) << 6)
            | (uint32_t)(utf8[2] & 0x3f);
        if (ucs < WC_C_UTF8_L3)
            break;
        return ucs;
    case 4:
        ucs = ((uint32_t)(utf8[0] & 0x07) << 18)
            | ((uint32_t)(utf8[1] & 0x3f) << 12)
            | ((uint32_t)(utf8[2] & 0x3f) << 6)
            | (uint32_t)(utf8[3] & 0x3f);
        if (ucs < WC_C_UTF8_L4)
            break;
        return ucs;
    case 5:
        ucs = ((uint32_t)(utf8[0] & 0x03) << 24)
            | ((uint32_t)(utf8[1] & 0x3f) << 18)
            | ((uint32_t)(utf8[2] & 0x3f) << 12)
            | ((uint32_t)(utf8[3] & 0x3f) << 6)
            | (uint32_t)(utf8[4] & 0x3f);
        if (ucs < WC_C_UTF8_L5)
            break;
        return ucs;
    case 6:
        ucs = ((uint32_t)(utf8[0] & 0x01) << 30)
            | ((uint32_t)(utf8[1] & 0x3f) << 24)
            | ((uint32_t)(utf8[2] & 0x3f) << 18)
            | ((uint32_t)(utf8[3] & 0x3f) << 12)
            | ((uint32_t)(utf8[4] & 0x3f) << 6)
            | (uint32_t)(utf8[5] & 0x3f);
        if (ucs < WC_C_UTF8_L6)
            break;
        return ucs;
    default:
        break;
    }
    return WC_C_UCS4_ERROR;
}

void wc_conv_from_utf8(struct wc_option* WcOption, struct Writer* w, const uint8_t* sp, const uint8_t* ep, wc_ces ces)
{
    const uint8_t* q = NULL;
    int state = WC_UTF8_NOSTATE;
    size_t next = 0;
    uint32_t ucs;

    const uint8_t* p;
    for (p = sp; p < ep && *p < 0x80; p++)
        ;
    if (p == ep) {
        CALL2(w, setBeginEnd, sp, ep);
        return;
    }

    if (p > sp)
        CALL2(w, pushStrLen, sp, (int)(p - sp));

    struct wc_status st;
    st.tag_data = (struct ArrayData) {
        .buf = st.tag_buf,
        .len = 0,
        .capacity = sizeof(st.tag_buf),
    };
    st.tag = arrayWriter(&st.tag_data);

    st.ntag = 0;
    for (; p < ep; p++) {
        switch (state) {
        case WC_UTF8_NOSTATE:
            next = WC_UTF8_MAP[*p];
            switch (next) {
            case 1:
                wtf_push_ucs(WcOption, w, (uint32_t)*p, &st);
                break;
            case 8:
                WRITER_PUSH_CH(w,*p);
                break;
            case 0:
            case 7:
                wtf_push_unknown(WcOption, w, p, 1);
                break;
            default:
                q = p;
                next--;
                state = WC_UTF8_NEXT;
                break;
            }
            break;
        case WC_UTF8_NEXT:
            if (WC_UTF8_MAP[*p]) {
                wtf_push_unknown(WcOption, w, q, p - q + 1);
                state = WC_UTF8_NOSTATE;
                break;
            }
            if (--next)
                break;
            state = WC_UTF8_NOSTATE;
            ucs = wc_utf8_to_ucs(q);
            if (ucs == WC_C_UCS4_ERROR || (ucs >= WC_C_UCS2_SURROGATE && ucs <= WC_C_UCS2_SURROGATE_END))
                wtf_push_unknown(WcOption, w, q, p - q + 1);
            else if (ucs != WC_C_UCS2_BOM)
                wtf_push_ucs(WcOption, w, ucs, &st);
            break;
        }
    }
    switch (state) {
    case WC_UTF8_NEXT:
        wtf_push_unknown(WcOption, w, q, p - q);
        break;
    }
}

static int
wc_push_tag_to_utf8(struct Writer* w, int ntag)
{
    const char* p;

    if (ntag) {
        p = wc_ucs_get_tag(ntag);
        if (p == NULL)
            ntag = 0;
    }
    if (ntag) {
        wc_ucs_to_utf8(WC_C_LANGUAGE_TAG, utf8_buf);
        CALL1(w, pushStr, utf8_buf);
        for (; *p; p++) {
            wc_ucs_to_utf8(WC_C_LANGUAGE_TAG0 | *p, utf8_buf);
            CALL1(w, pushStr, utf8_buf);
        }
    } else {
        wc_ucs_to_utf8(WC_C_CANCEL_TAG, utf8_buf);
        CALL1(w, pushStr, utf8_buf);
    }
    return ntag;
}

void wc_push_to_utf8(struct wc_option* WcOption, struct Writer* w, wc_wchar_t cc, struct wc_status* st)
{
    while (1) {
        switch (WC_CCS_SET(cc.ccs)) {
        case WC_CCS_US_ASCII:
            if (st->ntag)
                st->ntag = wc_push_tag_to_utf8(w, 0);
            WRITER_PUSH_CH(w,(cc.code & 0x7f));
            return;
        case WC_CCS_UCS2:
        case WC_CCS_UCS4:
            if (st->ntag)
                st->ntag = wc_push_tag_to_utf8(w, 0);
            wc_ucs_to_utf8(cc.code, utf8_buf);
            CALL1(w, pushStr, utf8_buf);
            return;
        case WC_CCS_UCS_TAG:
            if (WcOption->use_language_tag && wc_ucs_tag_to_tag(cc.code) != st->ntag)
                st->ntag = wc_push_tag_to_utf8(w, wc_ucs_tag_to_tag(cc.code));
            wc_ucs_to_utf8(wc_ucs_tag_to_ucs(cc.code), utf8_buf);
            CALL1(w, pushStr, utf8_buf);
            return;
        case WC_CCS_ISO_8859_1:
            if (st->ntag)
                st->ntag = wc_push_tag_to_utf8(w, 0);
            wc_ucs_to_utf8((cc.code | 0x80), utf8_buf);
            CALL1(w, pushStr, utf8_buf);
            return;
        case WC_CCS_UNKNOWN_W:
            if (!WcOption->no_replace) {
                if (st->ntag)
                    st->ntag = wc_push_tag_to_utf8(w, 0);
                CALL1(w, pushStr, WC_REPLACE_W);
            }
            return;
        case WC_CCS_UNKNOWN:
            if (!WcOption->no_replace) {
                if (st->ntag)
                    st->ntag = wc_push_tag_to_utf8(w, 0);
                CALL1(w, pushStr, WC_REPLACE);
            }
            return;
        default:
            if (WcOption->ucs_conv && (cc.code = wc_any_to_ucs(WcOption, cc)) != WC_C_UCS4_ERROR)
                cc.ccs = WC_CCS_UCS2;
            else
                cc.ccs = WC_CCS_IS_WIDE(cc.ccs) ? WC_CCS_UNKNOWN_W : WC_CCS_UNKNOWN;
            continue;
        }
    }
}

void wc_push_to_utf8_end(struct wc_option* WcOption, struct Writer* w, struct wc_status* st)
{
    if (st->ntag)
        st->ntag = wc_push_tag_to_utf8(w, 0);
    return;
}

void wc_char_conv_from_utf8(struct wc_option* WcOption, struct Writer* w, uint8_t c, struct wc_status* st)
{
    static uint8_t buf[6];
    static size_t nbuf, next;
    uint32_t ucs;

    if (st->state == -1) {
        st->state = WC_UTF8_NOSTATE;
        CALL0(&st->tag, clear);
        st->ntag = 0;
        nbuf = 0;
    }

    switch (st->state) {
    case WC_UTF8_NOSTATE:
        switch (next = WC_UTF8_MAP[c]) {
        case 1:
            wtf_push_ucs(WcOption, w, (uint32_t)c, st);
            break;
        case 8:
            WRITER_PUSH_CH(w,(char)c);
            break;
        case 0:
        case 7:
            break;
        default:
            buf[nbuf++] = c;
            next--;
            st->state = WC_UTF8_NEXT;
            return;
        }
        break;
    case WC_UTF8_NEXT:
        if (WC_UTF8_MAP[c])
            break;
        buf[nbuf++] = c;
        if (--next)
            return;
        ucs = wc_utf8_to_ucs(buf);
        if (ucs == WC_C_UCS4_ERROR || (ucs >= WC_C_UCS2_SURROGATE && ucs <= WC_C_UCS2_SURROGATE_END))
            break;
        if (ucs != WC_C_UCS2_BOM)
            wtf_push_ucs(WcOption, w, ucs, st);
        break;
    }
    st->state = -1;
}
