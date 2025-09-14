#pragma once
#include "geometry.h"
#include "Content.h"

struct Buffer;

/*
 * information of current page and link
 */
struct Content page_info_panel(struct UI ui, struct Buffer* buf);
