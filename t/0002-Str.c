#if 0
#!/bin/sh
test ${PWD##*/} = "t" || cd t || exit 2
make -C .. libwc/libwc.a myctype.o >/dev/null
cc -Werror -g -O0 ${0##*/} ../myctype.o \
	   -I.. -I../libwc -I/usr/local/include \
	   -L.. -L../libwc -L/usr/local/lib -lwc -lgc ||
	exit $?
./a.out "$@" || exit $?
rm -f ./a.out
exit
#endif

#include "../Str.c"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _strlen(x) (int)strlen(x)

#define ASSERT_int(x,y) \
{\
	if (x != y) {\
		printf("%d != %d\n", x, y);\
		assert(x == y);\
	}\
}

#define ASSERT_charp(x,y)\
{\
	if (strcmp(x,y)) {\
		printf("%s != %s\n", x, y);\
		assert(0);\
	}\
}

static Str t;
static char *char10 = "0123456789";

void
test_Strnew(void)
{
	Str s;
	s = Strnew();

	assert(s);
	assert(s->ptr);
	assert(s->length == 0);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	assert(s->ptr[0] == '\0');
}

void
test_Strnew_size()
{
	Str s;
	int n = 2 * INITIAL_STR_SIZE;

	s = Strnew_size(n);

	assert(s);
	assert(s->ptr);
	assert(s->length == 0);
	ASSERT_int(s->area_size, n);
	assert(s->ptr[0] == '\0');
}

void
test_Strnew_charp()
{
	Str s;
	char *in = "0123456789012345678901234567890123456789";
	int len = _strlen(in);

	s = Strnew_charp(in);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->length, len);
	ASSERT_int(s->area_size, len + 1);
	ASSERT_charp(s->ptr, in);
}

void
test_Strnew_m_charp()
{
	Str s;
	char *in  = "0123456789";
	char *exp = "01234567890123456789";
	int len = _strlen(in);

	s = Strnew_m_charp(in, in, NULL);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->length, 2 * len);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
}


void
test_Strnew_charp_n()
{
	Str s;
	char *in  = "0123456789";
	char *exp = "01234";
	int n = 5;

	s = Strnew_charp_n(in, n);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->length, n);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
}


void
test_Strdup()
{
	Str s;

	s = Strdup(t);

	assert(s);
	assert(s->ptr);
	assert(s->length == t->length);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, t->ptr);
}

void
test_Strclear()
{
	Str s = Strnew();

	s = Strdup(t);
	Strclear(s);

	ASSERT_int(s->length, 0);
	assert(s->ptr[0] == '\0');
}

void
test_Strfree()
{
	return;
}

void
test_Strcopy()
{
	Str s = Strnew();

	Strcopy(s, t);

	assert(s);
	assert(s->ptr);
	assert(s->length == t->length);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, t->ptr);
}

void
test_Strcopy_charp()
{
	Str s = Strnew();

	Strcopy_charp(s, t->ptr);

	assert(s);
	assert(s->ptr);
	assert(s->length == t->length);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, t->ptr);
}

void
test_Strcopy_charp_n()
{
	Str s;
	char *in  = "0123456789";
	char *exp = "01234";
	int n = 5;

	s = Strnew_charp(in);
	Strcopy_charp_n(s, in, n);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
	ASSERT_int(s->length, _strlen(exp));
}

void
test_Strcat_charp_n()
{
	Str s;
	char *in  = "0123456789";
	char *exp = "012345678901234";
	int n = _strlen(exp) - _strlen(in);

	s = Strdup(t);
	Strcat_charp_n(s, in, n);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_int(s->length, _strlen(exp));
	ASSERT_charp(s->ptr, exp);
}

void
test_Strcat()
{
	Str s, t;
	char *in  = "0123456789";
	char *exp = "01234567890123456789";

	s = Strnew_charp(in), t = Strnew_charp(in);
	Strcat(s, t);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_int(s->length, _strlen(exp));
	ASSERT_charp(s->ptr, exp);
}

void
test_Strcat_charp()
{
	Str s;
	char *in  = "0123456789";
	char *exp = "01234567890123456789";

	s = Strnew_charp(in);
	Strcat_charp(s, in);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
	ASSERT_int(s->length, _strlen(exp));
}

void
test_Strcat_m_charp()
{
	Str s;
	char *in  = "0123456789";
	char *exp = "012345678901234567890123456789";

	s = Strnew_charp(in);
	Strcat_m_charp(s, in, in, NULL);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
	ASSERT_int(s->length, _strlen(exp));
}


void
test_Strgrow()
{
	Str s = Strnew();

	Strgrow(s);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->length, 0);
	ASSERT_int(s->area_size, 2 * INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, "");

	Strgrow(s);
	Strgrow(s);
	Strgrow(s);
	Strgrow(s);
	Strgrow(s);
	Strgrow(s);
	Strgrow(s);
	Strgrow(s);

	ASSERT_int(s->length, 0);
	ASSERT_int(s->area_size, 12288);
}


void
test_Strsubstr()
{
	Str s;
	char *in  = "0123456789";
	char *exp = "56789";
	int n = 5;

	s = Strsubstr(t, n, n);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
	ASSERT_int(s->length, n);
}


void
test_Strlower()
{
	Str s;
	char *in  = "ABCD";
	char *exp = "abcd";

	s = Strnew_charp(in);

	Strlower(s);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
	ASSERT_int(s->length, _strlen(exp));
}


void
test_Strupper()
{
	Str s;
	char *in  = "abcd";
	char *exp = "ABCD";

	s = Strnew_charp(in);
	Strupper(s);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
	ASSERT_int(s->length, _strlen(exp));
}


void
test_Strchop()
{
	Str s;
	char *in  = "\r12\r\n";
	char *exp = "\r12";

	s = Strnew_charp(in);
	Strchop(s);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
	ASSERT_int(s->length, _strlen(exp));
}

void
test_Strinsert_char()
{
	Str s;
	char c = '\t';
	char *in  = "0123456789";
	char *exp = "01234\t56789";
	int n = 5;

	s = Strnew_charp(in);
	Strinsert_char(s, n, c);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->length, _strlen(exp));
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
}

void
test_Strinsert_charp()
{
	Str s;
	char *c = "\t";
	char *in  = "0123456789";
	char *exp = "01234\t56789";
	int n = 5;

	s = Strnew_charp(in);
	Strinsert_charp(s, n, c);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->length, _strlen(exp));
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
}

void
test_Strdelete()
{
	Str s;
	char *in  = "0123456789";
	char *exp = "0123789";
	int l = 4, n = 3;

	s = Strnew_charp(in);
	Strdelete(s, l, n);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->length, _strlen(exp));
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
}

void
test_Strtruncate()
{
	Str s;
	char *in  = "0123456789";
	char *exp = "01234";
	int n = 5;

	s = Strnew_charp(in);
	Strtruncate(s, n);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->length, _strlen(exp));
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
}

void
test_Strshrink()
{
	Str s;
	char *in  = "0123456789";
	char *exp = "01234";
	int n = 5;

	s = Strnew_charp(in);
	Strshrink(s, n);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->length, _strlen(exp));
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
}

void
test_Strremovefirstspaces()
{
	Str s;
	char *in  = " \t\n0123456789";
	char *exp = "0123456789";

	s = Strnew_charp(in);
	Strremovefirstspaces(s);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
	ASSERT_int(s->length, _strlen(exp));
}

void
test_Strremovetrailingspaces()
{
	Str s;
	char *in  = "0123456789 \t\n";
	char *exp = "0123456789";

	Strremovetrailingspaces(s);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->length, _strlen(exp));
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_charp(s->ptr, exp);
}

void
test_Stralign_left()
{
	Str s;
	char *in  = "0123456789";
	char *exp = "0123456789     ";
	int n = _strlen(exp);

	s = Strnew_charp(in);
	s = Stralign_left(s, n);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_int(s->length, _strlen(exp));
	ASSERT_charp(s->ptr, exp);
}

void
test_Stralign_right()
{
	Str s;
	char *in  = "0123456789";
	char *exp = "     0123456789";
	int n = _strlen(exp);

	s = Strnew_charp(in);
	s = Stralign_right(t, n);

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_int(s->length, _strlen(exp));
	ASSERT_charp(s->ptr, exp);
}

void
test_Stralign_center()
{
	Str s;
	char *in  = "0123456789";
	char *exp = "  0123456789   ";
	int n = _strlen(exp);

	s = Strnew_charp(in);
	s = Stralign_center(s, _strlen(exp));

	assert(s);
	assert(s->ptr);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	ASSERT_int(s->length, _strlen(exp));
	ASSERT_charp(s->ptr, exp);
}

void
test_Sprintf()
{
	return;
	Str s = Strnew();
	int n;

	n = 10;
	Sprintf("%s\n", t->ptr);

	assert(s);
	assert(s->ptr);
	assert(s->length == t->length);
	ASSERT_int(s->area_size, INITIAL_STR_SIZE);
	assert(s->ptr[0] == '\0');
	assert(strcmp(s->ptr, t->ptr));
}

int
main(int argc, char *argv[])
{
	t = malloc(sizeof (struct _Str));
	t->ptr = char10;
	t->length = _strlen(t->ptr);
	t->area_size = t->length + 1;

	if (argc > 1)
		printf("Running %s\n", "0001-Str.c");

	test_Strnew();
	test_Strnew_size();
	test_Strnew_charp();
	test_Strnew_m_charp();
	test_Strnew_charp_n();
	test_Strdup();
	test_Strclear();
	test_Strfree();
	test_Strcopy();
	test_Strcopy_charp();
	test_Strcopy_charp_n();
	test_Strcat_charp_n();
	test_Strcat();
	test_Strcat_charp();
	test_Strcat_m_charp();
	test_Strgrow();
	test_Strsubstr();
	test_Strlower();
	test_Strupper();
	test_Strchop();
	test_Strinsert_char();
	test_Strinsert_charp();
	test_Strdelete();
	test_Strtruncate();
	test_Strshrink();
	test_Strremovefirstspaces();
	test_Strremovetrailingspaces();
	test_Stralign_left();
	test_Stralign_right();
	test_Stralign_center();
	test_Sprintf();
}
