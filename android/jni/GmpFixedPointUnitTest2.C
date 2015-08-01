/*
    Author and Copyright: Johannes Gajdosik, 2014

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
g++ GmpFixedPoint.C GmpFixedPointUnitTest2.C -lgmp
*/

#include "GmpFixedPoint.H"

#include <stdlib.h>

#include <iostream>
#include <iomanip>
using std::cout;
using std::endl;

unsigned int MandelC(double mr,double mi,
                     unsigned int max_n) {
  double jr = 0.0;
  double ji = 0.0;
  unsigned int n = 0;
  while (n < max_n) {
//cout << n << ": " << jr << ' ' << ji << endl;
    double rq = jr*jr;
    double iq = ji*ji;
    if (rq+iq >= 4.0) return n;
    ji *= jr;
    ji = ji + ji + mi;
    jr = rq - iq + mr;
    n++;
  }
  return n;
//  return (0x10000*n+(max_n/2))/max_n;
}

double Rand(void) {
  return (1.0 + random()) * (4.0/(2.0+RAND_MAX)) - 2.0;
}

int main(int argc,char *argv[]) {
  const int n = 1;
  GmpFixedPointHeap a(n),b(n),c(n);

/*
  const long int x_int = 445705869;
  const long int y_int = 968026942;

    const double x = (1.0 + x_int) * (4.0/(2.0+RAND_MAX)) - 2.0;
    const double y = (1.0 + y_int) * (4.0/(2.0+RAND_MAX)) - 2.0;
    if (a.assignFromDouble(n,x)) abort();
    if (b.assignFromDouble(n,y)) abort();

cout << PrintableGmpFixedPoint(n,a) << ' ' << x - a.convertToDouble(n) << endl;
cout << PrintableGmpFixedPoint(n,b) << ' ' << y - b.convertToDouble(n) << endl;

    const int n_gmp = GmpMandel(n,a,b,1000);
    const int n_double = MandelC(x,y,1000);
    const int diff = n_gmp - n_double;
    if (diff) {
      cout << x_int << ' ' << y_int << "; " << n_gmp << ' ' << n_double << endl;
    }
*/
/*
const long int val_a = -4;
const long int val_b = 1LL;
const long int factor = 5;

  a.assignFromInt(n,val_a);
  b.assignFromInt(n,val_b);
cout << "  Signed: "
     << PrintableGmpFixedPoint(n,a) << " + " << factor << "*"
     << PrintableGmpFixedPoint(n,b) << " = ";
  mp_limb_t rc = a.addMulS(n,b,factor);
cout << PrintableGmpFixedPoint(n,a) << " ; " << std::hex << rc << std::dec << endl;

  a.assignFromInt(n,val_a);
cout << "Unsigned: "
     << PrintableGmpFixedPoint(n,a) << " + " << factor << "*"
     << PrintableGmpFixedPoint(n,b) << " = ";
  rc = a.addMulU(n,b,factor);
cout << PrintableGmpFixedPoint(n,a) << " ; " << std::hex << rc << std::dec << endl;
*/

const long int val_a    = -0x400000000LL;
const long int factor_a =  0x200000000LL;

const long int val_b    =  0x400000001LL;
const long int factor_b =  0x400000001LL;

  a.assignFromInt(n,val_a);
  b.assignFromInt(n,val_b);
  long int rc = c.linCombPlus(n,a,factor_a,b,factor_b);

cout << PrintableGmpFixedPoint(n,c) << " ; " << std::hex << rc << std::dec << endl;

  return 0;
}

