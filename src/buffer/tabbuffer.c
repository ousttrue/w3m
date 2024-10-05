#include "tabbuffer.h"
#include "buffer.h"

#include "document.h"
void SAVE_BUFPOSITION(struct Document *sbufp) {
  COPY_BUFPOSITION(sbufp, &Currentbuf->document);
}

void RESTORE_BUFPOSITION(struct Document *sbufp) {
  COPY_BUFPOSITION(&Currentbuf->document, sbufp);
}
