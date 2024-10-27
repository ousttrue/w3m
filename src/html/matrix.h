/*
 * matrix.h, matrix.c: Liner equation solver using LU decomposition.
 *
 * by K.Okabe  Aug. 1999
 *
 * You can use,copy,modify and distribute this program without any permission.
 */

#pragma once

struct matrix {
  double *me;
  int dim;
};

struct vector {
  double *ve;
  int dim;
};
