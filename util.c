#include "util.h"
#include "w3m_tty.h"
#include "config.h"
#include "display.h"

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
	tty_getch();

	return rv;
    }
    fmInit();

    return 0;
}

#if defined(USE_M17N) && defined(USE_UNICODE)
#include "libwc/ucs.h"
#include "libwc/wtf.h"

uint32_t
getChar(struct wc_option *WcOption, const char *p)
{
    return wc_any_to_ucs(WcOption, wtf_parse1((const uint8_t **)&p));
}

int
is_wordchar(uint32_t c)
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
