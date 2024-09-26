#include "rand48_win32.h"
#include <stdlib.h>

// https://stackoverflow.com/questions/74274179/i-cant-use-drand48-and-srand48-in-c
double drand48() { return rand() / (RAND_MAX + 1.0); }
long int lrand48() { return rand(); }
long int mrand48() { return rand() > RAND_MAX / 2 ? rand() : -rand(); }
void srand48(long int seedval) { srand(seedval); }
