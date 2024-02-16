#pragma once

struct Str;
struct FormList;
struct FormItemList {
  int type;
  Str *name;
  Str *value;
  Str *init_value;
  int checked, init_checked;
  int accept;
  int size;
  int rows;
  int maxlength;
  int readonly;
  FormList *parent;
  FormItemList *next;

  Str *query_from_followform();
  void query_from_followform_multipart();
};
