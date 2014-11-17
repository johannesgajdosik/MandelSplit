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

#include <math.h>

#include <unistd.h> // sysconf

#include <fstream>

static const double default_center_x = -0.75;
static const double default_center_y = 0;
static const double default_size_xy = 2.5;
static const int default_max_iter = 512;

static int GetNrOfProcessors(void) {
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
              was_working_last_time(false) {
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

int MandelDrawer::getMaxIter(void) const {
  return max_iter;
}

void MandelDrawer::reset(void) {
  if (center_x != default_center_x ||
      center_y != default_center_y ||
      size_xy != default_size_xy) {
    center_x = default_center_x;
    center_y = default_center_y;
    size_xy = default_size_xy;
  }
  setMaxIter(default_max_iter);
}

void MandelDrawer::reset(double center_re,double center_im,double size_re_im,
                         int max_iter) {
  if (center_x != center_re ||
      center_y != center_im ||
      size_xy != size_re_im) {
    center_x = center_re;
    center_y = center_im;
    size_xy = size_re_im;
  }
  setMaxIter(max_iter);
}

void MandelDrawer::setMaxIter(int n) {
  if (n > 0x10000) {
    if (n > 0x100000) {
      n = 0x100000;
    } else {
      n = n - (n&0xFFFF);
    }
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
}

void MandelDrawer::fitReIm(const Vector<float,2> &screen_pos,
                           const Vector<double,2> &re_im_pos,
                           double scale) {
  if (scale < 1e-16) scale = 1e-16;
  size_xy = scale * ((width<height)?width:height);
  if (size_xy > 4.0) size_xy = 4.0;
  fitReIm(screen_pos,re_im_pos);
}

void MandelDrawer::getOpenGLScreenCoordinates(float coor[8]) const {
/*
    texture:
  image->start_re
  image->start_im
  image->d_re_im
    should be:
  re_im_pos,d_re_im = XYToReImScale();

wohin am Bildschirm soll (image->start_re und
  image->start_re+{width,height}*image->d_re_im

(image->start_re-center_x)/XYToReImScale() + 0.5*width= left
(image->start_re+width*image->d_re_im-center_x)/XYToReImScale() + 0.5*width= right
coor[0] = 2*left/width-1 = 2*(image->start_re-center_x)/(width*XYToReImScale())
coor[2] = 2*right/width-1 = 2*(image->start_re+width*image->d_re_im-center_x)/(width*XYToReImScale())
coor[2]-coor[0] = 2*image->d_re_im/XYToReImScale()
*/

  const float factor =  image->d_re_im / XYToReImScale();

  coor[0] = 2.f*(image->start_re-center_x)/(width*XYToReImScale());
  coor[1] = 2.f*(image->start_im-center_y)/(height*XYToReImScale());
  coor[2] = coor[0]+2.f*factor;
  coor[5] = coor[1]+2.f*factor;

  coor[3] = coor[1];
  coor[4] = coor[0];
  coor[6] = coor[2];
  coor[7] = coor[5];
//cout << "MandelDrawer::getOpenGLScreenCoordinates: "
//        "image start: " << image->start_re << '/' << image->start_im << endl;
  
//cout << "MandelDrawer::getOpenGLScreenCoordinates: "
//     << coor[0] << '/' << coor[1] << ";  "
//     << coor[6] << '/' << coor[7] << endl;
}

static
void HSV2RGB(int h,int s,int v,unsigned char bgr[3]) {
  int r,g,b;
  h = h % (6*255);
  if (h < 0) h += 6*255;
  if (h < 256) {
    g = h;
    b = 255;
    r = 0;
  } else if ((h-=255) < 256) {
    g = 255;
    b = 255-h;
    r = 0;
  } else if ((h-=255) < 256) {
    r = h;
    g = 255;
    b = 0;
  } else if ((h-=255) < 256) {
    r = 255;
    g = 255-h;
    b = 0;
  } else if ((h-=255) < 256) {
    b = h;
    r = 255;
    g = 0;
  } else {
    h -= 255;
    b = 255;
    r = 255-h;
    g = 0;
  }
  bgr[0] = ((r*s)/255 + (255-s))*v/255;
  bgr[1] = ((g*s)/255 + (255-s))*v/255;
  bgr[2] = ((b*s)/255 + (255-s))*v/255;
}


static double H(double x) {
  return x;
  return x*x*(3.0-2.0*x);
}

static double Blue(double x) {
  return H(x);
}

static double Green(double x) {
  if (x < 0.4) return H(x*(1.0/0.4));
  if (x < 0.9) return 1.0-H((x-0.4)*(1.0/(0.9-0.4)));
  return H((x-0.9)*(1.0/(1.0-0.9)));
}

static double Red(double x) {
  if (x < 0.2) return H(x*(1.0/0.2));
  if (x < 0.7) return 1.0-H((x-0.2)*(1.0/(0.7-0.2)));
  return H((x-0.7)*(1.0/(1.0-0.7)));
}

void CheckGlError(const char *description);

void InitBlueStripes(const unsigned char r,const unsigned char g,
                     const int count,unsigned char *colors) {
    // choose count blue values from 0 to 255
  const int blue_increment = 255 / (count-1);
  int blue_remainder = 255 - blue_increment*(count-1);
  int blue = 0;
  for (int i=0;i<count;i++) {
    colors[0] = r;
    colors[1] = g;
    colors[2] = blue;
    colors += 3;
    blue += blue_increment;
    if (blue_remainder > 0) {
      blue++;
      blue_remainder--;
    }
  }
}

void InitGreenStripes(const unsigned char r,
                      const int count,unsigned char *colors) {
//  cout << "InitGreenStripes(" << ((int)r) << ',' << count << ')' << endl;
  const int nr_of_blue_stripes = (int)floor(sqrt(count));
  int blue_size = count / nr_of_blue_stripes;
  int blue_remainder = count - blue_size*nr_of_blue_stripes;
  const int green_increment = 255 / (nr_of_blue_stripes-1);
  int green_remainder = 255 - green_increment*(nr_of_blue_stripes-1);
  int green = 0;
  for (int i=0;i<nr_of_blue_stripes;i++) {
    int blue_count = blue_size;
    if (blue_remainder > 0) {
      blue_remainder--;
      blue_count++;
    }
    InitBlueStripes(r,green,blue_count,colors);
    colors += 3*blue_count;
    green += green_increment;
    if (green_remainder > 0) {
      green_remainder--;
      green++;
    }
  }
//  cout << "InitGreenStripes end" << endl;
}

void InitRedStripes(const int count,
                    unsigned char *colors) {
  const double h = cbrt(count);
  const int nr_of_green_stripes = (int)floor(sqrt(h*h));
  int green_size = count / nr_of_green_stripes;
  int green_remainder = count - green_size*nr_of_green_stripes;
  const int red_increment = 255 / (nr_of_green_stripes-1);
  int red_remainder = 255 - red_increment*(nr_of_green_stripes-1);
  int red = 0;
  for (int i=0;i<nr_of_green_stripes;i++) {
    int green_count = green_size;
    if (green_remainder > 0) {
      green_remainder--;
      green_count++;
    }
    InitGreenStripes(red,green_count,colors);
    colors += 3*green_count;
    red += red_increment;
    if (red_remainder > 0) {
      red_remainder--;
      red++;
    }
  }
//  cout << "InitRedStripes end" << endl;
}


void InitColors(int max,unsigned char colors[256*256*3]) {
  if (max > 256*256) max = 256*256;
  InitRedStripes(max,colors);
  memset(colors+3*max,0,3*(256*256-max));
}

bool MandelDrawer::step(void) {
//cout << "step: start" << endl;
  bool rval = true;
  if (new_parameters) {
    __sync_synchronize();

//cout << "step: (re)starting work" << endl;
    threads->cancelExecution();

    { // re-scale MandelImage
      float coor[8];
      getOpenGLScreenCoordinates(coor);
      unsigned int *new_data = new unsigned int[width*height];
      for (int y=0;y<height;y++) {
        double y_gl = (y+0.5)*(2.0/height) - 1.0;
        if (y_gl < coor[1] || y_gl > coor[5]) {
          memset(new_data+y*width,0,width*sizeof(unsigned int));
        } else {
          const int yb = (int)floor(0.5+(height-1) * (y_gl-coor[1])
                                         / (coor[5] - coor[1]));
          for (int x=0;x<width;x++) {
            double x_gl = (x+0.5)*(2.0/width) - 1.0;
            if (x_gl < coor[0] || x_gl > coor[2]) {
              new_data[y*width+x] = 0;
            } else {
              const int xb = (int)floor(0.5+(width-1) * (x_gl-coor[0])
                                             / (coor[2] - coor[0]));
              new_data[y*width+x] = image->data[yb*width+xb];
            }
          }
        }
      }
//cout << "step: changing buffer" << endl;
      delete[] image->data;
      image->data = new_data;
      CheckGlError("before glTexSubImage2D 0");
      glActiveTexture(GL_TEXTURE0);
      glTexSubImage2D(GL_TEXTURE_2D,0,
                      0,0,width,height,
                      GL_RGBA,GL_UNSIGNED_BYTE,
                      image->data);
      CheckGlError("after glTexSubImage2D 0");
    }


    image->d_re_im = size_xy / ((width<height)?width:height);
    image->start_re = center_x - 0.5*image->d_re_im * width;
    image->start_im = center_y - 0.5*image->d_re_im * height;

    image->max_iter = max_iter;
    if (new_max_iter) {
//cout << "step: must recalc lookup texture" << endl;
        // recalc lookup image
      glActiveTexture(GL_TEXTURE1);
      {
        unsigned char *pixels = new unsigned char[256*256*3];
        InitColors(max_iter,pixels);
/*
        unsigned char *p = pixels;
        int max = max_iter;
        if (max > 0x10000) max = 0x10000;
  //      const double log_scale = 1.0/log(max-1);
        for (int i=0;i<256*256;i++) {
          if (i < max) {
            InitColor(i/(double)(max-1),p);
            p += 3;

//            *p++ = 17 * ((i>>8)&15);
//            *p++ = 17 * ((i>>4)&15);
//            *p++ = 17 * (i&15);

//            *p++ = 36 * ((i>>6)&7);
//            *p++ = 36 * ((i>>3)&7);
//            *p++ = 36 * (i&7);


//            double x = (1+i)/(double)max;
//            HSV2RGB(i,255,255*sqrt(x),p);
//            p += 3;

//            double x = log(1+i)*log_scale; // 0<=x<=1
//            double x = (1+i)/(double)max;
//            x = x*(2.0-x);
//cout << i << '(' << x << "): " << Blue(x) << ", " << Green(x) << ", " << Red(x) << endl;

//            *p++ = (int)(0.5+255.0*Blue(x));
//            *p++ = (int)(0.5+255.0*Green(x));
//            *p++ = (int)(0.5+255.0*Red(x));

          } else {
            *p++ = 0;
            *p++ = 0;
            *p++ = 0;
          }
        }
*/
        CheckGlError("before glTexSubImage2D 1");
        glTexSubImage2D(GL_TEXTURE_2D,0,
                        0,0,256,256,
                        GL_RGB,GL_UNSIGNED_BYTE,pixels);
        CheckGlError("after glTexSubImage2D 1");
        delete[] pixels;
      }
      glActiveTexture(GL_TEXTURE0);
//cout << "step: lookup texture ok" << endl;
    }
    threads->startExecution(new MainJob(*threads,*image,width,height));
//cout << "step: startExecution: new MainJob queued" << endl;
    was_working_last_time = true;
    new_parameters = false;
    new_max_iter = false;
  } else {
    if (was_working_last_time) {
      if (threads->workIsFinished()) {
//cout << "step: work finished" << endl;
        was_working_last_time = false;
      } else {
//cout << "step: still working" << endl;
      }
      CheckGlError("before glTexSubImage2D 2");
      glActiveTexture(GL_TEXTURE0);
      
        // TODO: unite jobs and add currently executing jobs
        // GL_EXT_unpack_subimage seems to be not supported
//      glPixelStorei(GL_UNPACK_ROW_LENGTH,image->screen_width);

//glPixelStorei(GL_UNPACK_ALIGNMENT,4);

      threads->draw(image);

//      glTexSubImage2D(GL_TEXTURE_2D,0,
//                      0,0,width,height,
//                      GL_RGBA,GL_UNSIGNED_BYTE,
//                      image->data);
//      glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
      CheckGlError("after glTexSubImage2D 2");
    } else {
      rval = false;
    }
  }
//cout << "step: returns " << rval << endl;
  return rval;
}
