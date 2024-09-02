
#include "wc.h"
#include "jis.h"
#include "search.h"

#include "map/jisx0201k_jisx0208.map"
#include "map/jisx0208_jisx02131.map"

wc_wchar_t wc_jisx0201k_to_jisx0208(wc_wchar_t cc) {
  cc.code = jisx0201k_jisx0208_map[cc.code & 0x7f];
  cc.ccs = cc.code ? WC_CCS_JIS_X_0208 : WC_CCS_UNKNOWN_W;
  return cc;
}

wc_wchar_t wc_jisx0212_to_jisx0213(wc_wchar_t cc) {
  cc.ccs = WC_CCS_UNKNOWN_W;
  return cc;
}

wc_wchar_t wc_jisx0213_to_jisx0212(wc_wchar_t cc) {
  cc.ccs = WC_CCS_UNKNOWN_W;
  return cc;
}

wc_ccs wc_jisx0208_or_jisx02131(wc_uint16 code) {
  return wc_map_range_search(code & 0x7f7f, jisx0208_jisx02131_map,
                             N_jisx0208_jisx02131_map)
             ? WC_CCS_JIS_X_0213_1
             : WC_CCS_JIS_X_0208;
}

wc_ccs wc_jisx0212_or_jisx02132(wc_uint16 code) {
  return wc_jisx0212_jisx02132_map[(code >> 8) & 0x7f] ? WC_CCS_JIS_X_0213_2
                                                       : WC_CCS_JIS_X_0212;
}
