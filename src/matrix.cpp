/*  $Id: matrix.c,v 1.8 2003/04/07 16:27:10 ukai Exp $ */
/*
 * matrix.h, matrix.c: Liner equation solver using LU decomposition.
 *
 * by K.Okabe  Aug. 1999
 *
 * LUfactor, LUsolve, Usolve and Lsolve, are based on the functions in
 * Meschach Library Version 1.2b.
 */

/**************************************************************************
**
** Copyright (C) 1993 David E. Steward & Zbigniew Leyk, all rights reserved.
**
**			     Meschach Library
**
** This Meschach Library is provided "as is" without any express
** or implied warranty of any kind with respect to this software.
** In particular the authors shall not be liable for any direct,
** indirect, special, incidental or consequential damages arising
** in any way from use of the software.
**
** Everyone is granted permission to copy, modify and redistribute this
** Meschach Library, provided:
**  1.  All copies contain this copyright notice.
**  2.  All modified copies shall carry a notice stating who
**      made the last modification and the date of such modification.
**  3.  No charge is made for this software or works derived from it.
**      This clause shall not be construed as constraining other software
**      distributed on the same medium as this software, nor is a
**      distribution fee considered a charge.
**
***************************************************************************/

#define HAVE_FLOAT_H 1
#include "matrix.h"
#include "alloc.h"

/*
 * Macros from "fm.h".
 */

#define SWAPD(a, b)                                                            \
  {                                                                            \
    double tmp = a;                                                            \
    a = b;                                                                     \
    b = tmp;                                                                   \
  }
#define SWAPI(a, b)                                                            \
  {                                                                            \
    int tmp = a;                                                               \
    a = b;                                                                     \
    b = tmp;                                                                   \
  }

/*
 * LUfactor -- gaussian elimination with scaled partial pivoting
 *          -- Note: returns LU matrix which is A.
 */

int Matrix::LUfactor(int *indexarray) {
  int dim = this->dim, i, j, k, i_max, k_max;

  Vector scale(dim);

  for (i = 0; i < dim; i++)
    indexarray[i] = i;

  double mx, tmp;
  for (i = 0; i < dim; i++) {
    mx = 0.;
    for (j = 0; j < dim; j++) {
      tmp = fabs(this->M_VAL(i, j));
      if (mx < tmp)
        mx = tmp;
    }
    scale.ve[i] = mx;
  }

  k_max = dim - 1;
  for (k = 0; k < k_max; k++) {
    mx = 0.;
    i_max = -1;
    for (i = k; i < dim; i++) {
      if (fabs(scale.ve[i]) >= Tiny * fabs(this->M_VAL(i, k))) {
        tmp = fabs(this->M_VAL(i, k)) / scale.ve[i];
        if (mx < tmp) {
          mx = tmp;
          i_max = i;
        }
      }
    }
    if (i_max == -1) {
      this->M_VAL(k, k) = 0.;
      continue;
    }

    if (i_max != k) {
      SWAPI(indexarray[i_max], indexarray[k]);
      for (j = 0; j < dim; j++)
        SWAPD(this->M_VAL(i_max, j), this->M_VAL(k, j));
    }

    for (i = k + 1; i < dim; i++) {
      tmp = this->M_VAL(i, k) = this->M_VAL(i, k) / this->M_VAL(k, k);
      for (j = k + 1; j < dim; j++)
        this->M_VAL(i, j) -= tmp * this->M_VAL(k, j);
    }
  }
  return 0;
}

/*
 * LUsolve -- given an LU factorisation in A, solve Ax=b.
 */

int Matrix::LUsolve(int *indexarray, Vector *b, Vector *x) {
  int i, dim = this->dim;

  for (i = 0; i < dim; i++)
    x->ve[i] = b->ve[indexarray[i]];

  if (this->Lsolve(x, x, 1.) == -1 || this->Usolve(x, x, 0.) == -1)
    return -1;
  return 0;
}

/* m_inverse -- returns inverse of A, provided A is not too rank deficient
 *           -- uses LU factorisation */

Matrix Matrix::LUinverse(int *indexarray) {
  Matrix out(dim);
  Vector tmp(dim);
  Vector tmp2(dim);
  for (int i = 0; i < dim; i++) {
    for (int j = 0; j < dim; j++)
      tmp.ve[j] = 0.;
    tmp.ve[i] = 1.;
    if (this->LUsolve(indexarray, &tmp, &tmp2) == -1)
      return NULL;
    for (int j = 0; j < dim; j++)
      out.M_VAL(j, i) = tmp2.ve[j];
  }
  return out;
}

/*
 * Usolve -- back substitution with optional over-riding diagonal
 *        -- can be in-situ but doesn't need to be.
 */

int Matrix::Usolve(Vector *b, Vector *out, double diag) {
  int i, j, i_lim, dim = this->dim;
  double sum;

  for (i = dim - 1; i >= 0; i--) {
    if (b->ve[i] != 0.)
      break;
    else
      out->ve[i] = 0.;
  }
  i_lim = i;

  for (; i >= 0; i--) {
    sum = b->ve[i];
    for (j = i + 1; j <= i_lim; j++)
      sum -= this->M_VAL(i, j) * out->ve[j];
    if (diag == 0.) {
      if (fabs(this->M_VAL(i, i)) <= Tiny * fabs(sum))
        return -1;
      else
        out->ve[i] = sum / this->M_VAL(i, i);
    } else
      out->ve[i] = sum / diag;
  }

  return 0;
}

/*
 * Lsolve -- forward elimination with (optional) default diagonal value.
 */

int Matrix::Lsolve(Vector *b, Vector *out, double diag) {
  int i, j, i_lim, dim = this->dim;
  double sum;

  for (i = 0; i < dim; i++) {
    if (b->ve[i] != 0.)
      break;
    else
      out->ve[i] = 0.;
  }
  i_lim = i;

  for (; i < dim; i++) {
    sum = b->ve[i];
    for (j = i_lim; j < i; j++)
      sum -= this->M_VAL(i, j) * out->ve[j];
    if (diag == 0.) {
      if (fabs(this->M_VAL(i, i)) <= Tiny * fabs(sum))
        return -1;
      else
        out->ve[i] = sum / this->M_VAL(i, i);
    } else
      out->ve[i] = sum / diag;
  }

  return 0;
}
