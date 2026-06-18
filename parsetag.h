/* vi: set sw=4 ts=8 ai sm noet : */
#ifndef W3M_PARSETAG_H
#define W3M_PARSETAG_H

struct parsed_tagarg {
    char *arg;
    char *value;
    struct parsed_tagarg *next;
};

extern char *tag_get_value(struct parsed_tagarg *t, const char *arg);
extern struct parsed_tagarg *cgistr2tagarg(char *cgistr);

#endif
