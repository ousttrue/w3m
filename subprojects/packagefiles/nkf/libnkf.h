#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned char *nkf_convert(unsigned char *str, int strlen, char *opts,
                                  int optslen);

extern const char *nkf_guess(unsigned char *str, int strlen);

#ifdef __cplusplus
} /* extern "C" */
#endif
