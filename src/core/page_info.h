#pragma once
#include "geometry.h"
#include <Str.h>

struct Content;
struct Document;

/*
 * information of current page and link
 */
Str page_info_panel_html(struct Content *content, struct Document* doc, struct BufferPoint bp);
