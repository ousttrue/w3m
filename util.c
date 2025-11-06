/* vi: set sw=4 ts=8 ai sm noet : */
#include "util.h"

#include "config.h"
#include "display.h"
#include "myctype.h"
#include "terms.h"

#include <stdio.h>
#include <stdlib.h>

int
exec_cmd(char *cmd)
{
    int rv;

    fmTerm();
    if ((rv = system(cmd))) {
	printf("\n[Hit any key]");
	fflush(stdout);
	fmInit();
	getch();

	return rv;
    }
    fmInit();

    return 0;
}

#if defined(USE_M17N) && defined(USE_UNICODE)
#include "ucs.h"
#include "wtf.h"

wc_uint32
getChar(const char *p)
{
    return wc_any_to_ucs(wtf_parse1((const wc_uchar **)&p));
}

int
is_wordchar(wc_uint32 c)
{
    return wc_is_ucs_alnum(c);
}
#else		/* USE_M17N && USE_UNICODE */
int
is_wordchar(int c)
{
    return IS_ALNUM(c);
}
#endif		/* USE_M17N && USE_UNICODE */
