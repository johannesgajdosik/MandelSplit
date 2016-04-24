/*
    Author and Copyright: Johannes Gajdosik, 2016

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
g++ -Isrc GmpFixedPoint.C GmpFixedPointUnitTest5.C -lgmpxx -lgmp
*/

#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <iomanip>
using std::cin;
using std::cout;
using std::endl;

#include "GmpFixedPoint.H"

union MyIeeeDouble
{
  struct
    {
      unsigned int manl:32;
      unsigned int manh:20;
      unsigned int exp:11;
      unsigned int sig:1;
    } s32;
  struct
    {
      unsigned long long int man:52;
      unsigned int exp:11;
      unsigned int sig:1;
    } s;
  double d;
};

double Rand(void) {
  return (1.0 + random()) * (4.0/(2.0+RAND_MAX)) - 2.0;
}

void PrintDouble(double d) {
  MyIeeeDouble x;
  x.d = d;
  cout << "sig: " << x.s.sig
       << ", exp: " << x.s.exp
       << ", man: 0x" << std::hex << x.s.man << std::dec;
}

void PrintMpf(const mpf_t x) {
  gmp_printf("%Fg, prec=%d, size=%d, exp=%ld, limbs:",
             x,x[0]._mp_prec,x[0]._mp_size,x[0]._mp_exp);
  for (int i=0;i<x[0]._mp_size;i++) {
#ifdef __x86_64__
    printf(" 0x%016lx",x[0]._mp_d[i]);
#else
    printf(" 0x%08lx",x[0]._mp_d[i]);
#endif
  }
  printf("\n");
}

//#define INTERACTIVE

int main(int argc,char *argv[]) {
  const int n = 2;
  GmpFixedPointHeap a(n),b(n),c(n+2);

#ifdef INTERACTIVE

//  FLOAT_TYPE x(0,0,1*GmpFixedPoint::bits_per_limb);
MyIeeeDouble x;
//  cin >> x.d;
  x.d = 0.000487335080560455;
cout << "sig: " << x.s.sig
     << ", exp: " << x.s.exp
     << ", man: 0x" << std::hex << x.s.man << std::dec
     << endl;


cout << x.d << endl;

//  PrintMpf(x.get_mpf_t());

//  c.assign2FromMpf(n,x.get_mpf_t());
//  a.assignFromMpf(n,x.get_mpf_t());
  c.assign2FromDouble(n,x.d);
  a.assignFromDouble(n,x.d);

cout << endl;
cout << PrintableGmpFixedPoint(n+2,c) << endl;
cout << PrintableGmpFixedPoint(n,a) << endl;
cout << endl;
PrintMpf(c.convert2ToMpf(n).get_mpf_t());
cout << c.convertToDoubleNew(n+2) << endl;

#endif


for (double x = 1e-42; x < 1e19; x*=1.99) {
  c.assign2FromDouble(n,x);
  const double y = c.convertToDoubleNew(n+2);
  double e = (x-y)/x;
  if (e < 0.0) e = -1;
  if (e > 0.0) {
    cout << PrintableGmpFixedPoint(n+2,c) << ": "
         << std::setprecision(15) << x << "(";
    PrintDouble(x);
    cout << " != " << y << "(";
    PrintDouble(y);
    cout << ", Error: " << e << endl;
  }
}

  return 0;
}

