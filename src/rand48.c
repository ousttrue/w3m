#include "rand48.h"
#include <stdlib.h>

// #ifdef USE_INCLUDED_SRAND48
static unsigned long R1 = 0x1234abcd;
static unsigned long R2 = 0x330e;
#define A1 0x5deec
#define A2 0xe66d
#define C 0xb

void srand48(long seed) {
  R1 = (unsigned long)seed;
  R2 = 0x330e;
}

long lrand48(void) {
  R1 = (A1 * R1 << 16) + A1 * R2 + A2 * R1 + ((A2 * R2 + C) >> 16);
  R2 = (A2 * R2 + C) & 0xffff;
  return (long)(R1 >> 1);
}
// #endif

// https://stackoverflow.com/questions/74274179/i-cant-use-drand48-and-srand48-in-c
double drand48() { return lrand48() / (RAND_MAX + 1.0); }
// long int lrand48() { return rand(); }
long int mrand48() { return lrand48() > RAND_MAX / 2 ? rand() : -rand(); }
// void srand48(long int seedval) { srand(seedval); }
