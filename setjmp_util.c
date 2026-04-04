#include "setjmp_util.h"

sigjmp_buf AbortLoading;

MySignalHandler KeyAbort(SIGNAL_ARG)
{
    LONGJMP(AbortLoading, 1);
    SIGNAL_RETURN;
}
