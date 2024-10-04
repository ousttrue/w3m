//
// https://www.servernote.net/article.php?id=make-libnkf-for-c-cpp
//
/* Custom NKF Library for C/C++ from NKF.python3/NKF_python.c */
#ifndef __libnkf_h__
#define __libnkf_h__

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned char *nkf_convert(unsigned char *str, int strlen, char *opts,
                                  int optslen);

extern const char *nkf_guess(unsigned char *str, int strlen);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __libnkf_h__ */
