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

#ifndef MANDEL_DRAWER_H_
#define MANDEL_DRAWER_H_

#include "MpfClass.H"
#include "Vector.H"
#include "Mutex.H"

#include <iostream>
//#include <gmpxx.h>

class MandelDrawer {
public:
  MandelDrawer(void);
  ~MandelDrawer(void);
  struct Parameters {
    Parameters(void) : unity_pixel(0,0),precision(0) {}
    Parameters(const Parameters &p) {assign(p);}
    const Parameters &operator=(const Parameters &p) {assign(p);return *this;}
    void assign(const Parameters &p) {
      center.re.set_prec(p.center.re.get_prec());
      center.re = p.center.re;
      center.im.set_prec(p.center.im.get_prec());
      center.im = p.center.im;
      unity_pixel.re.set_prec(p.unity_pixel.re.get_prec());
      unity_pixel.re = p.unity_pixel.re;
      unity_pixel.im.set_prec(p.unity_pixel.im.get_prec());
      unity_pixel.im = p.unity_pixel.im;
      max_iter = p.max_iter;
      precision = p.precision;
    }
    Parameters(const Complex<FLOAT_TYPE> &cen,
               const Complex<FLOAT_TYPE> &uni,
               unsigned int max_iter)
        : max_iter(max_iter),precision(0) {
      center.re.set_prec(cen.re.get_prec());
      center.re = cen.re;
      center.im.set_prec(cen.im.get_prec());
      center.im = cen.im;
      unity_pixel.re.set_prec(uni.re.get_prec());
      unity_pixel.re = uni.re;
      unity_pixel.im.set_prec(uni.im.get_prec());
      unity_pixel.im = uni.im;
      updatePrecision();
    }
    Parameters(const void *&saved_state);
    size_t getSavedStateSize(void);
    void dumpSaveState(void *&saved_state) const;
      // coordinates of image center in the Mandelbrot set:
    Complex<FLOAT_TYPE> center;
      // points to one pixel right
    Complex<FLOAT_TYPE> unity_pixel;
    unsigned int max_iter;
    int precision; // just for conveniance, derived from unity_pixel
    void setPrecision(int p) {
      precision = p;
      const mp_bitcnt_t prec = (((p <= 0) ? 1 : p) + 2) * (8*sizeof(mp_limb_t));
      center.re.set_prec(prec);
      center.im.set_prec(prec);
      unity_pixel.re.set_prec(prec);
      unity_pixel.im.set_prec(prec);
    }
    void updatePrecision(void);
  };
    // params describe the position of the _next_ image that will be
    // calculated after the user has finished dragging.
    // params are changed by the GUI thread:
    //   XYToReIm() for initial query / fitReIm() for actual change of params
    // and are queried by the main (drawing) thread
    //   getOpenGLScreenCoordinates for getting coordinates of preview
    // Only when parameters_changed/max_iter_changed/unity_pixel_changed is set,
    // params will be copied into MandelImage,
    // and a new image will be calculated
  void reset(const Parameters &p);
  void getParameters(Parameters &p) const;
  static int GetPrecisionBits(int nr_of_limbs) {
    if (nr_of_limbs <= 0) {
      if (nr_of_limbs == 0) return 53;
      return 24;
    } else {
      return nr_of_limbs*(8*sizeof(mp_limb_t))+1; // plus signbit
    }
  }
  void setMaxIter(unsigned int n) {
    MutexLock lock(mutex);
    setMaxIterPrivate(n);
  }
  unsigned int getMaxIter(void) const {
    MutexLock lock(mutex);
    return params.max_iter;
  }
  int minimizeMaxIter(void);
  void initialize(int width,int height,int screen_width,int screen_height);
  void sizeChanged(int w,int h);
  void *getImageData(void) const;

  int getWidth(void) const {return width;}
  int getHeight(void) const {return height;}

  void XYToReIm(const Vector<float,2> &screen_pos,
                Complex<FLOAT_TYPE> &re_im_pos) const {
    MutexLock lock(mutex);
    XYToReImPrivate(screen_pos,re_im_pos);
  }

    // set Transformation to ReIm coordinates:
  void fitReset(void);
  void fitReIm(const Vector<float,2> &screen_pos,
               const Complex<FLOAT_TYPE> &re_im_pos) {
    MutexLock lock(mutex);
    fitReImPrivate(screen_pos,re_im_pos);
  }
    // returns params.precision, which may have changed:
  int fitReIm(const Vector<float,2> &screen_pos0,
              const Vector<float,2> &screen_pos1,
              Complex<FLOAT_TYPE> &re_im_pos0,
              Complex<FLOAT_TYPE> &re_im_pos1,
              bool enable_rotation);
    // returns true if unity_pixel had to be changed
  bool disableRotation(void);

  void startRecalc(void);


  bool step(float coor[8],Parameters &p);
  float getProgress(void) const;
  long long int fetchPixelSum(void);
  void setPriorityPoint(const Vector<float,2> screen_pos);
private:
  void setPriorityPointPrivate(const Vector<float,2> screen_pos);
  void XYToReImPrivate(const Vector<float,2> &screen_pos,
                       Complex<FLOAT_TYPE> &re_im_pos) const;
  void fitReImPrivate(const Vector<float,2> &screen_pos,
                      const Complex<FLOAT_TYPE> &re_im_pos);

//  int setPrecisionPrivate(int nr_of_limbs);

  void setMaxIterPrivate(unsigned int n);
  void getOpenGLScreenCoordinate(unsigned int i,unsigned int j,
                                 float coor[2]) const;
  class ThreadPool *const threads;
  class MandelImage *image;

    // step(),getOpenGLScreenCoordinates() is called from drawing thread while
    // fitReIm,etc are called from GUI Java thread
  mutable Mutex mutex; // for params
  Parameters params;

    // with/height of the image in pixels
  int width,height;

  volatile bool parameters_changed;
  volatile bool max_iter_changed;
  volatile bool unity_pixel_changed;
  bool was_working_last_time;
};

std::ostream &operator<<(std::ostream &o,const MandelDrawer::Parameters &p);
std::istream &operator>>(std::istream &i,MandelDrawer::Parameters &p);

#endif
