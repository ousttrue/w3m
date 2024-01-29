#pragma once

struct MapArea {
  char *url;
  char *target;
  const char *alt;
};

struct GeneralList;
struct Str;
struct MapList {
  Str *name;
  GeneralList *area;
  MapList *next;
};

struct MapArea;
extern MapArea *newMapArea(char *url, char *target, const char *alt,
                           char *shape, char *coords);
