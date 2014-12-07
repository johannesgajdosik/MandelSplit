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

#ifndef MANDEL_IMAGE_H_
#define MANDEL_IMAGE_H_


#include "Julia.h"

#include "Vector.H"
#include "Logger.H"

#include <stdlib.h>

class ThreadPool;

class MandelImage {
public:
  mutable volatile _Atomic_word pixel_count;
  Complex<double> start; // coordinates of first pixel (top_left)
  Complex<double> d_re_im; // points to the next (right) pixel
  unsigned int *data;
  const int screen_width; // line size
  const int screen_height;
  mutable unsigned int max_iter;
    // only compute pixel value if existing value>=recalc_limit
  unsigned int recalc_limit;
  ThreadPool &thread_pool;
  volatile _Atomic_word &terminate_flag;
  volatile _Atomic_word &nr_of_waiting_threads;
public:
  bool needRecalc(unsigned int val) const {
    return (val & 0x80000000) ||
           (val >= recalc_limit && recalc_limit < max_iter);
  }
  MandelImage(int screen_width,int screen_height,
              ThreadPool &thread_pool,
              volatile _Atomic_word &terminate_flag,
              volatile _Atomic_word &nr_of_waiting_threads)
    : data(new unsigned int[screen_width*screen_height]),
      screen_width(screen_width),screen_height(screen_height),
      recalc_limit(0),
      thread_pool(thread_pool),
      terminate_flag(terminate_flag),
      nr_of_waiting_threads(nr_of_waiting_threads) {}
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
