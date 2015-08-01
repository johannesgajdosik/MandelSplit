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
g++ GmpFixedPoint.C GmpFixedPointUnitTest.C -lgmp
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
  const int n = 2;
  GmpFixedPoint a,b,c;
  a.initialize(n);
  b.initialize(n);
  c.initialize(n);
  double dxy = 0.1;
  for (double y=-2.0+dxy;y<2.0;y+=dxy) {
    if (b.assignFromDouble(n,y)) abort();
    for (double x=-2.0+dxy;x<2.0;x+=dxy) {
      if (a.assignFromDouble(n,x)) abort();
      cout << std::setw(3)
           << (int)(GmpMandel(n,a,b,0xFF)-MandelC(x,y,0xFF));
    }
    cout << endl;
  }

  const int test_count = 1000000;
  for(int i=0;i<test_count;i++) {
    const long int x_int = random();
    const long int y_int = random();
    const double x = (1.0 + x_int) * (4.0/(2.0+RAND_MAX)) - 2.0;
    const double y = (1.0 + y_int) * (4.0/(2.0+RAND_MAX)) - 2.0;
    if (a.assignFromDouble(n,x)) abort();
    if (b.assignFromDouble(n,y)) abort();
    const int n_gmp = GmpMandel(n,a,b,1000);
    const int n_double = MandelC(x,y,1000);
    const int diff = n_gmp - n_double;
    if (diff) {
      cout << x_int << ' ' << y_int << "; " << n_gmp << ' ' << n_double << endl;
    }
  }
  
//  if (a.assignFromDouble(-2.0)) abort();

  return 0;
}

