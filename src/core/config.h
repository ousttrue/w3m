#pragma once

#define SETPGRP_VOID 1
#ifdef SETPGRP_VOID
#define SETPGRP() setpgrp()
#else
#define SETPGRP() setpgrp(0, 0)
#endif
#define HAVE_FLOAT_H 1
#define HAVE_SYS_SELECT_H 1

#define HAVE_SIGSETJMP 1

#define PATH_SEPARATOR ':'
