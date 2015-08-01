/*
    Author and Copyright: Johannes Gajdosik, 2015

    This file is part of MandelSplit.

    MandelSplit is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    MandelSplit is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with MandelSplit.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
g++ -Isrc GmpFixedPoint.C GmpFixedPointUnitTest3.C -lgmpxx -lgmp
*/

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <iomanip>
using std::cin;
using std::cout;
using std::endl;

#include "GmpFixedPoint.H"

double Rand(void) {
  return (1.0 + random()) * (4.0/(2.0+RAND_MAX)) - 2.0;
}

void PrintMpf(const mpf_t x) {
  gmp_printf("%Fg, prec=%d, size=%d, exp=%ld, limbs:",
             x,x[0]._mp_prec,x[0]._mp_size,x[0]._mp_exp);
  for (int i=0;i<x[0]._mp_size;i++) {
#ifdef __x86_64__
    printf(" 0x%016lx",x[0]._mp_d[i]);
#else
    printf(" 0x%08x",x[0]._mp_d[i]);
#endif
  }
  printf("\n");
}

int main(int argc,char *argv[]) {
  const int n = 1;
  GmpFixedPointHeap a(n),b(n),c(n+2);

//  FLOAT_TYPE x(0,0,1*GmpFixedPoint::bits_per_limb);
double x;
  cin >> x;

  cout << x << endl;
//  PrintMpf(x.get_mpf_t());

//  c.assign2FromMpf(n,x.get_mpf_t());
//  a.assignFromMpf(n,x.get_mpf_t());
  c.assign2FromDouble(n,x);
  a.assignFromDouble(n,x);

cout << endl;
cout << PrintableGmpFixedPoint(n+2,c) << endl;
cout << PrintableGmpFixedPoint(n,a) << endl;
cout << endl;
PrintMpf(c.convert2ToMpf(n).get_mpf_t());
  return 0;
}

