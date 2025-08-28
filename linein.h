#ifndef W3M_LINEIN_H_
#define W3M_LINEIN_H_

#include "Str.h"
#include "fm.h"
#include "history.h"

#include <stddef.h>

#define inputLineHist(p,d,f,h)	inputLineHistSearch(p,d,f,h,NULL)
#define inputLine(p,d,f)	inputLineHist(p,d,f,NULL)
#define inputStr(p,d)		inputLine(p,d,IN_STRING)
#define inputStrHist(p,d,h)	inputLineHist(p,d,IN_STRING,h)
#define inputFilename(p,d)	inputLine(p,d,IN_FILENAME)
#define inputFilenameHist(p,d,h)	inputLineHist(p,d,IN_FILENAME,h)
#define inputChar(p)		inputLine(p,"",IN_CHAR)

extern char *inputLineHistSearch(const char *prompt, const char *def_str,
				 int flag, Hist *hist,
				 int (*incfunc) (int ch, Str buf,
						 Lineprop *prop));

#endif
