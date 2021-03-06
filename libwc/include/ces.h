#pragma once
#include <stdint.h>
#include <assert.h>
#include <array>
#include <string_view>

enum CharacterEncodingScheme : uint32_t
{
    WC_CES_NONE = 0,

    WC_CES_N_US_ASCII = 0,
    WC_CES_N_ISO_8859_1 = 1,
    WC_CES_N_ISO_8859_2 = 2,
    WC_CES_N_ISO_8859_3 = 3,
    WC_CES_N_ISO_8859_4 = 4,
    WC_CES_N_ISO_8859_5 = 5,
    WC_CES_N_ISO_8859_6 = 6,
    WC_CES_N_ISO_8859_7 = 7,
    WC_CES_N_ISO_8859_8 = 8,
    WC_CES_N_ISO_8859_9 = 9,
    WC_CES_N_ISO_8859_10 = 10,
    WC_CES_N_ISO_8859_11 = 11,
    WC_CES_N_ISO_8859_12 = 12,
    WC_CES_N_ISO_8859_13 = 13,
    WC_CES_N_ISO_8859_14 = 14,
    WC_CES_N_ISO_8859_15 = 15,
    WC_CES_N_ISO_8859_16 = 16,
    WC_CES_N_ISO_2022_JP = 17,
    WC_CES_N_ISO_2022_JP_2 = 18,
    WC_CES_N_ISO_2022_JP_3 = 19,
    WC_CES_N_ISO_2022_CN = 20,
    WC_CES_N_ISO_2022_KR = 21,
    WC_CES_N_EUC_JP = 22,
    WC_CES_N_EUC_CN = 23,
    WC_CES_N_EUC_TW = 24,
    WC_CES_N_EUC_KR = 25,
    WC_CES_N_CP437 = 26,
    WC_CES_N_CP737 = 27,
    WC_CES_N_CP775 = 28,
    WC_CES_N_CP850 = 29,
    WC_CES_N_CP852 = 30,
    WC_CES_N_CP855 = 31,
    WC_CES_N_CP856 = 32,
    WC_CES_N_CP857 = 33,
    WC_CES_N_CP860 = 34,
    WC_CES_N_CP861 = 35,
    WC_CES_N_CP862 = 36,
    WC_CES_N_CP863 = 37,
    WC_CES_N_CP864 = 38,
    WC_CES_N_CP865 = 39,
    WC_CES_N_CP866 = 40,
    WC_CES_N_CP869 = 41,
    WC_CES_N_CP874 = 42,
    WC_CES_N_CP1006 = 43,
    WC_CES_N_CP1250 = 44,
    WC_CES_N_CP1251 = 45,
    WC_CES_N_CP1252 = 46,
    WC_CES_N_CP1253 = 47,
    WC_CES_N_CP1254 = 48,
    WC_CES_N_CP1255 = 49,
    WC_CES_N_CP1256 = 50,
    WC_CES_N_CP1257 = 51,
    WC_CES_N_KOI8_R = 52,
    WC_CES_N_KOI8_U = 53,
    WC_CES_N_NEXTSTEP = 54,
    WC_CES_N_RAW = 55,
    WC_CES_N_SHIFT_JIS = 56,
    WC_CES_N_SHIFT_JISX0213 = 57,
    WC_CES_N_GBK = 58,
    WC_CES_N_GB18030 = 59,
    WC_CES_N_HZ_GB_2312 = 60,
    WC_CES_N_BIG5 = 61,
    WC_CES_N_HKSCS = 62,
    WC_CES_N_UHC = 63,
    WC_CES_N_JOHAB = 64,
    WC_CES_N_CP1258 = 65,
    WC_CES_N_TCVN_5712 = 66,
    WC_CES_N_VISCII_11 = 67,
    WC_CES_N_VPS = 68,
    WC_CES_N_UTF_8 = 69,
    WC_CES_N_UTF_7 = 70,

    WC_CES_T_TYPE = 0x31ff00,
    WC_CES_T_NASCII = 0x01fe00,
    WC_CES_T_8BIT = 0x100000,
    WC_CES_T_MBYTE = 0x200000,
    WC_CES_T_ASCII = 0x000100,
    WC_CES_T_ISO_8859 = 0x000200,
    WC_CES_T_PRIV1 = 0x000400,
    WC_CES_T_ISO_2022 = 0x000800,
    WC_CES_T_EUC = 0x001000,
    WC_CES_T_PRIV2 = 0x002000,
    WC_CES_T_VIET = 0x004000,
    WC_CES_T_UTF = 0x008000,
    WC_CES_T_WTF = 0x010000,
    WC_CES_E_ISO_8859 = (WC_CES_T_ISO_8859 | WC_CES_T_8BIT),
    WC_CES_E_PRIV1 = (WC_CES_T_PRIV1 | WC_CES_T_8BIT),
    WC_CES_E_ISO_2022 = (WC_CES_T_ISO_2022 | WC_CES_T_MBYTE),
    WC_CES_E_EUC = (WC_CES_T_EUC | WC_CES_T_8BIT | WC_CES_T_MBYTE),
    WC_CES_E_PRIV2 = (WC_CES_T_PRIV2 | WC_CES_T_8BIT | WC_CES_T_MBYTE),
    WC_CES_E_VIET = (WC_CES_T_VIET | WC_CES_T_PRIV1 | WC_CES_T_8BIT),

    WC_CES_WTF = (WC_CES_T_WTF | WC_CES_T_8BIT | WC_CES_T_MBYTE),

#define WC_CES_TYPE(c) ((c)&WC_CES_T_TYPE)

    WC_CES_US_ASCII = (WC_CES_T_ASCII | WC_CES_N_US_ASCII),

    WC_CES_ISO_8859_1 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_1),
    WC_CES_ISO_8859_2 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_2),
    WC_CES_ISO_8859_3 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_3),
    WC_CES_ISO_8859_4 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_4),
    WC_CES_ISO_8859_5 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_5),
    WC_CES_ISO_8859_6 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_6),
    WC_CES_ISO_8859_7 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_7),
    WC_CES_ISO_8859_8 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_8),
    WC_CES_ISO_8859_9 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_9),
    WC_CES_ISO_8859_10 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_10),
    WC_CES_ISO_8859_11 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_11),
    WC_CES_TIS_620 = WC_CES_ISO_8859_11,
    WC_CES_ISO_8859_12 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_12),
    /* not yet exist */
    WC_CES_ISO_8859_13 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_13),
    WC_CES_ISO_8859_14 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_14),
    WC_CES_ISO_8859_15 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_15),
    WC_CES_ISO_8859_16 = (WC_CES_E_ISO_8859 | WC_CES_N_ISO_8859_16),

    WC_CES_ISO_2022_JP = (WC_CES_E_ISO_2022 | WC_CES_N_ISO_2022_JP),
    WC_CES_ISO_2022_JP_2 = (WC_CES_E_ISO_2022 | WC_CES_N_ISO_2022_JP_2),
    WC_CES_ISO_2022_JP_3 = (WC_CES_E_ISO_2022 | WC_CES_N_ISO_2022_JP_3),
    WC_CES_ISO_2022_CN = (WC_CES_E_ISO_2022 | WC_CES_N_ISO_2022_CN),
    WC_CES_ISO_2022_KR = (WC_CES_E_ISO_2022 | WC_CES_N_ISO_2022_KR),

    WC_CES_EUC_JP = (WC_CES_E_EUC | WC_CES_N_EUC_JP),
    WC_CES_EUC_CN = (WC_CES_E_EUC | WC_CES_N_EUC_CN),
    WC_CES_EUC_TW = (WC_CES_E_EUC | WC_CES_N_EUC_TW),
    WC_CES_EUC_KR = (WC_CES_E_EUC | WC_CES_N_EUC_KR),

    WC_CES_CP437 = (WC_CES_E_PRIV1 | WC_CES_N_CP437),
    WC_CES_CP737 = (WC_CES_E_PRIV1 | WC_CES_N_CP737),
    WC_CES_CP775 = (WC_CES_E_PRIV1 | WC_CES_N_CP775),
    WC_CES_CP850 = (WC_CES_E_PRIV1 | WC_CES_N_CP850),
    WC_CES_CP852 = (WC_CES_E_PRIV1 | WC_CES_N_CP852),
    WC_CES_CP855 = (WC_CES_E_PRIV1 | WC_CES_N_CP855),
    WC_CES_CP856 = (WC_CES_E_PRIV1 | WC_CES_N_CP856),
    WC_CES_CP857 = (WC_CES_E_PRIV1 | WC_CES_N_CP857),
    WC_CES_CP860 = (WC_CES_E_PRIV1 | WC_CES_N_CP860),
    WC_CES_CP861 = (WC_CES_E_PRIV1 | WC_CES_N_CP861),
    WC_CES_CP862 = (WC_CES_E_PRIV1 | WC_CES_N_CP862),
    WC_CES_CP863 = (WC_CES_E_PRIV1 | WC_CES_N_CP863),
    WC_CES_CP864 = (WC_CES_E_PRIV1 | WC_CES_N_CP864),
    WC_CES_CP865 = (WC_CES_E_PRIV1 | WC_CES_N_CP865),
    WC_CES_CP866 = (WC_CES_E_PRIV1 | WC_CES_N_CP866),
    WC_CES_CP869 = (WC_CES_E_PRIV1 | WC_CES_N_CP869),
    WC_CES_CP874 = (WC_CES_E_PRIV1 | WC_CES_N_CP874),
    WC_CES_CP1006 = (WC_CES_E_PRIV1 | WC_CES_N_CP1006),
    WC_CES_CP1250 = (WC_CES_E_PRIV1 | WC_CES_N_CP1250),
    WC_CES_CP1251 = (WC_CES_E_PRIV1 | WC_CES_N_CP1251),
    WC_CES_CP1252 = (WC_CES_E_PRIV1 | WC_CES_N_CP1252),
    WC_CES_CP1253 = (WC_CES_E_PRIV1 | WC_CES_N_CP1253),
    WC_CES_CP1254 = (WC_CES_E_PRIV1 | WC_CES_N_CP1254),
    WC_CES_CP1255 = (WC_CES_E_PRIV1 | WC_CES_N_CP1255),
    WC_CES_CP1256 = (WC_CES_E_PRIV1 | WC_CES_N_CP1256),
    WC_CES_CP1257 = (WC_CES_E_PRIV1 | WC_CES_N_CP1257),
    WC_CES_KOI8_R = (WC_CES_E_PRIV1 | WC_CES_N_KOI8_R),
    WC_CES_KOI8_U = (WC_CES_E_PRIV1 | WC_CES_N_KOI8_U),
    WC_CES_NEXTSTEP = (WC_CES_E_PRIV1 | WC_CES_N_NEXTSTEP),
    WC_CES_RAW = (WC_CES_E_PRIV1 | WC_CES_N_RAW),

    WC_CES_SHIFT_JIS = (WC_CES_E_PRIV2 | WC_CES_N_SHIFT_JIS),
    WC_CES_CP932 = WC_CES_SHIFT_JIS,
    WC_CES_CP943 = WC_CES_SHIFT_JIS,
    WC_CES_SHIFT_JISX0213 = (WC_CES_E_PRIV2 | WC_CES_N_SHIFT_JISX0213),
    WC_CES_GBK = (WC_CES_E_PRIV2 | WC_CES_N_GBK),
    WC_CES_CP936 = WC_CES_GBK,
    WC_CES_GB18030 = (WC_CES_E_PRIV2 | WC_CES_N_GB18030),
    WC_CES_HZ_GB_2312 = (WC_CES_T_PRIV2 | WC_CES_T_MBYTE | WC_CES_N_HZ_GB_2312),
    WC_CES_BIG5 = (WC_CES_E_PRIV2 | WC_CES_N_BIG5),
    WC_CES_CP950 = WC_CES_BIG5,
    WC_CES_HKSCS = (WC_CES_E_PRIV2 | WC_CES_N_HKSCS),
    WC_CES_UHC = (WC_CES_E_PRIV2 | WC_CES_N_UHC),
    WC_CES_CP949 = WC_CES_UHC,
    WC_CES_JOHAB = (WC_CES_E_PRIV2 | WC_CES_N_JOHAB),

    WC_CES_CP1258 = (WC_CES_E_PRIV1 | WC_CES_N_CP1258),
    WC_CES_TCVN_5712 = (WC_CES_E_VIET | WC_CES_N_TCVN_5712),
    WC_CES_VISCII_11 = (WC_CES_E_VIET | WC_CES_N_VISCII_11),
    WC_CES_VPS = (WC_CES_E_VIET | WC_CES_N_VPS),

    WC_CES_UTF_8 = (WC_CES_T_UTF | WC_CES_T_8BIT | WC_CES_T_MBYTE | WC_CES_N_UTF_8),
    WC_CES_UTF_7 = (WC_CES_T_UTF | WC_CES_T_MBYTE | WC_CES_N_UTF_7),

    WC_CES_END = WC_CES_N_UTF_7,
};
inline CharacterEncodingScheme operator~(CharacterEncodingScheme a) { return (CharacterEncodingScheme) ~(int)a; }
inline CharacterEncodingScheme operator|(CharacterEncodingScheme a, CharacterEncodingScheme b) { return (CharacterEncodingScheme)((int)a | (int)b); }
inline CharacterEncodingScheme operator&(CharacterEncodingScheme a, CharacterEncodingScheme b) { return (CharacterEncodingScheme)((int)a & (int)b); }
inline CharacterEncodingScheme operator^(CharacterEncodingScheme a, CharacterEncodingScheme b) { return (CharacterEncodingScheme)((int)a ^ (int)b); }
inline CharacterEncodingScheme &operator|=(CharacterEncodingScheme &a, CharacterEncodingScheme b) { return (CharacterEncodingScheme &)((int &)a |= (int)b); }
inline CharacterEncodingScheme &operator&=(CharacterEncodingScheme &a, CharacterEncodingScheme b) { return (CharacterEncodingScheme &)((int &)a &= (int)b); }
inline CharacterEncodingScheme &operator^=(CharacterEncodingScheme &a, CharacterEncodingScheme b) { return (CharacterEncodingScheme &)((int &)a ^= (int)b); }

union SingleCharacter
{
    // uint64_t value;
    char32_t value;
    struct
    {
        uint16_t low;
        uint16_t high;
    };
    std::array<char8_t, 4> bytes;

    /// src: string include a character. allow multibyte, any encoding
    // CharacterCode(std::string_view src)
    // {
    //     assert(src.size() < bytes.size());
    //     int i = 0;
    //     for (; i < src.size(); ++i)
    //     {
    //         bytes[i] = src[i];
    //     }
    //     for (; i < bytes.size(); ++i)
    //     {
    //         bytes[i] = 0;
    //     }
    // }

    template <typename T>
    SingleCharacter(const T *src)
    {
        static_assert(sizeof(T) == 1); // char variant
        uint32_t i = 0;
        for (; i < bytes.size(); ++i, ++src)
        {
            if (*src == 0)
            {
                break;
            }
            bytes[i] = *src;
        }
        for (; i < bytes.size(); ++i, ++src)
        {
            bytes[i] = 0;
        }
    }

    template <typename T>
    SingleCharacter(const T *src, uint32_t size)
    {
        static_assert(sizeof(T) == 1); // char variant
        if (size > 8)
        {
            throw std::runtime_error("out of range");
        }
        uint32_t i = 0;
        for (; i < size; ++i, ++src)
        {
            if (!*src)
            {
                break;
            }
            bytes[i] = *src;
        }
        for (; i < bytes.size(); ++i, ++src)
        {
            bytes[i] = 0;
        }
    }

    SingleCharacter()
    {
        value = 0;
    }

    uint32_t size() const
    {
        // if (bytes[7])
        // {
        //     return 8;
        // }
        // if (bytes[6])
        // {
        //     return 7;
        // }
        // if (bytes[5])
        // {
        //     return 6;
        // }
        // if (bytes[4])
        // {
        //     return 5;
        // }
        if (bytes[3])
        {
            return 4;
        }
        if (bytes[2])
        {
            return 3;
        }
        if (bytes[1])
        {
            return 2;
        }
        if (bytes[0])
        {
            return 1;
        }
        return 0;
    }

    explicit SingleCharacter(uint8_t c)
    {
        value = 0;
        bytes[0] = c;
    }

    explicit SingleCharacter(uint16_t c)
    {
        value = 0;
        low = c;
    }

    explicit SingleCharacter(uint64_t c)
    {
        value = c;
    }

    enum
    {
        UTF8_HEAD1 = 128,
        UTF8_HEAD2 = 128 + 64,
        UTF8_HEAD3 = 128 + 64 + 32,
        UTF8_HEAD4 = 128 + 64 + 32 + 16,
        UTF8_MASK6 = 32 + 16 + 8 + 4 + 2 + 1,
        UTF8_MASK5 = 16 + 8 + 4 + 2 + 1,
        UTF8_MASK4 = 8 + 4 + 2 + 1,
        UTF8_MASK3 = 4 + 2 + 1,
    };

    static SingleCharacter as_utf8(const char *utf8)
    {
        return as_utf8((const char8_t *)utf8);
    }
    static SingleCharacter as_utf8(const char8_t *utf8)
    {
        if (utf8[0] < 0x8f)
        {
            // ascii
            return SingleCharacter(utf8, 1);
        }

        auto masked = utf8[0] & UTF8_HEAD4;
        if (masked == UTF8_HEAD4)
        {
            // 4
            return SingleCharacter(utf8, 4);
        }
        else if (masked == UTF8_HEAD3)
        {
            // 3
            return SingleCharacter(utf8, 3);
        }
        else if (masked == UTF8_HEAD2)
        {
            // 2
            return SingleCharacter(utf8, 2);
        }
        else
        {
            assert(false);
            return {};
        }
    }

    static SingleCharacter unicode_from_utf8(const char8_t *utf8)
    {
        if (utf8[0] < 0x8f)
        {
            // ascii
            return SingleCharacter(utf8, 1);
        }

        SingleCharacter sc;
        auto masked = utf8[0] & UTF8_HEAD4;
        if (masked == UTF8_HEAD4)
        {
            // 4
        }
        else if (masked == UTF8_HEAD3)
        {
            // 3
            sc.value += ((utf8[0] & UTF8_MASK5) << 12);
            sc.value += ((utf8[1] & UTF8_MASK6) << 6);
            sc.value += (utf8[2] & UTF8_MASK6);
        }
        else if (masked == UTF8_HEAD2)
        {
            // 2
        }
        else
        {
            assert(false);
        }

        return sc;
    }

    static SingleCharacter unicode_to_utf8(char32_t unicode)
    {
        SingleCharacter sc;
        if (unicode <= 0x7f)
        {
            sc.bytes[0] = unicode;
        }
        else if (unicode <= 0x7ff)
        {
            sc.bytes[0] = UTF8_HEAD2 | UTF8_MASK5 & (unicode >> 6);
            sc.bytes[1] = UTF8_HEAD1 | UTF8_MASK6 & unicode;
        }
        else if (unicode <= 0xFFFF)
        {
            // to big endian
            sc.bytes[0] = UTF8_HEAD3 | ((unicode >> 12));
            sc.bytes[1] = UTF8_HEAD1 | (UTF8_MASK6 & (unicode >> 6));
            sc.bytes[2] = UTF8_HEAD1 | (UTF8_MASK6 & unicode);
            auto a = 0;
        }
        else if (unicode <= 0x10FFFF)
        {
            assert(false);
        }
        else
        {
            assert(false);
        }
        return sc;
    }

    bool operator==(const SingleCharacter &rhs) const
    {
        return value == rhs.value;
    }
};
static_assert(sizeof(SingleCharacter) == 4);
