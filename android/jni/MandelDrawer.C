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

#include "MandelDrawer.H"
#include "ThreadPool.H"
#include "MandelImage.H"
#include "Job.H"
#include "Logger.H"

#include "GLee.h"
#if !defined(__ANDROID__) && defined(Complex)
  // X.h defines Complex as a macro for Polygon shapes
#undef Complex
#endif


#include <unistd.h> // sysconf

#include <fstream>

void CheckGlError(const char *description);

static const Complex<double> default_center(-0.75,0);
static const Complex<double> default_unity_pixel(1.0/256,0);
static const int default_max_iter = 512;

static
int GetNrOfProcessors(void) {
  {
    const char *const fname = "/sys/devices/system/cpu/possible";
    std::ifstream i(fname);
    if (i) {
      int id0;
      if (i >> id0) {
        char c;
        if (i >> c) {
          int id1;
          if (i >> id1) {
            if (id1 >= id0) {
              id1 = id1-id0+1;
              cout << "detected " << id1 << " processors from "
                   << fname << endl;
              return id1;
            }
          }
        } else {
          cout << "detected one processor from "
               << fname << endl;
          return 1;
        }
      }
    }
  }

  const long nr_of_processors = sysconf(_SC_NPROCESSORS_CONF);
  if (nr_of_processors <= 0) {
    cout << "FATAL: nr_of_processors=" << nr_of_processors << endl;
    abort();
  }
  cout << "nr_of_processors: " << nr_of_processors << endl;
  cout << "detected " << nr_of_processors << " processors from "
          "sysconf(_SC_NPROCESSORS_CONF)" << endl;
  return nr_of_processors;
}




MandelDrawer::MandelDrawer(void)
             :threads(new ThreadPool(GetNrOfProcessors())),
              image(0),
              parameters_changed(true),
              max_iter_changed(true),
              unity_pixel_changed(true),
              was_working_last_time(false) {
}

void MandelDrawer::reset(const Parameters &p) {
  MutexLock lock(mutex);
  if (params.center != p.center) {
    params.center = p.center;
    parameters_changed = true;
  }
  if (params.unity_pixel != p.unity_pixel) {
    params.unity_pixel = p.unity_pixel;
    parameters_changed = true;
    unity_pixel_changed = true;
  }
  setMaxIterPrivate(p.max_iter);
}

void MandelDrawer::initialize(int width,int height) {
  MandelDrawer::width = width;
  MandelDrawer::height = height;
  image = new MandelImage(width,height,
                          *threads,
                          threads->terminate_flag,
                          threads->getNrOfWaitingThreads());
}

MandelDrawer::~MandelDrawer(void) {
  delete image;
  delete threads;
}

void *MandelDrawer::getImageData(void) const {
  return image->data;
}

//unsigned int MandelDrawer::getMaxIter(void) const {
//  return max_iter;
//}

void MandelDrawer::sizeChanged(int w,int h) {
  if (w != width || h != height) {
    width = w;
    height = h;
    parameters_changed = true;
    unity_pixel_changed = true;
  }
}

void MandelDrawer::setMaxIterPrivate(unsigned int n) {
  if (n > 0xFFFFFF) {
    n = 0xFFFFFF;
  } else if (n < 8) {
    n = 8;
  }
  if (params.max_iter != n) {
    params.max_iter = n;
    max_iter_changed = true;
  }
}

int MandelDrawer::minimizeMaxIter(void) {
    // find the greatest value < max_iter
  unsigned int better_max = image->findGreatestValueNotMax(width,height);
  cout << "MandelDrawer::minimizeMaxIter: findGreatestValueNotMax: " << better_max << endl;
  int highest_bit = 0;
  for (int i=better_max+1;i>1;i>>=1) {highest_bit++;}
  if (highest_bit > 9) {
    highest_bit -= 9;
    better_max = (better_max+(1<<highest_bit))
               & ((~0)<<highest_bit);
  } else {
    better_max++;
  }
  cout << "MandelDrawer::minimizeMaxIter: better_max: " << better_max << endl;
  setMaxIter(better_max);
  return better_max;
}

void MandelDrawer::startRecalc(void) {
  parameters_changed = true;
}

//const Vector<float,2> MandelDrawer::ReImToXY(
//                        const Complex<double> &re_im_pos) const {
//  const Complex<float> tmp((re_im_pos - center)/unity_pixel);
//  return Vector<float,2>(tmp.re + 0.5f*width,
//                         0.5f*height - 1.f - tmp.im);
//}

Complex<double> MandelDrawer::XYToReIm(
                  const Vector<float,2> &screen_pos) const {
  MutexLock lock(mutex);
  return params.center
          + params.unity_pixel
          * Complex<float>(screen_pos[0] - 0.5f*width,
                           height-1-screen_pos[1] - 0.5f*height);
}

void MandelDrawer::fitReImPrivate(const Vector<float,2> &screen_pos,
                                  const Complex<double> &re_im_pos) {
  params.center = re_im_pos
                - params.unity_pixel
                * Complex<float>(screen_pos[0] - 0.5f*width,
                                 height-1-screen_pos[1] - 0.5f*height);
  {
    double h = params.center.length2();
    if (h > 4.0) {
      h = 2.0/sqrt(h);
      params.center *= h;
    }
  }
  params.center /= params.unity_pixel;
  params.center.re = floor(0.5 + params.center.re);
  params.center.im = floor(0.5 + params.center.im);
  params.center *= params.unity_pixel;
//cout << "fitReIm(" << screen_pos
//                   << " -> " << re_im_pos
//                   << "): " << params.center
//                   << ';' << params.unity_pixel << endl;
}

void MandelDrawer::fitReIm(const Vector<float,2> &screen_pos,
                           const Complex<double> &re_im_pos) {
  MutexLock lock(mutex);
  fitReImPrivate(screen_pos,re_im_pos);
}

const double epsilon = 4.44e-16; // 2^-51
const double epsilon2 = epsilon*epsilon;

void MandelDrawer::fitReIm(const Vector<float,2> &screen_pos0,
                           const Vector<float,2> &screen_pos1,
                           const Complex<double> &re_im_pos0,
                           const Complex<double> &re_im_pos1,
                           bool enable_rotation) {
  MutexLock lock(mutex);
  const Complex<float> screen_diff(screen_pos1[0]-screen_pos0[0],
                                   screen_pos0[1]-screen_pos1[1]);
  const Complex<double> re_im_diff(re_im_pos1-re_im_pos0);
  const double old_length2 = params.unity_pixel.length2();
  Complex<double> new_unity_pixel;
  if (enable_rotation) {
    new_unity_pixel = re_im_diff / screen_diff;
  } else {
    const double factor = sqrt(re_im_diff.length2()
                        / (screen_diff.length2()*old_length2));
    new_unity_pixel = params.unity_pixel * factor;
  }
  const double length2 = new_unity_pixel.length2();
  const double max_length = 4.0/((width<height)?width:height);
  const double min_length2 = epsilon2;
  if (length2 > max_length*max_length) {
    new_unity_pixel *= (max_length/sqrt(length2));
  } else if (length2 < min_length2) {
    new_unity_pixel *= sqrt(min_length2/length2);
  }
  if ( (params.unity_pixel-new_unity_pixel).length2() > epsilon2*length2) {
    params.unity_pixel = new_unity_pixel;
    if (enable_rotation || old_length2 != length2) {
      unity_pixel_changed = true;
    }
  }
  const Vector<float,2> screen_pos(0.5f*(screen_pos0+screen_pos1));
  const Complex<double> re_im_pos(0.5*(re_im_pos0+re_im_pos1));
  fitReImPrivate(screen_pos,re_im_pos);
//cout << "fitReIm(" << screen_pos0 << ',' << screen_pos1
//                   << " -> " << re_im_pos0 << ',' << re_im_pos1
//                   << "): " << params.center
//                   << ';' << params.unity_pixel << endl;
}

bool MandelDrawer::disableRotation(void) {
  MutexLock lock(mutex);
  if (params.unity_pixel.im != 0.0 || params.unity_pixel.re < 0) {
    params.unity_pixel.re = sqrt(params.unity_pixel.length2());
    params.unity_pixel.im = 0.0;
    unity_pixel_changed = true;
    return true;
  }
  return false;
}

void MandelDrawer::getOpenGLScreenCoordinate(int i,int j,float coor[2]) const {
  Complex<double> A(i,j);
  A *= image->d_re_im;
  A += image->start; // coordinates in Mandelbrot set
  A -= params.center;
  A /= params.unity_pixel;
  coor[0] = (A.re*2.0) / width;
  coor[1] = (A.im*2.0) / height;
}

void MandelDrawer::getOpenGLScreenCoordinates(float coor[8],
                                              Parameters &p) const {
/* The existing image was computed with
    image->d_re_im = unity_pixel;
    image->start = center - 0.5*unity_pixel * (width+sqrt(-1)*height);
  and the texture was mapped to Opengl with
    coor[0] = -1;
    coor[1] = -1;
    coor[6] = 1;
    coor[7] = 1;
    coor[2] = coor[6];
    coor[3] = coor[1];
    coor[4] = coor[0];
    coor[5] = coor[7];
  
  In the meantime unity_pixel and center have changed.
  How shall the existing texture be mapped?

  An existing point in the image (i+width*j) has Mandelbrot coordinates
    A = image->start+(i,j)*image->d_re_im;
  This point A shall now be displayed on
    ReImToXY(A)
  So it needs the OpenGl coordinates
    Complex<float> tmp = ((A - center)/unity_pixel) scaled by [w/h,1]
*/
  MutexLock lock(mutex);
  getOpenGLScreenCoordinate(0,0,coor+0);
  getOpenGLScreenCoordinate(width,0,coor+2);
  getOpenGLScreenCoordinate(0,height,coor+4);
  getOpenGLScreenCoordinate(width,height,coor+6);
  p = params;

//cout << "MandelDrawer::getOpenGLScreenCoordinates: "
//     << coor[0] << '/' << coor[1] << ";  "
//     << coor[6] << '/' << coor[7] << endl;
}






float MandelDrawer::getProgress(void) const {
  return image->pixel_count / (float)(width*height);
}

bool MandelDrawer::step(void) {
//cout << "step: start" << endl;
  bool rval = true;
  if (parameters_changed || max_iter_changed) {
    __sync_synchronize();

    threads->cancelExecution();
    image->pixel_count = 0;

    if (unity_pixel_changed) {
      image->recalc_limit = 0;
    } else {
      image->recalc_limit = image->max_iter;
    }

//cout << "step: (re)starting work: unity_pixel_changed:" << unity_pixel_changed
//     << ", recalc_limit:" << image->recalc_limit
//     << ", max_iter:" << image->max_iter
//     << endl;

    { // re-scale MandelImage
      Complex<double> K = image->d_re_im.inverse();
      Complex<double> D;
      {
        MutexLock lock(mutex);
        image->max_iter = params.max_iter;
        D = (params.center - image->start) * K;
        K *= params.unity_pixel;
        image->d_re_im = params.unity_pixel;
        image->start = params.center
                     - 0.5*params.unity_pixel*Complex<double>(width,height);
      }

      unsigned int *const new_data = new unsigned int[width*height];
      unsigned int *nd = new_data;
      for (int y=0;y<height;y++,nd+=width) {
        for (int x=0;x<width;x++) {
          const Complex<double> A = D + K * Complex<double>(x - 0.5*(width-1),
                                                            y - 0.5*(height-1));
          const int xb = (int)floor(A.re);
          const int yb = (int)floor(A.im);
          if (xb < 0 || yb < 0 || width <= xb || height <= yb) {
            nd[x] = 0x80000000;
          } else {
            if (unity_pixel_changed) {
              nd[x] = image->data[yb*(image->screen_width)+xb] | 0x80000000;
            } else {
              nd[x] = image->data[yb*(image->screen_width)+xb];
              if (!image->needRecalc(nd[x])) image->pixel_count++;
            }
          }
        }
      }
      for (int y=0;y<height;y++) {
        memcpy(image->data + y*(image->screen_width),new_data+y*width,
               sizeof(unsigned int)*width);
      }
      delete[] new_data;
      
      
//cout << "step: buffer rescaled" << endl;
      CheckGlError("before glTexSubImage2D 0");
#ifndef __ANDROID__
      glPixelStorei(GL_UNPACK_ROW_LENGTH,image->screen_width);
#endif
      glTexSubImage2D(GL_TEXTURE_2D,0,
                      0,0,width,height,
                      GL_RGBA,GL_UNSIGNED_BYTE,
                      image->data);
      CheckGlError("after glTexSubImage2D 0");
    }
    unity_pixel_changed = false;

    threads->startExecution(new MainJob(*image,width,height));
//cout << "step: startExecution: new MainJob queued" << endl;
    was_working_last_time = true;
    parameters_changed = false;
    max_iter_changed = false;
  } else {
    if (was_working_last_time) {
      if (threads->workIsFinished()) {
        const float progress = getProgress();
        if (progress != 1.f) {
          ABORT();
        }
MutexLock lock(mutex);
params.max_iter = image->max_iter;





        // cout << "step: work finished" << endl;
        was_working_last_time = false;
      } else {
        // cout << "step: still working" << endl;
      }
      CheckGlError("before glTexSubImage2D 2");
      
        // TODO: unite jobs and add currently executing jobs
        // GL_EXT_unpack_subimage seems to be not supported
#ifndef __ANDROID__
      glPixelStorei(GL_UNPACK_ROW_LENGTH,image->screen_width);
#endif
      threads->draw(image);

//      glTexSubImage2D(GL_TEXTURE_2D,0,
//                      0,0,width,height,
//                      GL_RGBA,GL_UNSIGNED_BYTE,
//                      image->data);
      CheckGlError("after glTexSubImage2D 2");
    } else {
      rval = false;
    }
  }
//cout << "step: returns " << rval << endl;
  return rval;
}
