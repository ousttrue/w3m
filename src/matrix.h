/*
 * matrix.h, matrix.c: Liner equation solver using LU decomposition.
 *
 * by K.Okabe  Aug. 1999
 *
 * You can use,copy,modify and distribute this program without any permission.
 */
#pragma once
#include <string.h>
#include <vector>
#include <assert.h>

#ifdef HAVE_FLOAT_H
#include <float.h>
#endif /* not HAVE_FLOAT_H */
#if defined(DBL_MAX)
inline double Tiny = 10.0 / DBL_MAX;
#elif defined(FLT_MAX)
inline double Tiny = 10.0 / FLT_MAX;
#else  /* not defined(FLT_MAX) */
inline double Tiny = 1.0e-30;
#endif /* not defined(FLT_MAX */

struct Matrix {
  std::vector<double> me;
  int dim;

  Matrix() : dim(0) {}
  Matrix(int d) : dim(d) { me.resize(d * d); }
  double M_VAL(int i, int j) const { return me[i * dim + j]; }
  double &M_VAL(int i, int j) { return me[i * dim + j]; }
  double m_entry(int i, int j) { return M_VAL(i, j); }
  void m_add_val(int i, int j, double x) { M_VAL(i, j) += x; }
  void m_set_val(int i, int j, double x) { M_VAL(i, j) = x; }
  void copy_to(Matrix *m2) {
    assert(dim == m2->dim);
    memcpy((m2)->me.data(), me.data(), dim * dim * sizeof(double));
  }
  int LUfactor(int *);
  int LUsolve(int *, struct Vector *, Vector *);
  Matrix LUinverse(int *);
  int Lsolve(Vector *, Vector *, double);
  int Usolve(Vector *, Vector *, double);
};

struct Vector {
  std::vector<double> ve;
  int dim;

  Vector() : dim(0) {}
  Vector(int d) : dim(d) { ve.resize(d); }
  double V_VAL(int i) const { return ve[i]; }
  double &V_VAL(int i) { return ve[i]; }
  double v_entry(int i) const { return (V_VAL(i)); }
  void v_set_val(int i, double x) { (V_VAL(i) = (x)); }
  void v_add_val(int i, double x) { (V_VAL(i) += (x)); }
};
