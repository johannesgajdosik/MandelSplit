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

#include "Vector.H"
#include "Mutex.H"

class MandelDrawer {
public:
  MandelDrawer(void);
  ~MandelDrawer(void);
  struct Parameters {
    Parameters(void) {}
    Parameters(const Complex<double> &center,
               const Complex<double> &unity_pixel,
               unsigned int max_iter)
      : center(center),unity_pixel(unity_pixel),max_iter(max_iter) {}
      // coordinates of image center in the Mandelbrot set:
    Complex<double> center;
      // points to one pixel right
    Complex<double> unity_pixel;
    unsigned int max_iter;
  };
  void reset(const Parameters &p);
  void getParameters(Parameters &p) {
    MutexLock lock(mutex);
    p = params;
  }
  void setMaxIter(unsigned int n) {
    MutexLock lock(mutex);
    setMaxIterPrivate(n);
  }
  int minimizeMaxIter(void);
  void initialize(int width,int height);
  void sizeChanged(int w,int h);
  void *getImageData(void) const;

  int getWidth(void) const {return width;}
  int getHeight(void) const {return height;}

    // ReIm <-> screen coordinate conversion
//  const Vector<float,2> ReImToXY(const Complex<double> &re_im_pos) const;
  Complex<double> XYToReIm(const Vector<float,2> &screen_pos) const;

  void getOpenGLScreenCoordinates(float coor[8],Parameters &p) const;

    // set Transformation to ReIm coordinates:
  void fitReIm(const Vector<float,2> &screen_pos,
               const Complex<double> &re_im_pos);
  void fitReIm(const Vector<float,2> &screen_pos0,
               const Vector<float,2> &screen_pos1,
               const Complex<double> &re_im_pos0,
               const Complex<double> &re_im_pos1,
               bool enable_rotation);
    // returns true if unity_pixel had to be changed
  bool disableRotation(void);

  void startRecalc(void);

  bool step(void);
  float getProgress(void) const;
private:
  void fitReImPrivate(const Vector<float,2> &screen_pos,
                      const Complex<double> &re_im_pos);

//  unsigned int getMaxIter(void) const {return max_iter;}
  void setMaxIterPrivate(unsigned int n);
  void getOpenGLScreenCoordinate(int i,int j,float coor[2]) const;
  class ThreadPool *const threads;
  class MandelImage *image;

    // step(),getOpenGLScreenCoordinates() is called from drawing thread while
    // fitReIm,etc are called from GUI Java thread
  mutable Mutex mutex; // for params
  Parameters params;

    // with/height of the image in pixels
  int width,height;

  bool parameters_changed;
  bool max_iter_changed;
  bool unity_pixel_changed;
  bool was_working_last_time;
};

#endif
