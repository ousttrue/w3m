#include "line.h"
#include "readbuffer.h"
#include <vector>



int Line::bytePosToColumn(int pos) const {
  return ::bytePosToColumn(lineBuf, propBuf, len, pos, 0, false);
}
