#include "line.h"

int Line::bytePosToColumn(int pos) const {
  return ::bytePosToColumn(lineBuf, propBuf, len, pos, 0, false);
}
