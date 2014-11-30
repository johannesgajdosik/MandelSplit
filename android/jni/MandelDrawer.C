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

#include <unistd.h> // sysconf

#include <fstream>

void CheckGlError(const char *description);

static const double default_center_x = -0.75;
static const double default_center_y = 0;
static const double default_size_xy = 2.5;
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
              center_x(default_center_x),
              center_y(default_center_y),
              size_xy(default_size_xy),
              max_iter(default_max_iter),
              new_parameters(true),
              new_max_iter(true),
              new_size_xy(true),
              was_working_last_time(false) {
}

void MandelDrawer::reset(void) {
  reset(default_center_x,default_center_y,default_size_xy,default_max_iter);
}

void MandelDrawer::reset(double center_re,double center_im,double size_re_im,
                         unsigned int max_iter) {
  if (center_x != center_re ||
      center_y != center_im) {
    center_x = center_re;
    center_y = center_im;
    new_parameters = true;
  }
  if (size_xy != size_re_im) {
    size_xy = size_re_im;
    new_parameters = true;
    new_size_xy = true;
  }
  setMaxIter(max_iter);
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

unsigned int MandelDrawer::getMaxIter(void) const {
  return max_iter;
}

void MandelDrawer::sizeChanged(int w,int h) {
  if (w != width || h != height) {
    width = w;
    height = h;
    new_parameters = true;
    new_size_xy = true;
  }
}

void MandelDrawer::setMaxIter(unsigned int n) {
  if (n > 0xFFFFFF) {
    n = 0xFFFFFF;
  } else if (n < 8) {
    n = 8;
  }
  if (max_iter != n) {
    max_iter = n;
    new_max_iter = true;
  }
}

void MandelDrawer::startRecalc(void) {
  new_parameters = true;
}

void MandelDrawer::XYToReIm(const Vector<float,2> &screen_pos,
                            Vector<double,2> &re_im_pos) const {
  const double d_re_im = XYToReImScale();
  re_im_pos[0] = center_x + d_re_im * (screen_pos[0] - 0.5*width);
  re_im_pos[1] = center_y + d_re_im * (height-1-screen_pos[1] - 0.5*height);
}

double MandelDrawer::XYToReImScale(void) const {
  return size_xy / ((width<height)?width:height);
}

void MandelDrawer::fitReIm(const Vector<float,2> &screen_pos,
                           const Vector<double,2> &re_im_pos) {
  const double d_re_im = XYToReImScale();
  center_x = re_im_pos[0] - d_re_im * (screen_pos[0] - 0.5*width); 
  center_y = re_im_pos[1] - d_re_im * (height-1-screen_pos[1] - 0.5*height);
  double h = center_x*center_x+center_y*center_y;
  if (h > 4.0) {
    h = 2.0/sqrt(h);
    center_x *= h;
    center_y *= h;
  }
  center_x = d_re_im * floor(0.5 + center_x / d_re_im);
  center_y = d_re_im * floor(0.5 + center_y / d_re_im);
}

void MandelDrawer::fitReIm(const Vector<float,2> &screen_pos,
                           const Vector<double,2> &re_im_pos,
                           double scale) {
  if (scale < 1e-16) scale = 1e-16;
  scale *= ((width<height)?width:height);
  if (scale > 4.0) scale = 4.0;
  if (size_xy != scale) {
    size_xy = scale;
    new_size_xy = true;
  }
  fitReIm(screen_pos,re_im_pos);
}

void MandelDrawer::getOpenGLScreenCoordinates(float coor[8]) const {
  const float scale = XYToReImScale();
  const float factor =  image->d_re_im / scale;

  coor[0] = 2.f*(image->start_re-center_x)/(width*scale);
  coor[1] = 2.f*(image->start_im-center_y)/(height*scale);
  coor[2] = coor[0]+2.f*factor;
  coor[5] = coor[1]+2.f*factor;

  coor[3] = coor[1];
  coor[4] = coor[0];
  coor[6] = coor[2];
  coor[7] = coor[5];

//cout << "MandelDrawer::getOpenGLScreenCoordinates: "
//     << coor[0] << '/' << coor[1] << ";  "
//     << coor[2] << '/' << coor[5] << endl;
}

float MandelDrawer::getProgress(void) const {
  return image->pixel_count / (float)(width*height);
}

bool MandelDrawer::step(void) {
//cout << "step: start" << endl;
  bool rval = true;
  if (new_parameters || new_max_iter) {
    __sync_synchronize();

    threads->cancelExecution();
    image->pixel_count = 0;

    if (new_size_xy) {
      image->recalc_limit = 0;
    } else {
      image->recalc_limit = image->max_iter;
    }
    image->max_iter = max_iter;

//cout << "step: (re)starting work: new_size_xy:" << new_size_xy
//     << ", recalc_limit:" << image->recalc_limit
//     << ", max_iter:" << image->max_iter
//     << endl;

    { // re-scale MandelImage
      float coor[8];
      getOpenGLScreenCoordinates(coor);
      unsigned int *const new_data = new unsigned int[width*height];
      unsigned int *nd = new_data;
      for (int y=0;y<height;y++,nd+=width) {
        double y_gl = (y+0.5)*(2.0/height) - 1.0;
        if (y_gl < coor[1] || y_gl >= coor[5]) {
          for (int x=0;x<width;x++) {
            nd[x] = 0x80000000;
          }
        } else {
          const int yb = (int)floor(height * (y_gl - coor[1])
                                        / (coor[5] - coor[1]));
          for (int x=0;x<width;x++) {
            double x_gl = (x+0.5)*(2.0/width) - 1.0;
            if (x_gl < coor[0] || x_gl >= coor[2]) {
              nd[x] = 0x80000000;
            } else {
              const int xb = (int)floor(width * (x_gl - coor[0])
                                           / (coor[2] - coor[0]));
              if (new_size_xy) {
                nd[x] = image->data[yb*(image->screen_width)+xb] | 0x80000000;
              } else {
                nd[x] = image->data[yb*(image->screen_width)+xb];
                if (!image->needRecalc(nd[x])) image->pixel_count++;
              }
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
    new_size_xy = false;

    image->d_re_im = XYToReImScale();
    image->start_re = center_x - 0.5*image->d_re_im * width;
    image->start_im = center_y - 0.5*image->d_re_im * height;

    threads->startExecution(new MainJob(*image,width,height));
//cout << "step: startExecution: new MainJob queued" << endl;
    was_working_last_time = true;
    new_parameters = false;
    new_max_iter = false;
  } else {
    if (was_working_last_time) {
      if (threads->workIsFinished()) {
        const float progress = getProgress();
        if (progress != 1.f) {
          ABORT();
        }
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
