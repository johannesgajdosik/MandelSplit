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

#ifndef MANDEL_IMAGE_H_
#define MANDEL_IMAGE_H_


#include "MpfClass.H"
#include "Vector.H"
#include "GmpFixedPoint.H"
#include "Logger.H"
#include "Julia.H"

#include <stdlib.h>

#include <gmpxx.h>

//static inline std::ostream &operator<<(std::ostream &o,const FLOAT_TYPE &x) {
//  return o << x.get_d();
//}

class ThreadPool;

static inline
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


class MandelImage {
public:
  ThreadPool &thread_pool;
  volatile _Atomic_word &terminate_flag;
  volatile _Atomic_word &nr_of_waiting_threads;
  mutable volatile _Atomic_word pixel_count;
private:
  unsigned int *data;
  const int screen_width; // line size
  const int screen_height;
  int priority_x;
  int priority_y;
  unsigned int max_iter;
  int precision; // -1..float,0..double,>0: gmp-size
  Complex<double> start; // coordinates of first pixel (top_left)
  Complex<double> d_re_im; // points to the next (right) pixel
  GmpFixedPointHeap start_re,start_im; // precision+2
  GmpFixedPointHeap d_re,d_im; // precision+2
    // only compute pixel value if existing value>=recalc_limit
  unsigned int recalc_limit;
public:
  unsigned int *getData(void) const {return data;}
  int getScreenWidth(void) const {return screen_width;}
  int getScreenHeight(void) const {return screen_height;}
  int getPriorityX(void) const {return priority_x;}
  int getPriorityY(void) const {return priority_y;}
  const Complex<double> &getStart(void) const {return start;}
  void setStart(const Complex<FLOAT_TYPE> &s) {
    start.re = s.re.get_d();
    start.im = s.im.get_d();
    if (precision > 0) {
      start_re.assign2FromMpf(precision,s.re.get_mpf_t());
      start_im.assign2FromMpf(precision,s.im.get_mpf_t());
    }
  }
  const Complex<double> &getDReIm(void) const {return d_re_im;}
  void setDReIm(const Complex<FLOAT_TYPE> &d) {
    d_re_im.re = d.re.get_d();
    d_re_im.im = d.im.get_d();
    if (precision > 0) {
      d_re.assign2FromMpf(precision,d.re.get_mpf_t());
      d_im.assign2FromMpf(precision,d.im.get_mpf_t());
      PrintMpf(d.im.get_mpf_t());
//cout << "setDReIm: assign2FromMpf(" << d.im
//     << ") returns " << d_im.convert2ToMpf(precision) << endl;
    }
  }
  const GmpFixedPoint &getStartRe(void) const {return start_re;}
  const GmpFixedPoint &getStartIm(void) const {return start_im;}
  const GmpFixedPoint &getDRe(void) const {return d_re;}
  const GmpFixedPoint &getDIm(void) const {return d_im;}
  unsigned int getMaxIter(void) const {return max_iter;}
  void setMaxIter(unsigned int m) {max_iter = m;}
  unsigned int getRecalcLimit(void) const {return recalc_limit;}
  void setRecalcLimit(unsigned int l) {recalc_limit = l;}
  void setPriorityPoint(int x,int y) {priority_x = x;priority_y = y;}
  int getVectorSize(void) const {
#ifdef __arm__
    return 1;
#endif
    return VECTOR_SIZE;
  }
    // -1..float,0..double,>=1:nr_of_limbs
  int getPrecision(void) const {
    return precision;
  }
  void setPrecision(int n) {
    const int n0 = (2+precision);
    const int n1 = (2+n);
    start_re.changePrecision(n0,n1);
    start_im.changePrecision(n0,n1);
    d_re.changePrecision(n0,n1);
    d_im.changePrecision(n0,n1);
    if (precision <= 0) {
      start_re.assign2FromDouble(n,start.re);
      start_im.assign2FromDouble(n,start.im);
      d_re.assign2FromDouble(n,d_re_im.re);
      d_im.assign2FromDouble(n,d_re_im.im);
//cout << "setPrecision(" << n << "): assign2FromDouble(" << d_re_im.im
//     << ") returns " << d_im.convert2ToMpf(n) << endl;
    }


    precision = n;
  }
  bool needRecalc(unsigned int val) const {
    return (val & 0x80000000) ||
           (val >= recalc_limit && recalc_limit < max_iter);
  }
  MandelImage(int screen_width,int screen_height,
              ThreadPool &thread_pool,
              volatile _Atomic_word &terminate_flag,
              volatile _Atomic_word &nr_of_waiting_threads)
    : thread_pool(thread_pool),
      terminate_flag(terminate_flag),
      nr_of_waiting_threads(nr_of_waiting_threads),
      pixel_count(0),
      data(new unsigned int[screen_width*screen_height]),
      screen_width(screen_width),screen_height(screen_height),
      priority_x(-1),priority_y(-1),
      precision(0),
      start(0,0),
      d_re_im(1,0),
      start_re(precision+2),start_im(precision+2),
      d_re(precision+2),d_im(precision+2),
      recalc_limit(0) {
  }
  ~MandelImage(void) {
    delete[] data;
  }
  void fillRect(unsigned int *d,int size_x,int size_y,
                unsigned int value) const {
    for (;size_y>0;size_y--,d+=screen_width) {
      for (int i=0;i<size_x;i++) {
#ifdef DEBUG
        if (d[i]) {
          cout << "FillRect: double drawing" << endl;
          ABORT();
        }
#endif
        d[i] = value;
      }
    }
  }
  void fillRect(int x,int y,int size_x,int size_y,unsigned int value) const {
    fillRect(data+y*screen_width+x,size_x,size_y,value);
  }
  void assertEmpty(int x,int y,int size_x,int size_y) const {
    unsigned int *d = data + y*screen_width + x;
    for (;size_y>0;size_y--,d+=screen_width) {
      for (int i=0;i<size_x;i++) {
        if (d[i]) {
          cout << "AssertEmpty: double drawing" << endl;
          ABORT();
        }
      }
    }
  }
  void assertNonEmpty(int x,int y,int size_x,int size_y) const {
    unsigned int *d = data + y*screen_width + x;
    for (;size_y>0;size_y--,d+=screen_width) {
      for (int i=0;i<size_x;i++) {
        if (d[i] == 0) {
          cout << "assertNonEmpty: not full" << endl;
          ABORT();
        }
      }
    }
  }
  unsigned int findGreatestValueNotMax(int size_x,int size_y) const {
    unsigned int rval = 8;
    const unsigned int *d = data;
    for (;size_y>0;size_y--,d+=screen_width) {
      for (int i=0;i<size_x;i++) {
        const unsigned int val = 0x7FFFFFFF & d[i];
        if (rval < val && val < max_iter) {
          rval = val;
          if (rval == max_iter-1) goto exit_loop;
        }
      }
    }
    exit_loop:
    return rval;
  }
};




#endif
