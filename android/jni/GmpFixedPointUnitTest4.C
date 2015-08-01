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
g++ -Isrc GmpFixedPoint.C GmpFixedPointUnitTest4.C -lgmpxx -lgmp
*/

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <iomanip>
using std::cin;
using std::cout;
using std::endl;

#include "GmpFixedPoint.H"
#include "GmpFloat.H"
#include "Vector.H"


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
  const int n = 2;
//  GmpFixedPointHeap a(n),b(n),c(n+2);

  GmpFloat x(0,2*GmpFixedPoint::bits_per_limb);
  GmpFloat y(0,2*GmpFixedPoint::bits_per_limb);
  GmpFloat z(0,2*GmpFixedPoint::bits_per_limb);
  cin >> x >> y;

Complex<GmpFloat> a(x,y);
cout << a << endl;
cout << ~a << endl;
cout << a.inverse() << endl;
a.invert();
cout << a << endl;
cout << a.inverse() << endl;


  return 0;
}

