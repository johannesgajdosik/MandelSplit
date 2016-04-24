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

#ifdef __ANDROID__
extern int opengl_version;
#endif

#define MY_GL_UNPACK_ROW_LENGTH 0x0CF2

#include <unistd.h> // sysconf

#include <fstream>

void CheckGlError(const char *description);

static const Complex<FLOAT_TYPE> default_center(-0.75,0.0);
static const Complex<FLOAT_TYPE> default_unity_pixel(1.0/256,0.0);
static const int default_max_iter = 512;

static const FLOAT_TYPE half(0.5);
static const FLOAT_TYPE zero(0.0);


static
int GetNrOfProcessors(void) {
  {
//return 1;
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


static inline FLOAT_TYPE InitMpfBin(const void *&p) {
  __mpf_struct h;
  memcpy(&h,p,offsetof(__mpf_struct,_mp_d));
  p = ((char*)p) + offsetof(__mpf_struct,_mp_d);
  h._mp_d = (mp_limb_t*)p;
  p = ((char*)p) + sizeof(mp_limb_t)*abs(h._mp_size);
  return FLOAT_TYPE(&h);
}

void MandelDrawer::Parameters::updatePrecision(void) {
  const int bits = 2-(int)floor(0.5*ln2(unity_pixel.length2()));
  const int new_precision = (bits <= 53)
                          ? 0
                          : (((8*sizeof(mp_limb_t)-1)+bits)
                            / (8*sizeof(mp_limb_t)));
  if (precision != new_precision) {
    cout << "MandelDrawer::Parameters::updatePrecision: "
            "changing precision from "
         << precision << " to " << new_precision << endl;
    precision = new_precision;
  }
}

MandelDrawer::Parameters::Parameters(const void *&p) : precision(0) {
  FLOAT_TYPE c_r(InitMpfBin(p));
  FLOAT_TYPE c_i(InitMpfBin(p));
  FLOAT_TYPE u_r(InitMpfBin(p));
  FLOAT_TYPE u_i(InitMpfBin(p));
  center.re.set_prec(c_r.get_prec());
  center.re = c_r;
  center.im.set_prec(c_i.get_prec());
  center.im = c_i;
  unity_pixel.re.set_prec(u_r.get_prec());
  unity_pixel.re = u_r;
  unity_pixel.im.set_prec(u_i.get_prec());
  unity_pixel.im = u_i;
  memcpy(&max_iter,p,sizeof(unsigned int));p=((char*)p)+sizeof(unsigned int);
  updatePrecision();
}

static inline size_t GetMpfBinSize(const mpf_t x) {
  return offsetof(__mpf_struct,_mp_d) + sizeof(mp_limb_t)*abs(x->_mp_size);
}

static inline void DumpMpfBin(void *&p,const mpf_t x) {
  memcpy(p,x,offsetof(__mpf_struct,_mp_d));
  p = ((char*)p) + offsetof(__mpf_struct,_mp_d);
  const unsigned int s = sizeof(mp_limb_t)*abs(x->_mp_size);
  memcpy(p,x->_mp_d,s);
  p = ((char*)p) + s;
}

size_t MandelDrawer::Parameters::getSavedStateSize(void) {
  return GetMpfBinSize(center.re.get_mpf_t())
       + GetMpfBinSize(center.im.get_mpf_t())
       + GetMpfBinSize(unity_pixel.re.get_mpf_t())
       + GetMpfBinSize(unity_pixel.im.get_mpf_t())
       + sizeof(unsigned int);
}

void MandelDrawer::Parameters::dumpSaveState(void *&p) const {
  DumpMpfBin(p,center.re.get_mpf_t());
  DumpMpfBin(p,center.im.get_mpf_t());
  DumpMpfBin(p,unity_pixel.re.get_mpf_t());
  DumpMpfBin(p,unity_pixel.im.get_mpf_t());
  memcpy(p,&max_iter,sizeof(unsigned int));p=((char*)p)+sizeof(unsigned int);
}


MandelDrawer::MandelDrawer(void)
             :threads(new ThreadPool(GetNrOfProcessors())),
              image(0),width(0),height(0),
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
  params.precision = p.precision;
}

void MandelDrawer::getParameters(Parameters &p) const {
  MutexLock lock(mutex);
  p = params;
}

void MandelDrawer::initialize(int width,int height,
                              int screen_width,int screen_height) {
  if (MandelDrawer::width != width ||
      MandelDrawer::height != height) {
    MandelDrawer::width = width;
    MandelDrawer::height = height;
    if (image) delete image;
    image = new MandelImage(screen_width,screen_height,
                            *threads,
                            threads->terminate_flag,
                            threads->getNrOfWaitingThreads());
    parameters_changed = true;
    max_iter_changed = true;
    unity_pixel_changed = true;
    was_working_last_time = false;
  }
}

MandelDrawer::~MandelDrawer(void) {
  delete threads;
  delete image;
}

void *MandelDrawer::getImageData(void) const {
  return image->getData();
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

//int MandelDrawer::setPrecisionPrivate(int nr_of_limbs) {
//cout << "MandelDrawer::setPrecisionPrivate(" << nr_of_limbs
//     << ") old: " << params.precision << endl;
//  const int rval = params.precision;
//  if (rval != nr_of_limbs) {
//    params.setPrecision(nr_of_limbs);
//    parameters_changed = true;
//    unity_pixel_changed = true;
//  }
//  return rval;
//}

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
//  cout << "MandelDrawer::minimizeMaxIter: findGreatestValueNotMax: " << better_max << endl;
  int highest_bit = 0;
  for (int i=better_max+1;i>1;i>>=1) {highest_bit++;}
  if (highest_bit > 9) {
    highest_bit -= 9;
    better_max = (better_max+(1<<highest_bit))
               & ((~0)<<highest_bit);
  } else {
    better_max++;
  }
//  cout << "MandelDrawer::minimizeMaxIter: better_max: " << better_max << endl;
  setMaxIter(better_max);
  return better_max;
}

void MandelDrawer::startRecalc(void) {
  parameters_changed = true;
}

//const Vector<float,2> MandelDrawer::ReImToXY(
//                        const Complex<FLOAT_TYPE> &re_im_pos) const {
//  const Complex<float> tmp((re_im_pos - center)/unity_pixel);
//  return Vector<float,2>(tmp.re + 0.5f*width,
//                         0.5f*height - 1.f - tmp.im);
//}

void MandelDrawer::XYToReImPrivate(const Vector<float,2> &screen_pos,
                                   Complex<FLOAT_TYPE> &re_im_pos) const {
  re_im_pos = params.unity_pixel;
  re_im_pos *= Complex<FLOAT_TYPE>(screen_pos[0] - 0.5f*width,
                                   0.5f*height-1.f-screen_pos[1]);
  re_im_pos += params.center;
}


void MandelDrawer::fitReset(void) {
  MutexLock lock(mutex);
  if (image->getPrecision() <= 0) {
    params.unity_pixel = image->getDReIm();
    params.center = image->getStart();
  } else {
    params.unity_pixel.re = image->getDRe().convert2ToMpf(image->getPrecision());
    params.unity_pixel.im = image->getDIm().convert2ToMpf(image->getPrecision());
    params.center.re = image->getStartRe().convert2ToMpf(image->getPrecision());
    params.center.im = image->getStartIm().convert2ToMpf(image->getPrecision());
  }
  params.center += params.unity_pixel * Complex<FLOAT_TYPE>(0.5*width,0.5*height);
}

void MandelDrawer::fitReImPrivate(const Vector<float,2> &screen_pos,
                                  const Complex<FLOAT_TYPE> &re_im_pos) {
  params.center = re_im_pos
                - params.unity_pixel
                * Complex<FLOAT_TYPE>(screen_pos[0] - 0.5f*width,
                                      0.5f*height-1.f-screen_pos[1]);
  {
    FLOAT_TYPE h = params.center.length2();
    if (h > (unsigned long)1) {
      params.center *= (((unsigned long)1)/sqrt(h));
    }
  }

  params.center /= params.unity_pixel;
  params.center.re = floor(half + params.center.re);
  params.center.im = floor(half + params.center.im);
  params.center *= params.unity_pixel;

//  cout << "fitReIm(" << screen_pos
//                     << " -> " << re_im_pos
//                     << "): " << params.center
//                     << ';' << params.unity_pixel << endl;
  setPriorityPointPrivate(screen_pos);
}

const FLOAT_TYPE epsilon = 4.44e-16; // 2^-51
const FLOAT_TYPE epsilon2 = epsilon*epsilon;

int MandelDrawer::fitReIm(const Vector<float,2> &screen_pos0,
                          const Vector<float,2> &screen_pos1,
                          Complex<FLOAT_TYPE> &re_im_pos0,
                          Complex<FLOAT_TYPE> &re_im_pos1,
                          bool enable_rotation) {
  MutexLock lock(mutex);
//  cout << "fitReIm(" << screen_pos0 << ',' << screen_pos1
//                     << " -> " << re_im_pos0 << ',' << re_im_pos1
//                     << "): old: " << params.center
//                     << ';' << params.unity_pixel;
  const Complex<FLOAT_TYPE> screen_diff(screen_pos1[0]-screen_pos0[0],
                                        screen_pos0[1]-screen_pos1[1]);
  const Complex<FLOAT_TYPE> re_im_diff(re_im_pos1-re_im_pos0);
  const FLOAT_TYPE old_length2 = params.unity_pixel.length2();
  Complex<FLOAT_TYPE> new_unity_pixel;
  if (enable_rotation) {
    new_unity_pixel = re_im_diff / screen_diff;
  } else {
    new_unity_pixel = params.unity_pixel
                    * sqrt(re_im_diff.length2()
                            / (screen_diff.length2()*old_length2));
  }


  FLOAT_TYPE length2(new_unity_pixel.length2());
  const FLOAT_TYPE max_length(2.0/((width<height)?width:height));
  if (length2 > max_length*max_length) {
    new_unity_pixel *= (max_length/sqrt(length2));
    length2 = max_length*max_length;
  }


///  if ( (params.unity_pixel-new_unity_pixel).length2() > epsilon2*length2) {
    params.unity_pixel = new_unity_pixel;

    const int bits = 2-(int)floor(0.5*ln2(length2));
    params.precision = 0;
    if (bits > 53) {
      params.precision = ((8*sizeof(mp_limb_t)-1)+bits)
                        / (8*sizeof(mp_limb_t));
    }
//    if (enable_rotation || old_length2 != length2) {
      unity_pixel_changed = true;
//    }
///  }
  const Vector<float,2> screen_pos(0.5f*(screen_pos0+screen_pos1));
  const Complex<FLOAT_TYPE> re_im_pos(mul_2exp(re_im_pos0.re+re_im_pos1.re,-1),
                                      mul_2exp(re_im_pos0.im+re_im_pos1.im,-1));
  fitReImPrivate(screen_pos,re_im_pos);
//  cout << " ,new: " << params.center
//       << ';' << params.unity_pixel << endl;
  if (!enable_rotation) {
    // recalculate re_im_pos0, re_im_pos1:
    XYToReImPrivate(screen_pos0,re_im_pos0);
    XYToReImPrivate(screen_pos1,re_im_pos1);
  }
  return params.precision;
}

bool MandelDrawer::disableRotation(void) {
  MutexLock lock(mutex);
  if (image->getPrecision() <= 0) {
    if (params.unity_pixel.im != zero ||
        params.unity_pixel.re < zero) {
      params.unity_pixel.re = sqrt(params.unity_pixel.length2());
//      params.unity_pixel.im = zero.copy();
      params.unity_pixel.im = zero;
      unity_pixel_changed = true;
      return true;
    }
  }
  return true;
}

void MandelDrawer::getOpenGLScreenCoordinate(unsigned int i,unsigned int j,
                                             float coor[2]) const {
  Complex<FLOAT_TYPE> A;
  if (image->getPrecision() <= 0) {
    Complex<double> tmp(i,j);
    tmp *= image->getDReIm();
    tmp += image->getStart();
    A.re = FLOAT_TYPE(tmp.re);
    A.im = FLOAT_TYPE(tmp.im);
  } else {
    mp_limb_t mem_re[image->getPrecision()+2];
    mp_limb_t mem_im[image->getPrecision()+2];
    GmpFixedPointExternalMem re(mem_re);
    GmpFixedPointExternalMem im(mem_im);
      // one extra limb so that there is no carry/borrow:
    re.p[image->getPrecision()+1]
      = re.linCombMinusU1(image->getDRe(),i,image->getDIm(),j);
    im.p[image->getPrecision()+1]
      = im.linCombPlusU1(image->getDRe(),j,image->getDIm(),i);
    re.add2(image->getStartRe());
    im.add2(image->getStartIm());
    A.re = FLOAT_TYPE(re.convert2ToMpf(image->getPrecision()));
    A.im = FLOAT_TYPE(im.convert2ToMpf(image->getPrecision()));
  }
   // A has now coordinates in Mandelbrot set
  A -= params.center;
//cout << "getOpenGLScreenCoordinate: dividing by " << params.unity_pixel << endl;
  A /= params.unity_pixel;
  coor[0] = (A.re.get_d()*2.0) / width;
  coor[1] = (A.im.get_d()*2.0) / height;
}

//void MandelDrawer::getOpenGLScreenCoordinates(float coor[8],
//                                              Parameters &p) const {
/* The existing image was computed with
    image->getDReIm() = unity_pixel;
    image->getStart() = center - 0.5*unity_pixel * (width+sqrt(-1)*height);
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
    A = image->getStart()+(i,j)*image->getDReIm();
  This point A shall now be displayed on
    ReImToXY(A)
  So it needs the OpenGl coordinates
    Complex<float> tmp = ((A - center)/unity_pixel) scaled by [w/h,1]
*/
//  MutexLock lock(mutex);
//  getOpenGLScreenCoordinate(0,0,coor+0);
//  getOpenGLScreenCoordinate(width,0,coor+2);
//  getOpenGLScreenCoordinate(0,height,coor+4);
//  getOpenGLScreenCoordinate(width,height,coor+6);
//  p = params;

////cout << "MandelDrawer::getOpenGLScreenCoordinates: "
////     << coor[0] << '/' << coor[1] << ";  "
////     << coor[6] << '/' << coor[7] << endl;
//}



void MandelDrawer::setPriorityPoint(const Vector<float,2> screen_pos) {
  const int xy[2] = {
    (int)floorf(0.5f+screen_pos[0]),
    height-1-(int)floorf(0.5f+screen_pos[1])
  };
  threads->startPause();
  threads->sortJobqueue(xy);
  image->setPriorityPoint(xy[0],xy[1]);
  threads->finishPause();
}

void MandelDrawer::setPriorityPointPrivate(const Vector<float,2> screen_pos) {
  const int xy[2] = {
    (int)floorf(0.5f+screen_pos[0]),
    height-1-(int)floorf(0.5f+screen_pos[1])
  };
  image->setPriorityPoint(xy[0],xy[1]);
}


float MandelDrawer::getProgress(void) const {
  return image->pixel_count / (float)(width*height);
}

bool MandelDrawer::step(float coor[8],Parameters &params_copy) {
//cout << "step: start" << endl;
  bool rval = true;
  if (parameters_changed || max_iter_changed) {
    __sync_synchronize();

    threads->cancelExecution();
    image->pixel_count = 0;

    mutex.lock();
    params_copy = params;
    if (image->getPrecision() != params.precision) {
      cout << "MandelDrawer::step: changing image precision from "
           << image->getPrecision() << " to " << params.precision << endl;
      params.setPrecision(params.precision);
      image->setPrecision(params.precision);
      GmpFixedPointLockfree::changeNrOfLimbs(params.precision);
    }
    if (unity_pixel_changed) {
      image->setRecalcLimit(0);
//      image->setPriorityPoint(-1,-1);
    } else {
      image->setRecalcLimit(image->getMaxIter());
    }
    image->setMaxIter(params.max_iter);

//cout << "step: (re)starting work: unity_pixel_changed:" << unity_pixel_changed
//     << ", recalc_limit:" << image->getRecalcLimit()
//     << ", max_iter:" << image->getMaxIter()
//     << ", precision:" << image->getPrecision()
//     << endl;

    { // re-scale MandelImage
      Complex<double> K,D;
      Complex<FLOAT_TYPE> Kh;
      Complex<FLOAT_TYPE> Dh(params.center);
      if (image->getPrecision() <= 0) {
        Kh.re = FLOAT_TYPE(image->getDReIm().re);
        Kh.im = FLOAT_TYPE(image->getDReIm().im);
        Dh.re -= FLOAT_TYPE(image->getStart().re);
        Dh.im -= FLOAT_TYPE(image->getStart().im);
      } else {
        Kh.re = FLOAT_TYPE(image->getDRe().convert2ToMpf(image->getPrecision()));
        Kh.im = FLOAT_TYPE(image->getDIm().convert2ToMpf(image->getPrecision()));
        Dh.re -= FLOAT_TYPE(image->getStartRe().convert2ToMpf(image->getPrecision()));
        Dh.im -= FLOAT_TYPE(image->getStartIm().convert2ToMpf(image->getPrecision()));
      }
      const FLOAT_TYPE l2(Kh.length2());
      Kh.invert();
      Dh *= Kh;
      Kh *= params.unity_pixel;
      K.re = Kh.re.get_d();
      K.im = Kh.im.get_d();
      D.re = Dh.re.get_d();
      D.im = Dh.im.get_d();
      image->setDReIm(params.unity_pixel);
      Dh = params.unity_pixel * Complex<FLOAT_TYPE>(-0.5*width,-0.5*height)
         + params.center;
      image->setStart(Dh);
   
      getOpenGLScreenCoordinate(0,0,coor+0);
      getOpenGLScreenCoordinate(width,0,coor+2);
      getOpenGLScreenCoordinate(0,height,coor+4);
      getOpenGLScreenCoordinate(width,height,coor+6);

      mutex.unlock();
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
              nd[x] = image->getData()[yb*(image->getScreenWidth())+xb] | 0x80000000;
            } else {
              nd[x] = image->getData()[yb*(image->getScreenWidth())+xb];
              if (!image->needRecalc(nd[x])) image->pixel_count++;
            }
          }
        }
      }
      for (int y=0;y<height;y++) {
        memcpy(image->getData() + y*(image->getScreenWidth()),new_data+y*width,
               sizeof(unsigned int)*width);
      }
      delete[] new_data;

//cout << "step: buffer rescaled" << endl;
      CheckGlError("before glTexSubImage2D 0");
#ifdef __ANDROID__
      if (opengl_version > 2)
#endif
        glPixelStorei(MY_GL_UNPACK_ROW_LENGTH,image->getScreenWidth());
      glTexSubImage2D(GL_TEXTURE_2D,0,
                      0,0,width,height,
                      GL_RGBA,GL_UNSIGNED_BYTE,
                      image->getData());
      CheckGlError("after glTexSubImage2D 0");
    }
    unity_pixel_changed = false;

//cout << "step: startExecution: queueing new MainJob" << endl;
    threads->startExecution(MainJob::create(*image,width,height));
//cout << "step: startExecution: new MainJob queued, precision "
//     << image->getPrecision() << endl;
    was_working_last_time = true;
    parameters_changed = false;
    max_iter_changed = false;
  } else {
    {
      MutexLock lock(mutex);
      getOpenGLScreenCoordinate(0,0,coor+0);
      getOpenGLScreenCoordinate(width,0,coor+2);
      getOpenGLScreenCoordinate(0,height,coor+4);
      getOpenGLScreenCoordinate(width,height,coor+6);
      params_copy = params;
    }
    if (was_working_last_time) {
      if (threads->workIsFinished()) {
        const float progress = getProgress();
        if (progress != 1.f) {
//          ABORT();
        }
///MutexLock lock(mutex);
///params.max_iter = image->getMaxIter();





//        cout << "step: work finished" << endl;
        was_working_last_time = false;
      } else {
//        cout << "step: still working" << endl;
      }
      CheckGlError("before glTexSubImage2D 2");
      
        // TODO: unite jobs and add currently executing jobs
        // GL_EXT_unpack_subimage seems to be not supported
#ifdef __ANDROID__
      if (opengl_version > 2)
#endif
        glPixelStorei(MY_GL_UNPACK_ROW_LENGTH,image->getScreenWidth());
      threads->draw(image);

//      glTexSubImage2D(GL_TEXTURE_2D,0,
//                      0,0,width,height,
//                      GL_RGBA,GL_UNSIGNED_BYTE,
//                      image->getData());
      CheckGlError("after glTexSubImage2D 2");
    } else {
      rval = false;
    }
  }
//cout << "step: returns " << rval << endl;
  return rval;
}


std::ostream &operator<<(std::ostream &o,
                         const MandelDrawer::Parameters &p) {
  const std::streamsize last_prec = o.precision();
  const std::ios_base::fmtflags last_ff = o.flags();
  o.flags((last_ff & (~std::ios_base::scientific)) | std::ios_base::fixed);
  const int prec = -(int)floor(0.150514997831991*ln2(p.unity_pixel.length2()));
  o.precision(prec+2);
  o << '[' << p.center;
  o.flags((last_ff & (~std::ios_base::fixed)) | std::ios_base::scientific);
  o.precision(9);
  o << ',' << p.unity_pixel;
  o.flags(last_ff);
  o.precision(last_prec);
  o << ',' << p.max_iter << ']';
  return o;
}

/*
TODO
std::istream &operator>>(std::istream &i,
                         MandelDrawer::Parameters &p) {
  char ch;
  if (!(i >> ch)) return i;
  if (ch != '[') {
    i.setstate(std::ios::failbit|std::ios::badbit);
    return i;
  }
  if (!(i >> p.precision)) return i;
  if (!(i >> ch)) return i;
  if (ch != ',') {
    i.setstate(std::ios::failbit|std::ios::badbit);
    return i;
  }

  const int prec = (p.precision+2)*sizeof(mp_limb_t)*8;
  p.center.re.set_prec(prec);
  p.center.im.set_prec(prec);
  if (!(i >> p.center)) return i;
  if (!(i >> ch)) return i;
  if (ch != ',') {
    i.setstate(std::ios::failbit|std::ios::badbit);
    return i;
  }

  p.unity_pixel.re.set_prec(prec);
  p.unity_pixel.im.set_prec(prec);
  if (!(i >> p.unity_pixel)) return i;
  if (!(i >> ch)) return i;
  if (ch != ',') {
    i.setstate(std::ios::failbit|std::ios::badbit);
    return i;
  }

  if (!(i >> p.max_iter)) return i;
  if (!(i >> ch)) return i;
  if (ch != ']') i.setstate(std::ios::failbit|std::ios::badbit);

  return i;
}
*/
