/*
    AHMED -- Another software library on Hierarchical Matrices for
             Elliptic Differential equations

    Copyright (c) 2012 Mario Bebendorf

    You should have received a copy of the license along with 
    this software; if not, see AHMED's internet site.
*/


//*****************************************************************
// Iterative template routine -- FGMRES
//
// GMRES solves the unsymmetric linear system Ax = b using the
// Flexible Generalized Minimum Residual method
//
// The return value indicates convergence within nsteps (input)
// iterations (0), or no convergence within nsteps iterations (1).
//
// Upon successful return, output arguments have the following values:
//
//      x  --  approximate solution to Ax = b
// nsteps  --  the number of iterations performed before the
//             tolerance was reached
//    eps  --  the residual after the final iteration
//
//*****************************************************************
#include <cmath>
#include "blas.h"
#include "matrix.h"

static void genPlRot(double dx, double dy, double& cs, double& sn)
{
  if (dy==0.0) {
    cs = 1.0;
    sn = 0.0;
  } else if (fabs(dy)>fabs(dx)) {
    double tmp = dx / dy;
    sn = 1.0 / sqrt(1.0+tmp*tmp);
    cs = tmp * sn;
  } else {
    double tmp = dy / dx;
    cs = 1.0 / sqrt(1.0+tmp*tmp);
    sn = tmp * cs;
  }
}


inline void applPlRot(double& dx, double& dy, double cs, double sn)
{
  double tmp = cs*dx + sn*dy;
  dy = cs*dy - sn*dx;
  dx = tmp;
}


// solve H y = s and update x += Zy
static void update(unsigned n, unsigned k, double* H,
                   unsigned ldH, double* s, double* Z, double* x)
{
  double *y = new double[k];
  blas::copy(k, s, y);
  int inf;

  dtrtrs_(JOB_STR+5, JOB_STR, JOB_STR, &k, &N_ONE, H, &ldH, y, &k, &inf);
  assert(inf==0);

  // x += Z y
  blas::gemva(n, k, D_ONE, Z, y, x);

  delete [] y;
}

unsigned FGMRes(const Matrix<double>& A, double* const b, double* const x,
                double& eps, const unsigned m, unsigned& nsteps)
{
  double resid;
  unsigned j = 1;

  double *r = new double[2*A.n+(2*A.n+m+3)*(m+1)];   // n
  double *V = r + A.n;                         // n x (m+1)
  double *Z = V + A.n*(m+1);                   // n x (m+1)
  double *H = Z + A.n*(m+1);                   // m+1 x m
  double *cs = H + (m+1)*m;                  // m+1
  double *sn = cs + m+1;                     // m+1
  double *s = sn + m+1;                      // m+1

  // normb = norm(b)
  double normb = blas::nrm2(A.n, b);
  if (normb == 0.0) {
    blas::setzero(A.n, x);
    eps = 0.0;
    nsteps = 0;
    return 0;
  }

  // r = b - Ax
  blas::copy(A.n, b, r);
  A.amux(D_MONE, x, r);

  double beta = blas::nrm2(A.n, r);

  if ((resid=beta/normb)<=eps) {
    eps = resid;
    nsteps = 0;
    delete [] r;
    return 0;
  }

  while (j<=nsteps) {
    blas::copy(A.n, r, V);                // v0 first orthonormal vector
    blas::scal(A.n, 1.0/beta, V);

    s[0] = beta;
    blas::setzero(m, s+1);

    for (unsigned i=0; i<m && j<=nsteps; i++, j++) {

      // z[i] = A M * v[i];
      blas::copy(A.n, V+i*A.n, Z+i*A.n);
      A.precond_apply(Z+i*A.n);

      blas::setzero(A.n, V+(i+1)*A.n);
      A.amux(D_ONE, Z+i*A.n, V+(i+1)*A.n);

      for (unsigned k=0; k<=i; k++) {
        H[k+i*(m+1)] = blas::scpr(A.n, V+(i+1)*A.n, V+k*A.n);
        blas::axpy(A.n, -H[k+i*(m+1)], V+k*A.n, V+(i+1)*A.n);
      }

      H[i*(m+2)+1] = blas::nrm2(A.n, V+(i+1)*A.n);
      blas::scal(A.n, 1.0/H[i*(m+2)+1], V+(i+1)*A.n);

      // apply old Givens rotations to the last column in H
      for (unsigned k=0; k<i; k++)
        applPlRot(H[k+i*(m+1)], H[k+1+i*(m+1)], cs[k], sn[k]);

      // generate new Givens rotation which eleminates H[i*(m+2)+1]
      genPlRot(H[i*(m+2)], H[i*(m+2)+1], cs[i], sn[i]);

      // apply it to H and s
      applPlRot(H[i*(m+2)], H[i*(m+2)+1], cs[i], sn[i]);
      applPlRot(s[i], s[i+1], cs[i], sn[i]);

      if ((resid=fabs(s[i+1]/normb)) < eps) {
        update(A.n, i+1, H, m+1, s, Z, x);
        eps = resid;
        nsteps = j;
        delete [] r;
        return 0;
      }
      //#ifndef NDEBUG
      std::cout << "Step " << j << ", resid=" << resid << std::endl;
      //#endif
    }

    update(A.n, m, H, m+1, s, Z, x);

    // r = b - A x;
    blas::copy(A.n, b, r);
    A.amux(D_MONE, x, r);
    beta = blas::nrm2(A.n, r);

    if ((resid=beta/normb) < eps) {
      eps = resid;
      nsteps = j;
      delete [] r;
      return 0;
    }
  }

  eps = resid;
  delete [] r;
  return 1;
}


// ----------------------------------------------------------------------------
// complex version

static void genPlRot(dcomp a, dcomp b, double &cs, dcomp &sn)
{
  double k1, k2;
  if (Re(b)==0 && Im(b)==0) {
    cs = 1.0;
    sn = dcomp(0., 0.);
  } else if (Re(a)==0 && Im(a) == 0) {
    cs = 0.0;
    sn = dcomp(1., 0.);
  } else {
    if (Re(a)>=Im(a)) {
      k1 = Re(a) * Re(b) - Im(a) * Im(b);
      k1 /= abs2(a);
      k2 = (k1 * Im(a) - Im(b)) / Re(a);
    } else {
      k1 = Re(a) * Re(b) + Im(a) * Im(b);
      k1 /= abs2(a);
      k2 = (Re(b) - k1 * Re(a)) / Im(a);
    }
    cs = 1.0 / sqrt(1 + k1*k1 + k2*k2);
    sn = dcomp(cs * k1, cs * k2);
  }
}

static void applPlRot(dcomp &a, dcomp &b, double cs, dcomp sn)
{
  double ra, ia, rb, ib;
  ra = cs * Re(a) + Re(sn) * Re(b) - Im(sn) * Im(b);
  ia = cs * Im(a) + Re(sn) * Im(b) + Im(sn) * Re(b);
  rb = cs * Re(b) - Re(sn) * Re(a) - Im(sn) * Im(a);
  ib = cs * Im(b) - Re(sn) * Im(a) + Im(sn) * Re(a);

  a = dcomp(ra, ia);
  b = dcomp(rb, ib);
}

// H y = s, x += Zy
static void update(unsigned n, unsigned k, dcomp* H,
                   unsigned ldH, dcomp* s, dcomp* Z, dcomp* x)
{
  dcomp *y = new dcomp[k];
  blas::copy(k, s, y);

  int inf;
  ztrtrs_(JOB_STR+5, JOB_STR, JOB_STR, &k, &N_ONE, H, &ldH, y, &k, &inf);
  assert(inf==0);

  // x += Z y
  blas::gemva(n, k, Z_ONE, Z, y, x);

  delete [] y;
}

unsigned FGMRes(const Matrix<dcomp>& A, dcomp* const b, dcomp* const x,
                double& eps, const unsigned m, unsigned& nsteps)
{
  double resid;
  unsigned j = 1;

  dcomp *r   = new dcomp[A.n];		// nx1, Residuum r
  dcomp *V   = new dcomp[A.n * (m+1)];	// nx(m+1), orthonormale Matrix
  dcomp *Z   = new dcomp[A.n * (m+1)];	// nx(m+1)
  dcomp *H   = new dcomp[(m+1)*m];	// (m+1)xm, Hessenbergmatrix
  dcomp *s   = new dcomp[m+1];		// (m+1)x1, rechte Seite des GS Hy=s
  dcomp *sn  = new dcomp[m+1];		// m+1, Sinus-Teil der Rotation
  double *cs = new double[m+1];		// m+1, Kosinus-Teil der Rotation

  // normb = norm(b)
  double normb = blas::nrm2(A.n, b);
  if (normb == 0.0) {
    blas::setzero(A.n, x);
    eps = 0.0;
    nsteps = 0;
    return 0;
  }

  // r = b - Ax
  blas::copy(A.n, b, r);
  A.amux(Z_MONE, x, r);

  double beta = blas::nrm2(A.n, r);

  if ((resid=beta/normb)<=eps) {
    eps = resid;
    nsteps = 0;
    delete [] cs;
    delete [] sn;
    delete [] s;
    delete [] H;
    delete [] Z;
    delete [] V;
    delete [] r;
    return 0;
  }

  while (j<=nsteps) {
    blas::copy(A.n, r, V);                 // v0 first orthonormal vector
    blas::scal(A.n, 1.0/beta, V);

    s[0] = beta;
    blas::setzero(m, s+1);

    for (unsigned i=0; i<m && j<=nsteps; i++, j++) {

      // z[i] = A M * v[i];
      blas::copy(A.n, V+i*A.n, Z+i*A.n);
      A.precond_apply(Z+i*A.n);

      blas::setzero(A.n, V+(i+1)*A.n);
      A.amux(Z_ONE, Z+i*A.n, V+(i+1)*A.n);

      for (unsigned k=0; k<=i; k++) {
        H[k+i*(m+1)] = blas::scpr(A.n, V+k*A.n, V+(i+1)*A.n);
        blas::axpy(A.n, Z_MONE * H[k+i*(m+1)], V+k*A.n, V+(i+1)*A.n);
      }

      H[i*(m+2)+1] = blas::nrm2(A.n, V+(i+1)*A.n);
      blas::scal(A.n, 1.0/H[i*(m+2)+1], V+(i+1)*A.n);

      // apply old Givens rotations to the last column in H
      for (unsigned k=0; k<i; k++)
        applPlRot(H[k+i*(m+1)], H[k+1+i*(m+1)], cs[k], sn[k]);

      // generate new Givens rotation which eleminates H[i*(m+2)+1]
      genPlRot(H[i*(m+2)], H[i*(m+2)+1], cs[i], sn[i]);

      // apply it to H and s
      applPlRot(H[i*(m+2)], H[i*(m+2)+1], cs[i], sn[i]);
      applPlRot(s[i], s[i+1], cs[i], sn[i]);

      if ((resid=fabs(abs(s[i+1])/normb)) < eps) {
        update(A.n, i+1, H, m+1, s, Z, x);
        eps = resid;
        nsteps = j;
        delete [] cs;
        delete [] sn;
        delete [] s;
        delete [] H;
        delete [] Z;
        delete [] V;
        delete [] r;
        return 0;
      }
      //#ifndef NDEBUG
      std::cout << "Step " << j << ", resid=" << resid << std::endl;
      //#endif

    }

    update(A.n, m, H, m+1, s, Z, x);

    // r = b - A * x
    blas::copy(A.n, b, r);
    A.amux(Z_MONE, x, r);

    beta = blas::nrm2(A.n, r);
    if ((resid=beta/normb) < eps) {
      eps = resid;
      nsteps = j;
      delete [] r;
      return 0;
    }
  }

  eps = resid;
  delete [] cs;
  delete [] sn;
  delete [] s;
  delete [] H;
  delete [] Z;
  delete [] V;
  delete [] r;
  return 1;
}
