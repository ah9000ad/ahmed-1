/*
    AHMED -- Another software library on Hierarchical Matrices for
             Elliptic Differential equations

    Copyright (c) 2012 Mario Bebendorf

    You should have received a copy of the license along with 
    this software; if not, see AHMED's internet site.
*/


// CRS transpose

void CS_transp(unsigned n, double* A, unsigned* jA, unsigned *iA,
               double* B, unsigned *jB, unsigned *iB)
{
  unsigned nnz = iA[n];
  unsigned *inz = new unsigned[n];

  for (unsigned i=0; i<n; ++i) inz[i] = 0;

  // compute number of entries of each column in A
  for (unsigned l=0; l<nnz; ++l) inz[jA[l]]++;

  // create iB
  iB[0] = nnz = 0;
  for (unsigned i=0; i<n; ++i) {
    nnz += inz[i];
    inz[i] = 0;
    iB[i+1] = nnz;
  }

  // create arrays jB, B
  unsigned l = iA[0];
  for (unsigned i=0; i<n; ++i) {
    while (l<iA[i+1]) {
      unsigned j = jA[l];
      unsigned k = iB[j] + inz[j]++;
      B[k] = A[l++];
      jB[k] = i;
    }
  }

  delete [] inz;
}

