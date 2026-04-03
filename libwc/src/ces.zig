const c = @cImport({
    @cInclude("ces.h");
    @cInclude("iso2022.h");
    @cInclude("sjis.h");
    @cInclude("hz.h");
    @cInclude("big5.h");
    @cInclude("hkscs.h");
    @cInclude("johab.h");
    @cInclude("gbk.h");
    @cInclude("gb18030.h");
    @cInclude("uhc.h");
    @cInclude("viet.h");
    @cInclude("utf8.h");
    @cInclude("utf7.h");
});

const gset_usascii = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = c.WC_C_G0_CS94, .init = 1 },
    .{},
};

fn gset_iso8859(no: []const u8) [3]c.wc_gset {
    return [3]c.wc_gset{
        .{ .ccs = c.WC_CCS_US_ASCII, .g = c.WC_C_G0_CS94, .init = 1 },
        .{ .ccs = @field(c, "WC_CCS_ISO_8859_" ++ no), .g = c.WC_C_G1_CS96 | 0x80, .init = 1 },
        .{},
    };
}

fn gset_cp(no: []const u8) [3]c.wc_gset {
    return gset_priv1("CP" ++ no);
}
fn gset_priv1(no: []const u8) [3]c.wc_gset {
    return [3]c.wc_gset{
        .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
        .{ .ccs = @field(c, "WC_CCS_" ++ no), .g = 0x80, .init = 1 },
        .{},
    };
}

const gset_iso2022jp = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = c.WC_C_G0_CS94, .init = 1 },
    .{ .ccs = c.WC_CCS_JIS_X_0208, .g = c.WC_C_G0_CS94, .init = 0 },
    .{},
};
const gset_ext_iso2022jp = [_]c.wc_uchar{ c.WC_C_G0_CS94, c.WC_C_G2_CS96, c.WC_C_G0_CS94, c.WC_C_G2_CS96 };
const gset_iso2022jp2 = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = c.WC_C_G0_CS94, .init = 1 },
    .{ .ccs = c.WC_CCS_JIS_X_0208, .g = c.WC_C_G0_CS94, .init = 0 },
    .{ .ccs = c.WC_CCS_JIS_X_0212, .g = c.WC_C_G0_CS94, .init = 0 },
    .{ .ccs = c.WC_CCS_GB_2312, .g = c.WC_C_G0_CS94, .init = 0 },
    .{ .ccs = c.WC_CCS_KS_X_1001, .g = c.WC_C_G0_CS94, .init = 0 },
    .{ .ccs = c.WC_CCS_ISO_8859_1, .g = c.WC_C_G2_CS96, .init = 0 },
    .{ .ccs = c.WC_CCS_ISO_8859_7, .g = c.WC_C_G2_CS96, .init = 0 },
    .{},
};
const gset_ext_iso2022jp2 = gset_ext_iso2022jp;
const gset_iso2022jp3 = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = c.WC_C_G0_CS94, .init = 1 },
    .{ .ccs = c.WC_CCS_JIS_X_0208, .g = c.WC_C_G0_CS94, .init = 0 },
    .{ .ccs = c.WC_CCS_JIS_X_0213_1, .g = c.WC_C_G0_CS94, .init = 0 },
    .{ .ccs = c.WC_CCS_JIS_X_0213_2, .g = c.WC_C_G0_CS94, .init = 0 },
    .{},
};
const gset_ext_iso2022jp3 = gset_ext_iso2022jp;
const gset_iso2022cn = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = c.WC_C_G0_CS94, .init = 1 },
    .{ .ccs = c.WC_CCS_GB_2312, .g = c.WC_C_G1_CS94, .init = 1 },
    .{ .ccs = c.WC_CCS_ISO_IR_165, .g = c.WC_C_G1_CS94, .init = 0 },
    .{ .ccs = c.WC_CCS_CNS_11643_1, .g = c.WC_C_G1_CS94, .init = 0 },
    .{ .ccs = c.WC_CCS_CNS_11643_2, .g = c.WC_C_G2_CS94, .init = 0 },
    .{ .ccs = c.WC_CCS_CNS_11643_3, .g = c.WC_C_G3_CS94, .init = 0 },
    .{ .ccs = c.WC_CCS_CNS_11643_4, .g = c.WC_C_G3_CS94, .init = 0 },
    .{ .ccs = c.WC_CCS_CNS_11643_5, .g = c.WC_C_G3_CS94, .init = 0 },
    .{ .ccs = c.WC_CCS_CNS_11643_6, .g = c.WC_C_G3_CS94, .init = 0 },
    .{ .ccs = c.WC_CCS_CNS_11643_7, .g = c.WC_C_G3_CS94, .init = 0 },
    .{},
};
const gset_ext_iso2022cn = [_]c.wc_uchar{ c.WC_C_G2_CS94, c.WC_C_G2_CS96, c.WC_C_G2_CS94, c.WC_C_G2_CS96 };
const gset_iso2022kr = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = c.WC_C_G0_CS94, .init = 1 },
    .{ .ccs = c.WC_CCS_KS_X_1001, .g = c.WC_C_G1_CS94, .init = 1 },
    .{},
};
const gset_ext_iso2022kr = [_]c.wc_uchar{ c.WC_C_G1_CS94, c.WC_C_G1_CS96, c.WC_C_G1_CS94, c.WC_C_G1_CS96 };
const gset_eucjp = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = c.WC_C_G0_CS94, .init = 1 },
    .{ .ccs = c.WC_CCS_JIS_X_0208, .g = c.WC_C_G1_CS94 | 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_JIS_X_0201K, .g = c.WC_C_G2_CS94 | 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_JIS_X_0213_1, .g = c.WC_C_G1_CS94 | 0x80, .init = 0 },
    .{ .ccs = c.WC_CCS_JIS_X_0213_2, .g = c.WC_C_G3_CS94 | 0x80, .init = 0 },
    .{ .ccs = c.WC_CCS_JIS_X_0212, .g = c.WC_C_G3_CS94 | 0x80, .init = 1 },
    .{},
};
const gset_euccn = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = c.WC_C_G0_CS94, .init = 1 },
    .{ .ccs = c.WC_CCS_GB_2312, .g = c.WC_C_G1_CS94 | 0x80, .init = 1 },
    .{},
};
const gset_euctw = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = c.WC_C_G0_CS94, .init = 1 },
    .{ .ccs = c.WC_CCS_CNS_11643_1, .g = c.WC_C_G1_CS94 | 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_CNS_11643_X, .g = c.WC_C_G2_CS94 | 0x80, .init = 1 },
    .{},
};
const gset_euckr = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = c.WC_C_G0_CS94, .init = 1 },
    .{ .ccs = c.WC_CCS_KS_X_1001, .g = c.WC_C_G1_CS94 | 0x80, .init = 1 },
    .{},
};
const gset_sjis = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
    .{ .ccs = c.WC_CCS_JIS_X_0208, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_JIS_X_0201K, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_SJIS_EXT_1, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_SJIS_EXT_2, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_SJIS_EXT, .g = 0x80, .init = 1 },
    .{},
};
const gset_sjisx0213 = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
    .{ .ccs = c.WC_CCS_JIS_X_0208, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_JIS_X_0201K, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_JIS_X_0213_1, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_JIS_X_0213_2, .g = 0x80, .init = 1 },
    .{},
};
const gset_hz = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
    .{ .ccs = c.WC_CCS_GB_2312, .g = 0, .init = 0 },
    .{},
};
const gset_big5 = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
    .{ .ccs = c.WC_CCS_BIG5_1, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_BIG5_2, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_BIG5, .g = 0x80, .init = 1 },
    .{},
};
const gset_hkscs = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
    .{ .ccs = c.WC_CCS_BIG5_1, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_BIG5_2, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_BIG5, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_HKSCS_1, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_HKSCS_2, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_HKSCS, .g = 0x80, .init = 1 },
    .{},
};
const gset_johab = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
    .{ .ccs = c.WC_CCS_JOHAB_1, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_JOHAB_2, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_JOHAB_3, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_JOHAB, .g = 0x80, .init = 1 },
    .{},
};
const gset_gbk = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
    .{ .ccs = c.WC_CCS_GB_2312, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_GBK_80, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_GBK_1, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_GBK_2, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_GBK, .g = 0x80, .init = 1 },
    .{},
};
const gset_gb18030 = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
    .{ .ccs = c.WC_CCS_GB_2312, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_GBK_1, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_GBK_2, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_GBK, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_GBK_EXT_1, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_GBK_EXT_2, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_GBK_EXT, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_GB18030, .g = 0x80, .init = 1 },
    .{},
};
const gset_uhc = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
    .{ .ccs = c.WC_CCS_KS_X_1001, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_UHC_1, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_UHC_2, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_UHC, .g = 0x80, .init = 1 },
    .{},
};
fn gset_priv2(ccs: []const u8) [4]c.wc_gset {
    return [4]c.wc_gset{
        .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
        .{ .ccs = @field(c, "WC_CCS_" ++ ccs ++ "_1"), .g = 0x80, .init = 1 },
        .{ .ccs = @field(c, "WC_CCS_" ++ ccs ++ "_2"), .g = 0x80, .init = 1 },
        .{},
    };
}

const gset_cp1258 = gset_priv2("CP1258");
const gset_viscii11 = gset_priv2("VISCII_11");
const gset_vps = gset_priv2("VPS");
const gset_tcvn5712 = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
    .{ .ccs = c.WC_CCS_TCVN_5712_1, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_TCVN_5712_2, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_TCVN_5712_3, .g = 0x80, .init = 1 },
    .{},
};

const gset_utf8 = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
    .{ .ccs = c.WC_CCS_UCS2, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_UCS4, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_UCS_TAG, .g = 0x80, .init = 1 },
    .{},
};
const gset_utf7 = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
    .{ .ccs = c.WC_CCS_UCS2, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_UCS4, .g = 0x80, .init = 1 },
    .{ .ccs = c.WC_CCS_UCS_TAG, .g = 0x80, .init = 1 },
    .{},
};

const gset_raw = [_]c.wc_gset{
    .{ .ccs = c.WC_CCS_US_ASCII, .g = 0, .init = 1 },
    .{ .ccs = c.WC_CCS_RAW, .g = 0x80, .init = 1 },
    .{},
};

fn ces_ascii(id: c.wc_ces, name: [*c]const u8, desc: [*c]const u8) c.wc_ces_info {
    return .{
        .id = id,
        .name = name,
        .desc = desc,
        .gset = &gset_usascii,
        .gset_ext = null,
        .conv_from =  c.wc_conv_from_ascii,
        .push_to = c.wc_push_to_iso8859,
        .char_conv = c.wc_char_conv_from_iso2022,
    };
}
fn ces_iso8859(id: c.wc_ces, name: [*c]const u8, desc: [*c]const u8, no: []const u8) c.wc_ces_info {
    return .{
        .id = id,
        .name = name,
        .desc = desc,
        .gset = &gset_iso8859(no),
        .gset_ext = null,
        .conv_from = c.wc_conv_from_iso2022,
        .push_to = c.wc_push_to_iso8859,
        .char_conv = c.wc_char_conv_from_iso2022,
    };
}
fn ces_priv1(id: c.wc_ces, name: [*c]const u8, desc: [*c]const u8, gset: [*c]const c.wc_gset) c.wc_ces_info {
    return .{
        .id = id,
        .name = name,
        .desc = desc,
        .gset = gset,
        .gset_ext = null,
        .conv_from = c.wc_conv_from_priv1,
        .push_to = wc_push_to_priv1,
        .char_conv = c.wc_char_conv_from_priv1,
    };
}

fn ces_iso2022(id: c.wc_ces, name: [*c]const u8, desc: [*c]const u8, terr: []const u8) c.wc_ces_info {
    return .{
        .id = id,
        .name = name,
        .desc = desc,
        .gset = &@field(@This(), "gset_iso2022" ++ terr),
        .gset_ext = &@field(@This(), "gset_ext_iso2022" ++ terr),
        .conv_from = c.wc_conv_from_iso2022,
        .push_to = c.wc_push_to_iso2022,
        .char_conv = c.wc_char_conv_from_iso2022,
    };
}
fn ces_euc(id: c.wc_ccs, name: [*c]const u8, desc: [*c]const u8, terr: []const u8) c.wc_ces_info {
    return .{
        .id = id,
        .name = name,
        .desc = desc,
        .gset = &@field(@This(), "gset_euc" ++ terr),
        .gset_ext = null,
        .conv_from = c.wc_conv_from_iso2022,
        .push_to = @field(@This(), "wc_push_to_euc" ++ terr),
        .char_conv = c.wc_char_conv_from_iso2022,
    };
}
const wc_push_to_eucjp = c.wc_push_to_eucjp;
const wc_push_to_euctw = c.wc_push_to_euctw;
const wc_push_to_euckr = c.wc_push_to_euc;
const wc_push_to_euccn = c.wc_push_to_euc;

fn ces_priv2(id: c.wc_ces, name: [*c]const u8, desc: [*c]const u8, T: type, ces: []const u8) c.wc_ces_info {
    // #define ces_priv2(id, name, desc, ces)           \
    return .{
        .id = id,
        .name = name,
        .desc = desc,
        .gset = &@field(@This(), "gset_" ++ ces),
        .gset_ext = null,
        .conv_from = @field(T, "wc_conv_from_" ++ ces),
        .push_to = @field(T, "wc_push_to_" ++ ces),
        .char_conv = @field(T, "wc_char_conv_from_" ++ ces),
    };
}

const wc_push_to_raw = c.wc_push_to_raw;
const wc_push_to_hz = c.wc_push_to_hz;
const wc_push_to_priv1 = c.wc_push_to_iso8859;
const wc_push_to_cp1258 = c.wc_push_to_viet;
const wc_push_to_tcvn5712 = c.wc_push_to_viet;
const wc_push_to_viscii11 = c.wc_push_to_viet;
const wc_push_to_vps = c.wc_push_to_viet;
const wc_conv_from_cp1258 = c.wc_conv_from_priv1;
const wc_conv_from_tcvn5712 = c.wc_conv_from_viet;
const wc_conv_from_viscii11 = c.wc_conv_from_viet;
const wc_conv_from_vps = c.wc_conv_from_viet;
const wc_conv_from_raw = c.wc_conv_from_priv1;
const wc_char_conv_from_hz = c.wc_char_conv_from_iso2022;
const wc_char_conv_from_cp1258 = c.wc_char_conv_from_priv1;
const wc_char_conv_from_tcvn5712 = c.wc_char_conv_from_viet;
const wc_char_conv_from_viscii11 = c.wc_char_conv_from_viet;
const wc_char_conv_from_vps = c.wc_char_conv_from_viet;
const wc_char_conv_from_raw = c.wc_char_conv_from_priv1;
const wc_conv_from_hz = c.wc_conv_from_hz;

export const WcCesInfo = [_]c.wc_ces_info{
    ces_ascii(c.WC_CES_US_ASCII, "US-ASCII", "Latin (US-ASCII)"),

    ces_iso8859(c.WC_CES_ISO_8859_1, "ISO-8859-1", "Latin 1 (ISO-8859-1)", "1"),
    ces_iso8859(c.WC_CES_ISO_8859_2, "ISO-8859-2", "Latin 2 (ISO-8859-2)", "2"),
    ces_iso8859(c.WC_CES_ISO_8859_3, "ISO-8859-3", "Latin 3 (ISO-8859-3)", "3"),
    ces_iso8859(c.WC_CES_ISO_8859_4, "ISO-8859-4", "Latin 4 (ISO-8859-4)", "4"),
    ces_iso8859(c.WC_CES_ISO_8859_5, "ISO-8859-5", "Cyrillic (ISO-8859-5)", "5"),
    ces_iso8859(c.WC_CES_ISO_8859_6, "ISO-8859-6", "Arabic (ISO-8859-6)", "6"),
    ces_iso8859(c.WC_CES_ISO_8859_7, "ISO-8859-7", "Greek (ISO-8859-7)", "7"),
    ces_iso8859(c.WC_CES_ISO_8859_8, "ISO-8859-8", "Hebrew (ISO-8859-8)", "8"),
    ces_iso8859(c.WC_CES_ISO_8859_9, "ISO-8859-9", "Turkish (ISO-8859-9)", "9"),
    ces_iso8859(c.WC_CES_ISO_8859_10, "ISO-8859-10", "Nordic (ISO-8859-10)", "10"),
    ces_iso8859(c.WC_CES_ISO_8859_11, "ISO-8859-11", "Thai (ISO-8859-11, TIS-620)", "11"),
    .{ .id = c.WC_CES_ISO_8859_12 },
    ces_iso8859(c.WC_CES_ISO_8859_13, "ISO-8859-13", "Baltic Rim (ISO-8859-13)", "13"),
    ces_iso8859(c.WC_CES_ISO_8859_14, "ISO-8859-14", "Celtic (ISO-8859-14)", "14"),
    ces_iso8859(c.WC_CES_ISO_8859_15, "ISO-8859-15", "Latin 9 (ISO-8859-15)", "15"),
    ces_iso8859(c.WC_CES_ISO_8859_16, "ISO-8859-16", "Romanian (ISO-8859-16)", "16"),

    ces_iso2022(c.WC_CES_ISO_2022_JP, "ISO-2022-JP", "Japanese (ISO-2022-JP)", "jp"),
    ces_iso2022(c.WC_CES_ISO_2022_JP_2, "ISO-2022-JP-2", "Japanese (ISO-2022-JP-2)", "jp2"),
    ces_iso2022(c.WC_CES_ISO_2022_JP_3, "ISO-2022-JP-3", "Japanese (ISO-2022-JP-3)", "jp3"),
    ces_iso2022(c.WC_CES_ISO_2022_CN, "ISO-2022-CN", "Chinese (ISO-2022-CN)", "cn"),
    ces_iso2022(c.WC_CES_ISO_2022_KR, "ISO-2022-KR", "Korean (ISO-2022-KR)", "kr"),

    ces_euc(c.WC_CES_EUC_JP, "EUC-JP", "Japanese (EUC-JP)", "jp"),
    ces_euc(c.WC_CES_EUC_CN, "EUC-CN", "Chinese (EUC-CN, GB2312)", "cn"),
    ces_euc(c.WC_CES_EUC_TW, "EUC-TW", "Chinese Taiwan (EUC-TW)", "tw"),
    ces_euc(c.WC_CES_EUC_KR, "EUC-KR", "Korean (EUC-KR)", "kr"),

    ces_priv1(c.WC_CES_CP437, "CP437", "Latin (CP437)", &gset_cp("437")),
    ces_priv1(c.WC_CES_CP737, "CP737", "Greek (CP737)", &gset_cp("737")),
    ces_priv1(c.WC_CES_CP775, "CP775", "Baltic Rim (CP775)", &gset_cp("775")),
    ces_priv1(c.WC_CES_CP850, "CP850", "Latin 1 (CP850)", &gset_cp("850")),
    ces_priv1(c.WC_CES_CP852, "CP852", "Latin 2 (CP852)", &gset_cp("852")),
    ces_priv1(c.WC_CES_CP855, "CP855", "Cyrillic (CP855)", &gset_cp("855")),
    ces_priv1(c.WC_CES_CP856, "CP856", "Hebrew (CP856)", &gset_cp("856")),
    ces_priv1(c.WC_CES_CP857, "CP857", "Turkish (CP857)", &gset_cp("857")),
    ces_priv1(c.WC_CES_CP860, "CP860", "Portuguese (CP860)", &gset_cp("860")),
    ces_priv1(c.WC_CES_CP861, "CP861", "Icelandic (CP861)", &gset_cp("861")),
    ces_priv1(c.WC_CES_CP862, "CP862", "Hebrew (CP862)", &gset_cp("862")),
    ces_priv1(c.WC_CES_CP863, "CP863", "Canada French (CP863)", &gset_cp("863")),
    ces_priv1(c.WC_CES_CP864, "CP864", "Arabic (CP864)", &gset_cp("864")),
    ces_priv1(c.WC_CES_CP865, "CP865", "Nordic (CP865)", &gset_cp("865")),
    ces_priv1(c.WC_CES_CP866, "CP866", "Cyrillic (CP866)", &gset_cp("866")),
    ces_priv1(c.WC_CES_CP869, "CP869", "Greek 2 (CP869)", &gset_cp("869")),
    ces_priv1(c.WC_CES_CP874, "CP874", "Thai (CP874)", &gset_cp("874")),
    ces_priv1(c.WC_CES_CP1006, "CP1006", "Arabic (CP1006)", &gset_cp("1006")),
    ces_priv1(c.WC_CES_CP1250, "CP1250", "Latin 2 (CP1250)", &gset_cp("1250")),
    ces_priv1(c.WC_CES_CP1251, "CP1251", "Cyrillic (CP1251)", &gset_cp("1251")),
    ces_priv1(c.WC_CES_CP1252, "CP1252", "Latin 1 (CP1252)", &gset_cp("1252")),
    ces_priv1(c.WC_CES_CP1253, "CP1253", "Greek (CP1253)", &gset_cp("1253")),
    ces_priv1(c.WC_CES_CP1254, "CP1254", "Turkish (CP1254)", &gset_cp("1254")),
    ces_priv1(c.WC_CES_CP1255, "CP1255", "Hebrew (CP1255)", &gset_cp("1255")),
    ces_priv1(c.WC_CES_CP1256, "CP1256", "Arabic (CP1256)", &gset_cp("1256")),
    ces_priv1(c.WC_CES_CP1257, "CP1257", "Baltic Rim (CP1257)", &gset_cp("1257")),
    ces_priv1(c.WC_CES_KOI8_R, "KOI8-R", "Cyrillic (KOI8-R)", &gset_priv1("KOI8_R")),
    ces_priv1(c.WC_CES_KOI8_U, "KOI8-U", "Ukrainian (KOI8-U)", &gset_priv1("KOI8_U")),
    ces_priv1(c.WC_CES_NEXTSTEP, "NeXTSTEP", "NeXTSTEP", &gset_priv1("NEXTSTEP")),

    ces_priv2(c.WC_CES_RAW, "Raw", "8bit Raw", @This(), "raw"),

    ces_priv2(c.WC_CES_SHIFT_JIS, "Shift_JIS", "Japanese (Shift_JIS, CP932)", c, "sjis"),
    ces_priv2(c.WC_CES_SHIFT_JISX0213, "Shift_JISX0213", "Japanese (Shift_JISX0213)", c, "sjisx0213"),
    ces_priv2(c.WC_CES_GBK, "GBK", "Chinese (GBK, CP936)", c, "gbk"),
    ces_priv2(c.WC_CES_GB18030, "GB18030", "Chinese (GB18030)", c, "gb18030"),
    ces_priv2(c.WC_CES_HZ_GB_2312, "HZ-GB-2312", "Chinese (HZ-GB-2312)", @This(), "hz"),
    ces_priv2(c.WC_CES_BIG5, "Big5", "Chinese Taiwan (Big5, CP950)", c, "big5"),
    ces_priv2(c.WC_CES_HKSCS, "HKSCS", "Chinese Hong Kong (HKSCS)", c, "hkscs"),
    ces_priv2(c.WC_CES_UHC, "UHC", "Korean (UHC, CP949)", c, "uhc"),
    ces_priv2(c.WC_CES_JOHAB, "Johab", "Korean (Johab)", c, "johab"),

    ces_priv2(c.WC_CES_CP1258, "CP1258", "Vietnamese (CP1258)", @This(), "cp1258"),
    ces_priv2(c.WC_CES_TCVN_5712, "TCVN-5712", "Vietnamese (TCVN-5712)", @This(), "tcvn5712"),
    ces_priv2(c.WC_CES_VISCII_11, "VISCII-1.1", "Vietnamese (VISCII 1.1)", @This(), "viscii11"),
    ces_priv2(c.WC_CES_VPS, "VPS", "Vietnamese (VPS)", @This(), "vps"),

    ces_priv2(c.WC_CES_UTF_8, "UTF-8", "Unicode (UTF-8)", c, "utf8"),
    ces_priv2(c.WC_CES_UTF_7, "UTF-7", "Unicode (UTF-7)", c, "utf7"),
    .{},
};
