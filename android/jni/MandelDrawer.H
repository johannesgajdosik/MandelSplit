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

class MandelDrawer {
public:
  MandelDrawer(void);
  ~MandelDrawer(void);
  void reset(void);
  void reset(double center_re,double center_im,double size_re_im,
             unsigned int max_iter);
  void initialize(int width,int height);
  void sizeChanged(int w,int h);
  void *getImageData(void) const;
  unsigned int getMaxIter(void) const;
  void setMaxIter(unsigned int n);
  double getCenterRe(void) const {return center_x;}
  double getCenterIm(void) const {return center_y;}
  double getSizeReIm(void) const {return size_xy;}

    // get ReIm coordinates
  void XYToReIm(const Vector<float,2> &screen_pos,
                Vector<double,2> &re_im_pos) const;
  double XYToReImScale(void) const;
  void getOpenGLScreenCoordinates(float coor[8]) const;

    // set Transformation to ReIm coordinates:
  void fitReIm(const Vector<float,2> &screen_pos,
               const Vector<double,2> &re_im_pos);
  void fitReIm(const Vector<float,2> &screen_pos,
               const Vector<double,2> &re_im_pos,
               double scale);

  void startRecalc(void);

  void convertScreenCoordinates(int x,int y,double &re,double &im) const;
  void gotoScreenCoordinates(int x,int y,double re,double im);

  bool step(void);
  float getProgress(void) const;
private:
  class ThreadPool *const threads;
  class MandelImage *image;
  double center_x;
  double center_y;
  double size_xy;
  int width;
  int height;
  unsigned int max_iter;
  bool new_parameters;
  bool new_max_iter;
  bool new_size_xy;
  bool was_working_last_time;
};

#endif
