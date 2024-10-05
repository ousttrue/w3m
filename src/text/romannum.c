#include "romannum.h"

static char roman_num1[] = {
    'i', 'x', 'c', 'm', '*',
};
static char roman_num5[] = {
    'v',
    'l',
    'd',
    '*',
};

static Str romanNum2(int l, int n) {
  Str s = Strnew();

  switch (n) {
  case 1:
  case 2:
  case 3:
    for (; n > 0; n--)
      Strcat_char(s, roman_num1[l]);
    break;
  case 4:
    Strcat_char(s, roman_num1[l]);
    Strcat_char(s, roman_num5[l]);
    break;
  case 5:
  case 6:
  case 7:
  case 8:
    Strcat_char(s, roman_num5[l]);
    for (n -= 5; n > 0; n--)
      Strcat_char(s, roman_num1[l]);
    break;
  case 9:
    Strcat_char(s, roman_num1[l]);
    Strcat_char(s, roman_num1[l + 1]);
    break;
  }
  return s;
}

Str romanNumeral(int n) {
  Str r = Strnew();

  if (n <= 0)
    return r;
  if (n >= 4000) {
    Strcat_charp(r, "**");
    return r;
  }
  Strcat(r, romanNum2(3, n / 1000));
  Strcat(r, romanNum2(2, (n % 1000) / 100));
  Strcat(r, romanNum2(1, (n % 100) / 10));
  Strcat(r, romanNum2(0, n % 10));

  return r;
}

Str romanAlphabet(int n) {
  Str r = Strnew();
  int l;
  char buf[14];

  if (n <= 0)
    return r;

  l = 0;
  while (n) {
    buf[l++] = 'a' + (n - 1) % 26;
    n = (n - 1) / 26;
  }
  l--;
  for (; l >= 0; l--)
    Strcat_char(r, buf[l]);

  return r;
}
