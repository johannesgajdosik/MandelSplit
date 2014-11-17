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

#include "Logger.H"

#include <stdlib.h>

class ThreadPool;

class MandelImage {
public:
  double start_re; // start of first pixel
  double start_im;
  double d_re_im; // size of one pixel
  unsigned int *data;
  const int screen_width; // line size
  const int screen_height;
  unsigned int max_iter;
  ThreadPool &thread_pool;
  volatile _Atomic_word &terminate_flag;
  volatile _Atomic_word &nr_of_waiting_threads;
public:
  MandelImage(int screen_width,int screen_height,
              ThreadPool &thread_pool,
              volatile _Atomic_word &terminate_flag,
              volatile _Atomic_word &nr_of_waiting_threads)
    : data(new unsigned int[screen_width*screen_height]),
      screen_width(screen_width),screen_height(screen_height),
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
          abort();
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
          abort();
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
          abort();
        }
      }
    }
  }
  void horzLine4(unsigned int *d,double re,double im,int size_x) const {
      // prerequirement: size_x%VECTOR_SIZE = 0
    VECTOR_TYPE mr[VECTOR_SIZE];
    VECTOR_TYPE mi[VECTOR_SIZE];
    im += start_im;
    for (int i=0;i<VECTOR_SIZE;i++) mi[i] = im;
    for (;size_x>=VECTOR_SIZE;size_x-=VECTOR_SIZE,d+=VECTOR_SIZE) {
      if (terminate_flag) return;
      for (int i=0;i<VECTOR_SIZE;i++,re+=d_re_im) {
#ifdef DEBUG
        if (d[i]) {
          cout << "horzLine4: double drawing" << endl;
          abort();
        }
#endif
        mr[i] = start_re+re;
      }
      JULIA_FUNC(mr,mi,max_iter,d);
    }
  }
  void horzLine(unsigned int *d,double re,double im,int size_x) const {
    VECTOR_TYPE mr[VECTOR_SIZE];
    VECTOR_TYPE mi[VECTOR_SIZE];
    im += start_im;
    for (int i=0;i<VECTOR_SIZE;i++) mi[i] = im;
    for (;size_x>=VECTOR_SIZE;size_x-=VECTOR_SIZE,d+=VECTOR_SIZE) {
      if (terminate_flag) return;
      for (int i=0;i<VECTOR_SIZE;i++,re+=d_re_im) {
#ifdef DEBUG
        if (d[i]) {
          cout << "horzLine: double drawing" << endl;
          abort();
        }
#endif
        mr[i] = start_re+re;
      }
      JULIA_FUNC(mr,mi,max_iter,d);
    }
    if (size_x > 0) {
      if (terminate_flag) return;
      unsigned int tmp[VECTOR_SIZE];
      for (int i=0;i<VECTOR_SIZE;i++,re+=d_re_im) mr[i] = start_re+re;
      JULIA_FUNC(mr,mi,max_iter,tmp);
      for (int i=0;i<size_x;i++) {
#ifdef DEBUG
        if (d[i]) {
          cout << "horzLine: double drawing2" << endl;
          abort();
        }
#endif
        d[i] = tmp[i];
      }
    }
  }
  void horzLine(int x,int y,int size_x) const {
    horzLine(data+y*screen_width+x,x*d_re_im,y*d_re_im,size_x);
  }
  void vertLine(unsigned int *d,double re,double im,int size_y) const {
    VECTOR_TYPE mr[VECTOR_SIZE];
    VECTOR_TYPE mi[VECTOR_SIZE];
    re += start_re;
    for (int i=0;i<VECTOR_SIZE;i++) mr[i] = re;
    unsigned int tmp[VECTOR_SIZE];
    for (int i=0;;) {
      if (terminate_flag) return;
      for (int j=0;j<VECTOR_SIZE;j++,im+=d_re_im) mi[j] = start_im+im;
      JULIA_FUNC(mr,mi,max_iter,tmp);
      for (int j=0;j<VECTOR_SIZE;j++,d+=screen_width) {
        *d = tmp[j];
        if (++i >= size_y) return;
      }
    }
  }
  void vertLine(int x,int y,int size_y) const {
    vertLine(data+y*screen_width+x,x*d_re_im,y*d_re_im,size_y);
  }
  void fullRect(unsigned int *data,
                double re,double im,int size_x,int size_y) const {
    unsigned int *d = data;
    int w = size_x & (~(VECTOR_SIZE-1));
    double yh = im;
    for (int j=0;j<size_y;j++,d+=screen_width,yh+=d_re_im) {
      horzLine4(d,re,yh,w);
      if (terminate_flag) return;
    }
    d = data+w;
    re += w*d_re_im;
    for (;w<size_x;w++,d++,re+=d_re_im) {
      vertLine(d,re,im,size_y);
      if (terminate_flag) return;
    }
  }
  void fullRect(int x,int y,int size_x,int size_y) const {
    fullRect(data+y*screen_width+x,x*d_re_im,y*d_re_im,size_x,size_y);
  }
};




#endif
