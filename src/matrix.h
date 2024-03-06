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

  double M_VAL(int i, int j) const { return (this->me[(i) * this->dim + (j)]); }
  double &M_VAL(int i, int j) { return (this->me[(i) * this->dim + (j)]); }
  double m_entry(int i, int j) { return M_VAL(i, j); }
  void m_add_val(int i, int j, double x) { this->M_VAL(i, j) += (x); }
  void m_set_val(int i, int j, double x) { this->M_VAL(i, j) = (x); }

  void copy_to(Matrix *m2) {
    assert(dim == m2->dim);
    memcpy((m2)->me.data(), (this)->me.data(),
           (this)->dim * (this)->dim * sizeof(double));
  }

  int LUfactor(int *);
  int LUsolve(int *, struct vector *, vector *);
  Matrix LUinverse(int *);
  int Lsolve(vector *, vector *, double);
  int Usolve(vector *, vector *, double);
};

struct vector {
  double *ve;
  int dim;
};

#define V_VAL(v, i) ((v)->ve[i])

/*
 * Compatible macros with those in Meschach Library.
 */

#define v_entry(v, i) (V_VAL(v, i))
#define v_free(v) ((v) = NULL)
#define m_free(m) ((m) = NULL)
#define px_free(px) ((px) = NULL)
#define v_set_val(v, i, x) (V_VAL(v, i) = (x))
#define v_add_val(v, i, x) (V_VAL(v, i) += (x))
#define v_get(n) (new_vector(n))
#define px_get(n) (New_N(int, n))
typedef int PERM;
extern vector *new_vector(int);
