#pragma once
#include "geometry.h"

struct Content;
struct Document;

/*
 * information of current page and link
 */
struct Content page_info_panel(struct Content *content, struct Document* doc, struct BufferPoint bp);
