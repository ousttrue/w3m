/* $Id: matrix.h,v 1.7 2002/07/18 14:59:02 ukai Exp $ */
/* 
 * matrix.h, matrix.c: Liner equation solver using LU decomposition.
 * 
 * by K.Okabe  Aug. 1999
 * 
 * You can use,copy,modify and distribute this program without any permission.
 */

#ifndef _MATRIX_H
#include <math.h>
#include <string.h>

/* 
 * Types.
 */

struct matrix {
    double *me;
    int dim;
};

struct vector {
    double *ve;
    int dim;
};

/* 
 * Macros.
 */

#define M_VAL(m,i,j) ((m)->me[(i)*(m)->dim +(j)])
#define V_VAL(v,i) ((v)->ve[i])

/* 
 * Compatible macros with those in Meschach Library.
 */

#define m_entry(m,i,j) (M_VAL(m,i,j))
#define v_entry(v,i) (V_VAL(v,i))
#define m_copy(m1,m2) (bcopy((m1)->me,(m2)->me,(m1)->dim*(m1)->dim*sizeof(double)))
#define v_free(v) ((v)=NULL)
#define m_free(m) ((m)=NULL)
#define px_free(px) ((px)=NULL)
#define m_set_val(m,i,j,x) (M_VAL(m,i,j) = (x))
#define m_add_val(m,i,j,x) (M_VAL(m,i,j) += (x))
#define v_set_val(v,i,x) (V_VAL(v,i) = (x))
#define v_add_val(v,i,x) (V_VAL(v,i) += (x))
#define m_get(r,c) (new_matrix(r))
#define v_get(n) (new_vector(n))
#define px_get(n) (New_N(int,n))
typedef int PERM;

extern int LUfactor(matrix*, int *);
extern matrix* m_inverse(matrix*, matrix*);
extern matrix* LUinverse(matrix*, int *, matrix*);
extern int LUsolve(matrix*, int *, vector*, vector*);
extern int Lsolve(matrix*, vector*, vector*, double);
extern int Usolve(matrix*, vector*, vector*, double);
extern matrix* new_matrix(int);
extern vector* new_vector(int);

#define _MATRIX_H
#endif				/* _MATRIX_H */
