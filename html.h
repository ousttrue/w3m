/* $Id: html.h,v 1.31 2010/08/14 01:29:40 htrb Exp $ */
#ifndef _HTML_H
#define _HTML_H
#include "config.h"
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>

#include <time.h>

struct cmdtable {
    char* cmdname;
    int cmd;
};

#define CMP_NOCOMPRESS 0
#define CMP_COMPRESS 1
#define CMP_GZIP 2
#define CMP_BZIP2 3
#define CMP_DEFLATE 4
#define CMP_BROTLI 5

#define ENC_7BIT 0
#define ENC_BASE64 1
#define ENC_QUOTE 2
#define ENC_UUENCODE 3

struct environment {
    unsigned char env;
    int type;
    int count;
    char indent;
};

#define MAX_ENV_LEVEL 20
#define MAX_INDENT_LEVEL 10

#define INDENT_INCR IndentIncr

#endif /* _HTML_H */
