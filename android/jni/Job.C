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

#include "Job.H"
#include "MandelImage.H"
#include "ThreadPool.H"

#include <string.h> // memset
#include <stdlib.h>

#include "Logger.H"

#include "Julia.H"

#include "GLee.h"
#if !defined(__ANDROID__) && defined(Complex)
  // X.h defines Complex as a macro for Polygon shapes
#undef Complex
#endif

FreeList JobQueueBase::free_list;

class FirstStageJob : public ChildJob {
public:
  FirstStageJob(Job *parent) : ChildJob(parent) {}
  void *operator new(size_t size) {
    if (size != sizeof(FirstStageJob)) ABORT();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
private:
  int getDistance(const int xy[2]) const {
    ABORT();
    return 0;
  }
  bool isFirstStageJob(void) const {return true;}
  bool execute(void) {return false;}
  static FreeList free_list;
};

FreeList FirstStageJob::free_list;

class LineJob : public ChildJob {
protected:
  LineJob(Job *parent,
          const MandelImage &image,int x,int y,unsigned int *d,int size)
    : ChildJob(parent),image(image),x(x),y(y),d(d),size(size) {}
protected:
  int getDistanceHorz(const int xy[2]) const {
    if (getParent()->isFirstStageJob()) {
      return static_cast<FirstStageJob*>(getParent())
               ->getParent()->getDistance(xy);
    }
    const int dist_x = (xy[0]<x) ? (x-xy[0]) :
                       (x+size<=xy[0]) ? (xy[0]-x-size+1) : 0;
    const int dist_y = (xy[1]<y) ? (y-xy[1]) : (xy[1]-y);
    return dist_x + dist_y;
  }
  int getDistanceVert(const int xy[2]) const {
    if (getParent()->isFirstStageJob()) {
      return static_cast<FirstStageJob*>(getParent())
               ->getParent()->getDistance(xy);
    }
    const int dist_x = (xy[0]<x) ? (x-xy[0]) : (xy[0]-x);
    const int dist_y = (xy[1]<y) ? (y-xy[1]) :
                       (y+size<=xy[1]) ? (xy[1]-y-size+1) : 0;
    return dist_x + dist_y;
  }
protected:
  const MandelImage &image;
  const int x;
  const int y;
  unsigned int *const d;
  int size;
};

class LineJobDouble : public LineJob {
protected:
  LineJobDouble(Job *parent,
          const MandelImage &image,int x,int y,unsigned int *d,
          const Complex<double> &re_im,int size)
    : LineJob(parent,image,x,y,d,size),re_im(re_im) {}
public:
  void *operator new(size_t size) {
    if (size != sizeof(LineJobDouble)) ABORT();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
protected:
  Complex<double> re_im;
  static FreeList free_list;
};

FreeList LineJobDouble::free_list;


class HorzLineJobDouble : public LineJobDouble {
public:
  static inline ChildJob *create(Job *parent,
                                 const MandelImage &image,int x,int y,
                                 int size_x);
  static inline HorzLineJobDouble *create(Job *parent,
                                    const MandelImage &image,int x,int y,
                                    unsigned int *d,
                                    const Complex<double> &re_im,
                                    int size_x);
protected:
  HorzLineJobDouble(Job *parent,
              const MandelImage &image,int x,int y,unsigned int *d,
              const Complex<double> &re_im,int size_x)
    : LineJobDouble(parent,image,x,y,d,re_im,size_x) {}
private:
  int getDistance(const int xy[2]) const {return getDistanceHorz(xy);}
  void print(std::ostream &o) const {
    o << "HorzLineJobDouble(" << x << ',' << y << ',' << size << ')';
  }
  bool execute(void);
  void drawToTexture(void) const {
    glTexSubImage2D(GL_TEXTURE_2D,0,
                    x,y,size,1,
                    GL_RGBA,GL_UNSIGNED_BYTE,d);
  }
};

class RecalcLimitHorzLineJobDouble : public HorzLineJobDouble {
public:
  RecalcLimitHorzLineJobDouble(Job *parent,
                         const MandelImage &image,int x,int y,unsigned int *d,
                         const Complex<double> &re_im,int size_x)
    : HorzLineJobDouble(parent,image,x,y,d,re_im,size_x) {}
private:
  void print(std::ostream &o) const {
    o << "RecalcLimitHorzLineJobDouble(" << x << ',' << y << ',' << size << ')';
  }
  bool execute(void);
};

inline HorzLineJobDouble *HorzLineJobDouble::create(Job *parent,
                                        const MandelImage &image,int x,int y,
                                        unsigned int *d,
                                        const Complex<double> &re_im,
                                        int size_x) {
  return
     (image.getRecalcLimit() > 0)
   ? new RecalcLimitHorzLineJobDouble(parent,image,x,y,d,re_im,size_x)
   : new HorzLineJobDouble(parent,image,x,y,d,re_im,size_x);
}

bool RecalcLimitHorzLineJobDouble::execute(void) {
  VECTOR_TYPE mr[VECTOR_SIZE];
  VECTOR_TYPE mi[VECTOR_SIZE];
  unsigned int tmp[VECTOR_SIZE];
  unsigned int *pos[VECTOR_SIZE];
  unsigned int *d = HorzLineJobDouble::d;
  int x = HorzLineJobDouble::x;
  int size_x = HorzLineJobDouble::size;
  int vector_count = 0;
  int count = 0;
  for (;size_x>0;size_x--,d++,x++,re_im+=image.getDReIm()) {
    if (terminate_flag) {
        // if max_iter has increased, mark remaining black pixels dirty:
      if (image.getMaxIter() > image.getRecalcLimit()) {
        for (int i=0;i<vector_count;i++) *(pos[i]) |= 0x80000000;
        do {
          if (*d >= image.getRecalcLimit()) *d |= 0x80000000;
          d++;
          size_x--;
        } while (size_x > 0);
      }
      goto exit_loop;
    }
    if (size_x > VECTOR_SIZE && image.nr_of_waiting_threads) {
      const int size_x0 = ((size_x/2)+(VECTOR_SIZE-1)) & (~(VECTOR_SIZE-1));
      image.thread_pool.queueJob(new RecalcLimitHorzLineJobDouble(
                                       getParent(),image,x+size_x0,y,
                                       d+size_x0,
                                       re_im+size_x0*image.getDReIm(),
                                       size_x-size_x0));
      HorzLineJobDouble::size -= (size_x-size_x0);
      size_x = size_x0;
    }
      // process pixel at (re_im)=(x,y)=*d
    if (image.needRecalc(*d)) {
      pos[vector_count] = d;
      mr[vector_count] = image.getStart().re+re_im.re;
      mi[vector_count] = image.getStart().im+re_im.im;
      vector_count++;
      if (vector_count >= VECTOR_SIZE) {
        vector_count = 0;
        JULIA_FUNC(mr,mi,image.getMaxIter(),tmp);
        count += VECTOR_SIZE;
        for (int i=0;i<VECTOR_SIZE;i++) *(pos[i]) = tmp[i];
      }
    }
  }
  if (vector_count > 0) {
    if (terminate_flag) {
      for (int i=0;i<vector_count;i++) *(pos[i]) |= 0x80000000;
      goto exit_loop;
    }
    JULIA_FUNC(mr,mi,image.getMaxIter(),tmp);
    count += vector_count;
    for (int i=0;i<vector_count;i++) *(pos[i]) = tmp[i];
  }
  exit_loop:
  __atomic_add(&image.pixel_count,count);
  resetParent();
  return true;
}

bool HorzLineJobDouble::execute(void) {
//if (y == 0)
//cout << "HorzLineJobDouble(" << x << ',' << y << ',' << size
//     << ")::execute begin" << endl;
  VECTOR_TYPE mr[VECTOR_SIZE];
  VECTOR_TYPE mi[VECTOR_SIZE];
  unsigned int *d = HorzLineJobDouble::d;
  int x = HorzLineJobDouble::x;
  int size_x = HorzLineJobDouble::size;
  int count = 0;
  for (;size_x>=VECTOR_SIZE;x+=VECTOR_SIZE,size_x-=VECTOR_SIZE,d+=VECTOR_SIZE) {
    if (terminate_flag) goto exit_loop;
    if (size_x > VECTOR_SIZE && image.nr_of_waiting_threads) {
      const int size_x0 = ((size_x/2)+(VECTOR_SIZE-1)) & (~(VECTOR_SIZE-1));
      image.thread_pool.queueJob(new HorzLineJobDouble(getParent(),image,x+size_x0,y,
                                                 d+size_x0,
                                                 re_im+size_x0*image.getDReIm(),
                                                 size_x-size_x0));
//if (y == 0)
//cout << "HorzLineJobDouble: split off: " << x+size_x0 << ", " << size_x-size_x0
//     << endl;
      HorzLineJobDouble::size -= (size_x-size_x0);
      size_x = size_x0;
//if (y == 0)
//cout << "HorzLineJobDouble: split rem: " << HorzLineJobDouble::x
//     << ", " << HorzLineJobDouble::size << endl;
    }
    for (int i=0;i<VECTOR_SIZE;i++,re_im+=image.getDReIm()) {
#ifdef DEBUG
      if (d[i]) {
        cout << "HorzLineJobDouble::execute: double drawing" << endl;
        ABORT();
      }
#endif
      mr[i] = image.getStart().re+re_im.re;
      mi[i] = image.getStart().im+re_im.im;
    }
    JULIA_FUNC(mr,mi,image.getMaxIter(),d);
    count += VECTOR_SIZE;
  }
  if (size_x > 0) {
    if (terminate_flag) goto exit_loop;
    unsigned int tmp[VECTOR_SIZE];
    for (int i=0;i<VECTOR_SIZE;i++,re_im+=image.getDReIm()) {
      mr[i] = image.getStart().re+re_im.re;
      mi[i] = image.getStart().im+re_im.im;
    }
    JULIA_FUNC(mr,mi,image.getMaxIter(),tmp);
    count += size;
    for (int i=0;i<size_x;i++) {
#ifdef DEBUG
      if (d[i]) {
        cout << "HorzLineJobDouble::execute: double drawing2" << endl;
        ABORT();
      }
#endif
      d[i] = tmp[i];
    }
  }
  exit_loop:
  __atomic_add(&image.pixel_count,count);
  resetParent();
//if (y == 0)
//cout << "HorzLineJobDouble::execute end" << endl;
  return true;
}



class VertLineJobDouble : public LineJobDouble {
public:
  static inline ChildJob *create(Job *parent,
                                 const MandelImage &image,int x,int y,
                                 int size_y);
  static inline VertLineJobDouble *create(Job *parent,
                                    const MandelImage &image,int x,int y,
                                    unsigned int *d,
                                    const Complex<double> &re_im,
                                    int size_y);
protected:
  VertLineJobDouble(Job *parent,
              const MandelImage &image,int x,int y,unsigned int *d,
              const Complex<double> &re_im,int size_y)
    : LineJobDouble(parent,image,x,y,d,re_im,size_y) {}
private:
  int getDistance(const int xy[2]) const {return getDistanceHorz(xy);}
  void print(std::ostream &o) const {
    o << "VertLineJobDouble(" << x << ',' << y << ',' << size << ')';
  }
  bool execute(void);
  void drawToTexture(void) const {
#ifdef __ANDROID__
    unsigned int tmp[size];
    unsigned int *p = tmp;
    int h = size;
    const unsigned int *d = VertLineJobDouble::d;
    do {
      *p++ = *d;
      d += image.getScreenWidth();
      h--;
    } while (h > 0);
    glTexSubImage2D(GL_TEXTURE_2D,0,
                    x,y,1,size,
                    GL_RGBA,GL_UNSIGNED_BYTE,tmp);
//    int h = size;
//    int y = VertLineJobDouble::y;
//    const unsigned int *d = VertLineJobDouble::d;
//    do {
//      glTexSubImage2D(GL_TEXTURE_2D,0,
//                      x,y,1,1,
//                      GL_RGBA,GL_UNSIGNED_BYTE,d);
//      h--;
//      y++;
//      d += image.getScreenWidth();
//    } while (h > 0);
#else
    glTexSubImage2D(GL_TEXTURE_2D,0,
                    x,y,1,size,
                    GL_RGBA,GL_UNSIGNED_BYTE,d);
#endif
  }
};

class RecalcLimitVertLineJobDouble : public VertLineJobDouble {
public:
  RecalcLimitVertLineJobDouble(Job *parent,
                         const MandelImage &image,int x,int y,unsigned int *d,
                         const Complex<double> &re_im,int size_y)
    : VertLineJobDouble(parent,image,x,y,d,re_im,size_y) {}
private:
  void print(std::ostream &o) const {
    o << "RecalcLimitVertLineJobDouble(" << x << ',' << y << ',' << size << ')';
  }
  bool execute(void);
};

inline VertLineJobDouble *VertLineJobDouble::create(Job *parent,
                                        const MandelImage &image,int x,int y,
                                        unsigned int *d,
                                        const Complex<double> &re_im,
                                        int size_y) {
  return
     (image.getRecalcLimit() > 0)
   ? new RecalcLimitVertLineJobDouble(parent,image,x,y,d,re_im,size_y)
   : new VertLineJobDouble(parent,image,x,y,d,re_im,size_y);
}

bool RecalcLimitVertLineJobDouble::execute(void) {
  VECTOR_TYPE mr[VECTOR_SIZE];
  VECTOR_TYPE mi[VECTOR_SIZE];
  unsigned int tmp[VECTOR_SIZE];
  unsigned int *pos[VECTOR_SIZE];
  unsigned int *d = VertLineJobDouble::d;
  int y = VertLineJobDouble::y;
  int size_y = VertLineJobDouble::size;
  int vector_count = 0;
  int count = 0;
  for (;size_y>0;
       size_y--,d+=image.getScreenWidth(),y++,
       re_im+=image.getDReIm().cross()) {
    if (terminate_flag) {
        // if max_iter has increased, mark remaining black pixels dirty:
      if (image.getMaxIter() > image.getRecalcLimit()) {
        for (int i=0;i<vector_count;i++) *(pos[i]) |= 0x80000000;
        do {
          if (*d >= image.getRecalcLimit()) *d |= 0x80000000;
          d += image.getScreenWidth();
          size_y--;
        } while (size_y > 0);
      }
      goto exit_loop;
    }
    if (size_y > VECTOR_SIZE && image.nr_of_waiting_threads) {
      const int size_y0 = ((size_y/2)+(VECTOR_SIZE-1)) & (~(VECTOR_SIZE-1));
      image.thread_pool.queueJob(new RecalcLimitVertLineJobDouble(
                                       getParent(),image,x,y+size_y0,
                                       d+size_y0*image.getScreenWidth(),
                                       re_im+size_y0*image.getDReIm().cross(),
                                       size_y-size_y0));
      VertLineJobDouble::size -= (size_y-size_y0);
      size_y = size_y0;
    }
      // process pixel at (re,im)=(x,y)=*d
    if (image.needRecalc(*d)) {
      pos[vector_count] = d;
      mr[vector_count] = image.getStart().re+re_im.re;
      mi[vector_count] = image.getStart().im+re_im.im;
      vector_count++;
      if (vector_count >= VECTOR_SIZE) {
        vector_count = 0;
        JULIA_FUNC(mr,mi,image.getMaxIter(),tmp);
        count += VECTOR_SIZE;
        for (int i=0;i<VECTOR_SIZE;i++) *(pos[i]) = tmp[i];
      }
    }
  }
  if (vector_count > 0) {
    if (terminate_flag) {
      for (int i=0;i<vector_count;i++) *(pos[i]) |= 0x80000000;
      goto exit_loop;
    }
    JULIA_FUNC(mr,mi,image.getMaxIter(),tmp);
    count += vector_count;
    for (int i=0;i<vector_count;i++) *(pos[i]) = tmp[i];
  }
  __atomic_add(&image.pixel_count,count);
  exit_loop:
  resetParent();
  return true;
}

bool VertLineJobDouble::execute(void) {
//cout << "VertLineJobDouble(" << x << ',' << y << ',' << size
//     << ")::execute begin: " << re << endl;
  if (size <= 0) ABORT();
  VECTOR_TYPE mr[VECTOR_SIZE];
  VECTOR_TYPE mi[VECTOR_SIZE];
  unsigned int tmp[VECTOR_SIZE];
  unsigned int *d = VertLineJobDouble::d;
  int size_y = VertLineJobDouble::size;
  int count = 0;
  for (int y=VertLineJobDouble::y;size_y>VECTOR_SIZE;
       y+=VECTOR_SIZE,size_y-=VECTOR_SIZE) {
    if (terminate_flag) goto exit_loop;
    if (image.nr_of_waiting_threads) {
      const int size_y0 = ((size_y/2)+(VECTOR_SIZE-1)) & (~(VECTOR_SIZE-1));
      image.thread_pool.queueJob(new VertLineJobDouble(
                                       getParent(),image,x,y+size_y0,
                                       d+size_y0*image.getScreenWidth(),
                                       re_im+size_y0*image.getDReIm().cross(),
                                       size_y-size_y0));
//if (x == 399)
//cout << "VertLineJobDouble: split off: " << y+size_y0 << ", " << size_y-size_y0
//     << endl;
      VertLineJobDouble::size -= (size_y-size_y0);
      size_y = size_y0;
      if (size_y <= VECTOR_SIZE) break;
    }
    for (int j=0;j<VECTOR_SIZE;j++,re_im+=image.getDReIm().cross()) {
      mr[j] = image.getStart().re+re_im.re;
      mi[j] = image.getStart().im+re_im.im;
    }
    JULIA_FUNC(mr,mi,image.getMaxIter(),tmp);
    count += VECTOR_SIZE;
    for (int j=0;j<VECTOR_SIZE;j++,d+=image.getScreenWidth()) {
      *d = tmp[j];
    }
  }
  if (terminate_flag) goto exit_loop;
  for (int j=0;j<VECTOR_SIZE;j++,re_im+=image.getDReIm().cross()) {
    mr[j] = image.getStart().re+re_im.re;
    mi[j] = image.getStart().im+re_im.im;
  }
  JULIA_FUNC(mr,mi,image.getMaxIter(),tmp);
  count += size_y;
  for (int j=0;j<size_y;j++,d+=image.getScreenWidth()) {
    *d = tmp[j];
  }
  __atomic_add(&image.pixel_count,count);
  exit_loop:
  resetParent();
//cout << "VertLineJobDouble(" << VertLineJobDouble::x << ',' << VertLineJobDouble::y
//     << ")::execute end" << endl;
  return true;
}





class LineJobGmp : public LineJob {
protected:
  LineJobGmp(Job *parent,
             const MandelImage &image,int x,int y,unsigned int *d,
             GmpFixedPointLockfree &re,
             GmpFixedPointLockfree &im,int size)
    : LineJob(parent,image,x,y,d,size),re(re),im(im) {}
public:
  void *operator new(size_t size) {
    if (size != sizeof(LineJobGmp)) ABORT();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
protected:
  GmpFixedPointLockfree re;
  GmpFixedPointLockfree im;
  static FreeList free_list;
};

FreeList LineJobGmp::free_list;

class HorzLineJobGmp : public LineJobGmp {
public:
  static inline HorzLineJobGmp *create(Job *parent,
                                       const MandelImage &image,int x,int y,
                                       unsigned int *d,
                                       GmpFixedPointLockfree &re,
                                       GmpFixedPointLockfree &im,
                                       int size_x);
protected:
  HorzLineJobGmp(Job *parent,
                 const MandelImage &image,int x,int y,unsigned int *d,
                 GmpFixedPointLockfree &re,
                 GmpFixedPointLockfree &im,int size_x)
    : LineJobGmp(parent,image,x,y,d,re,im,size_x) {}
private:
  int getDistance(const int xy[2]) const {return getDistanceHorz(xy);}
  void print(std::ostream &o) const {
    o << "HorzLineJobGmp(" << x << ',' << y << ',' << size << ')';
  }
  bool execute(void);
  void drawToTexture(void) const {
    glTexSubImage2D(GL_TEXTURE_2D,0,
                    x,y,size,1,
                    GL_RGBA,GL_UNSIGNED_BYTE,d);
  }
};

inline ChildJob *HorzLineJobDouble::create(Job *parent,
                                     const MandelImage &image,int x,int y,
                                     int size_x) {
  if (image.getPrecision() > 0) {
//cout << "HorzLineJobDouble::create: 100" << endl;
    GmpFixedPointLockfree re;
//cout << "HorzLineJobDouble::create: 101" << endl;
    GmpFixedPointLockfree im;
//cout << "HorzLineJobDouble::create: 102" << endl;
      // one extra limb so that there is no carry/borrow:
    re.p[image.getPrecision()+1]
      = re.linCombMinusU1(image.getDRe(),x,image.getDIm(),y);
    im.p[image.getPrecision()+1]
      = im.linCombPlusU1(image.getDRe(),y,image.getDIm(),x);
    re.add2(image.getStartRe());
    im.add2(image.getStartIm());
//cout << "HorzLineJobDouble::create: 198" << endl;
    return HorzLineJobGmp::create(
                             parent,image,x,y,
                             image.getData()+y*image.getScreenWidth()+x,
                             re,im,size_x);
  }
  return HorzLineJobDouble::create(
                        parent,image,x,y,
                        image.getData()+y*image.getScreenWidth()+x,
                        Complex<double>(x,y)*image.getDReIm(),
                        size_x);
}

class RecalcLimitHorzLineJobGmp : public HorzLineJobGmp {
public:
  RecalcLimitHorzLineJobGmp(Job *parent,
                            const MandelImage &image,
                            int x,int y,unsigned int *d,
                            GmpFixedPointLockfree &re,
                            GmpFixedPointLockfree &im,int size_x)
    : HorzLineJobGmp(parent,image,x,y,d,re,im,size_x) {}
private:
  void print(std::ostream &o) const {
    o << "RecalcLimitHorzLineJobGmp(" << x << ',' << y << ',' << size << ')';
  }
  bool execute(void);
};

inline HorzLineJobGmp *HorzLineJobGmp::create(Job *parent,
                                              const MandelImage &image,
                                              int x,int y,unsigned int *d,
                                              GmpFixedPointLockfree &re,
                                              GmpFixedPointLockfree &im,
                                              int size_x) {
  return
     (image.getRecalcLimit() > 0)
   ? new RecalcLimitHorzLineJobGmp(parent,image,x,y,d,re,im,size_x)
   : new HorzLineJobGmp(parent,image,x,y,d,re,im,size_x);
}

bool RecalcLimitHorzLineJobGmp::execute(void) {
  unsigned int *d = HorzLineJobGmp::d;
  int x = HorzLineJobGmp::x;
  int size_x = HorzLineJobGmp::size;
  int count = 0;
  for (;;) {
    if (terminate_flag) {
        // if max_iter has increased, mark remaining black pixels dirty:
      if (image.getMaxIter() > image.getRecalcLimit()) {
        do {
          if (*d >= image.getRecalcLimit()) *d |= 0x80000000;
          d++;
          size_x--;
        } while (size_x > 0);
      }
      break;
    }
    if (image.nr_of_waiting_threads && size_x > 1) {
      const int size_x0 = (size_x/2);
      GmpFixedPointLockfree tmp_re;
      GmpFixedPointLockfree tmp_im;
      tmp_re.assign2(re);
      tmp_im.assign2(im);
      tmp_re.addMulU2(image.getDRe(),size_x0);
      tmp_im.addMulU2(image.getDIm(),size_x0);
      image.thread_pool.queueJob(new RecalcLimitHorzLineJobGmp(
                                       getParent(),image,
                                       x+size_x0,y,d+size_x0,
                                       tmp_re,tmp_im,
                                       size_x-size_x0));
      HorzLineJobGmp::size -= (size_x-size_x0);
      size_x = size_x0;
    }
      // process pixel at (re_im)=(x,y)=*d
    if (image.needRecalc(*d)) {
      *d = GmpFixedPoint::GmpMandel2(re,im,image.getMaxIter());
      count++;
    }
    size_x--;
    if (size_x <= 0) break;
    x++;
    d++;
    re.add2(image.getDRe());
    im.add2(image.getDIm());
  }
  __atomic_add(&image.pixel_count,count);
  resetParent();
  return true;
}

bool HorzLineJobGmp::execute(void) {
  unsigned int *d = HorzLineJobGmp::d;
  int x = HorzLineJobGmp::x;
  int size_x = HorzLineJobGmp::size;
  int count = 0;
//      cout << "HorzLineJobGmp::execute: start" << endl;
  while (!terminate_flag) {
//      cout << "HorzLineJobGmp::execute: 100" << endl;
    if (image.nr_of_waiting_threads && size_x > 1) {
//      cout << "HorzLineJobGmp::execute: 200" << endl;
      const int size_x0 = (size_x/2);
      GmpFixedPointLockfree tmp_re;
      GmpFixedPointLockfree tmp_im;
      tmp_re.assign2(re);
      tmp_im.assign2(im);
      tmp_re.addMulU2(image.getDRe(),size_x0);
      tmp_im.addMulU2(image.getDIm(),size_x0);
//      cout << "HorzLineJobGmp::execute: 250" << endl;
      image.thread_pool.queueJob(new HorzLineJobGmp(
                                       getParent(),image,
                                       x+size_x0,y,d+size_x0,
                                       tmp_re,tmp_im,
                                       size_x-size_x0));
//      cout << "HorzLineJobGmp::execute: 251" << endl;
      HorzLineJobGmp::size -= (size_x-size_x0);
      size_x = size_x0;
//      cout << "HorzLineJobGmp::execute: 299" << endl;
    }
#ifdef DEBUG
    if (*d) {
      cout << "HorzLineJobGmp::execute: double drawing" << endl;
      ABORT();
    }
#endif
//      cout << "HorzLineJobGmp::execute: 300" << endl;
    *d = GmpFixedPoint::GmpMandel2(re,im,image.getMaxIter());
//      cout << "HorzLineJobGmp::execute: 301" << endl;
    count++;
    size_x--;
    if (size_x <= 0) break;
    x++;
    d++;
    re.add2(image.getDRe());
    im.add2(image.getDIm());
//      cout << "HorzLineJobGmp::execute: 399" << endl;
  }
  __atomic_add(&image.pixel_count,count);
  resetParent();
//      cout << "HorzLineJobGmp::execute: end" << endl;
  return true;
}


class VertLineJobGmp : public LineJobGmp {
public:
  static inline VertLineJobGmp *create(Job *parent,
                                       const MandelImage &image,
                                       int x,int y,unsigned int *d,
                                       GmpFixedPointLockfree &re,
                                       GmpFixedPointLockfree &im,
                                       int size_y);
protected:
  VertLineJobGmp(Job *parent,
                 const MandelImage &image,int x,int y,unsigned int *d,
                 GmpFixedPointLockfree &re,
                 GmpFixedPointLockfree &im,int size_y)
    : LineJobGmp(parent,image,x,y,d,re,im,size_y) {}
private:
  int getDistance(const int xy[2]) const {return getDistanceVert(xy);}
  void print(std::ostream &o) const {
    o << "VertLineJobGmp(" << x << ',' << y << ',' << size << ')';
  }
  bool execute(void);
  void drawToTexture(void) const {
#ifdef __ANDROID__
    unsigned int tmp[size];
    unsigned int *p = tmp;
    int h = size;
    const unsigned int *d = VertLineJobGmp::d;
    do {
      *p++ = *d;
      d += image.getScreenWidth();
      h--;
    } while (h > 0);
    glTexSubImage2D(GL_TEXTURE_2D,0,
                    x,y,1,size,
                    GL_RGBA,GL_UNSIGNED_BYTE,tmp);
//    int h = size;
//    int y = VertLineJobGmp::y;
//    const unsigned int *d = VertLineJobGmp::d;
//    do {
//      glTexSubImage2D(GL_TEXTURE_2D,0,
//                      x,y,1,1,
//                      GL_RGBA,GL_UNSIGNED_BYTE,d);
//      h--;
//      y++;
//      d += image.getScreenWidth();
//    } while (h > 0);
#else
    glTexSubImage2D(GL_TEXTURE_2D,0,
                    x,y,1,size,
                    GL_RGBA,GL_UNSIGNED_BYTE,d);
#endif
  }
};

inline ChildJob *VertLineJobDouble::create(Job *parent,
                                     const MandelImage &image,int x,int y,
                                     int size_y) {
  if (image.getPrecision() > 0) {
    GmpFixedPointLockfree re;
    GmpFixedPointLockfree im;
      // one extra limb so that there is no carry/borrow:
    re.p[image.getPrecision()+1]
      = re.linCombMinusU1(image.getDRe(),x,image.getDIm(),y);
    im.p[image.getPrecision()+1]
      = im.linCombPlusU1(image.getDRe(),y,image.getDIm(),x);
    re.add2(image.getStartRe());
    im.add2(image.getStartIm());
    return VertLineJobGmp::create(
                            parent,image,x,y,
                            image.getData()+y*image.getScreenWidth()+x,
                            re,im,size_y);
  }
  return VertLineJobDouble::create(
                        parent,image,x,y,
                        image.getData()+y*image.getScreenWidth()+x,
                        Complex<double>(x,y)*image.getDReIm(),
                        size_y);
}

class RecalcLimitVertLineJobGmp : public VertLineJobGmp {
public:
  RecalcLimitVertLineJobGmp(Job *parent,
                            const MandelImage &image,
                            int x,int y,unsigned int *d,
                            GmpFixedPointLockfree &re,
                            GmpFixedPointLockfree &im,int size_y)
    : VertLineJobGmp(parent,image,x,y,d,re,im,size_y) {}
private:
  void print(std::ostream &o) const {
    o << "RecalcLimitVertLineJobGmp(" << x << ',' << y << ',' << size << ')';
  }
  bool execute(void);
};

inline VertLineJobGmp *VertLineJobGmp::create(Job *parent,
                                              const MandelImage &image,
                                              int x,int y,unsigned int *d,
                                              GmpFixedPointLockfree &re,
                                              GmpFixedPointLockfree &im,
                                              int size_y) {
  return
     (image.getRecalcLimit() > 0)
   ? new RecalcLimitVertLineJobGmp(parent,image,x,y,d,re,im,size_y)
   : new VertLineJobGmp(parent,image,x,y,d,re,im,size_y);
}

bool RecalcLimitVertLineJobGmp::execute(void) {
  unsigned int *d = VertLineJobGmp::d;
  int y = VertLineJobGmp::y;
  int size_y = VertLineJobGmp::size;
  int count = 0;
  for (;;) {
    if (terminate_flag) {
        // if max_iter has increased, mark remaining black pixels dirty:
      if (image.getMaxIter() > image.getRecalcLimit()) {
        do {
          if (*d >= image.getRecalcLimit()) *d |= 0x80000000;
          d++;
          size_y--;
        } while (size_y > 0);
      }
      break;
    }
    if (image.nr_of_waiting_threads && size_y > 1) {
      const int size_y0 = (size_y/2);
      GmpFixedPointLockfree tmp_re;
      GmpFixedPointLockfree tmp_im;
      tmp_re.assign2(re);
      tmp_im.assign2(im);
      tmp_re.subMulU2(image.getDIm(),size_y0);
      tmp_im.addMulU2(image.getDRe(),size_y0);
      image.thread_pool.queueJob(new RecalcLimitVertLineJobGmp(
                                       getParent(),image,
                                       x,y+size_y0,
                                       d+size_y0*image.getScreenWidth(),
                                       tmp_re,tmp_im,
                                       size_y-size_y0));
      VertLineJobGmp::size -= (size_y-size_y0);
      size_y = size_y0;
    }
      // process pixel at (re_im)=(x,y)=*d
    if (image.needRecalc(*d)) {
      *d = GmpFixedPoint::GmpMandel2(re,im,image.getMaxIter());
      count++;
    }
    size_y--;
    if (size_y <= 0) break;
    y++;
    d+=image.getScreenWidth();
    re.sub2(image.getDIm());
    im.add2(image.getDRe());
  }
  __atomic_add(&image.pixel_count,count);
  resetParent();
  return true;
}

bool VertLineJobGmp::execute(void) {
  unsigned int *d = VertLineJobGmp::d;
  int y = VertLineJobGmp::y;
  int size_y = VertLineJobGmp::size;
  int count = 0;
//      cout << "VertLineJobGmp::execute: start" << endl;
  while (!terminate_flag) {
//      cout << "VertLineJobGmp::execute: 100; " << size_y << endl;
    if (image.nr_of_waiting_threads && size_y > 1) {
//      cout << "VertLineJobGmp::execute: 200" << endl;
      const int size_y0 = (size_y/2);
      GmpFixedPointLockfree tmp_re;
      GmpFixedPointLockfree tmp_im;
      tmp_re.assign2(re);
      tmp_im.assign2(im);
      tmp_re.subMulU2(image.getDIm(),size_y0);
      tmp_im.addMulU2(image.getDRe(),size_y0);
//      cout << "VertLineJobGmp::execute: 250" << endl;
      image.thread_pool.queueJob(new VertLineJobGmp(
                                       getParent(),image,
                                       x,y+size_y0,
                                       d+size_y0*image.getScreenWidth(),
                                       tmp_re,tmp_im,
                                       size_y-size_y0));
//      cout << "VertLineJobGmp::execute: 251" << endl;
      VertLineJobGmp::size -= (size_y-size_y0);
      size_y = size_y0;
//      cout << "VertLineJobGmp::execute: 299" << endl;
    }
#ifdef DEBUG
    if (*d) {
      cout << "VertLineJobGmp::execute: double drawing" << endl;
      ABORT();
    }
#endif
//      cout << "VertLineJobGmp::execute: 300" << endl;
    *d = GmpFixedPoint::GmpMandel2(re,im,image.getMaxIter());
//      cout << "VertLineJobGmp::execute: 301" << endl;
    count++;
    size_y--;
    if (size_y <= 0) break;
    y++;
    d+=image.getScreenWidth();
    re.sub2(image.getDIm());
    im.add2(image.getDRe());
//      cout << "VertLineJobGmp::execute: 399" << endl;
  }
  __atomic_add(&image.pixel_count,count);
  resetParent();
//      cout << "VertLineJobGmp::execute: end" << endl;
  return true;
}




















class RectJob : public ChildJob {
protected:
  RectJob(Job *parent,
          const MandelImage &image,int x,int y,int size_x,int size_y)
    : ChildJob(parent),image(image),x(x),y(y),size_x(size_x),size_y(size_y) {}
  void *operator new(size_t size) {
    if (size != sizeof(RectJob)) ABORT();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
private:
  int getDistance(const int xy[2]) const {
    const int dist_x = (xy[0]<x) ? (x-xy[0]) :
                       (x+size_x<=xy[0]) ? (xy[0]-x-size_x+1) : 0;
    const int dist_y = (xy[1]<y) ? (y-xy[1]) :
                       (y+size_y<=xy[1]) ? (xy[1]-y-size_y+1) : 0;
    return dist_x + dist_y;
  }
protected:
  const MandelImage &image;
  const int x;
  const int y;
  const int size_x;
  const int size_y;
  static FreeList free_list;
};

FreeList RectJob::free_list;



class FillRectJob : public RectJob {
public:
  static FillRectJob *create(Job *parent,
                             const MandelImage &image,
                             int x,int y,int size_x,int size_y,
                             unsigned int value) {
    return new FillRectJob(parent,image,x,y,size_x,size_y,value);
  }
private:
  FillRectJob(Job *parent,
              const MandelImage &image,int x,int y,int size_x,int size_y,
              unsigned int value)
    : RectJob(parent,image,x,y,size_x,size_y),value(value) {}
  void *operator new(size_t size) {
    if (size != sizeof(FillRectJob)) ABORT();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
private:
    // execute as soon as possible:
  int getDistance(const int xy[2]) const {return -1;}
  void print(std::ostream &o) const {
    o << "FillRectJob("
      << x << ',' << y << ',' << size_x << ',' << size_y << ')';
  }
  bool execute(void) {
    int count = 0;
    unsigned int *d = image.getData() + y*image.getScreenWidth() + x;
    for (int j=size_y;j>0;j--,d+=image.getScreenWidth()) {
      for (int i=0;i<size_x;i++) {
#ifdef DEBUG
        if (d[i]) {
          cout << "FillRectJob::execute: double drawing" << endl;
          ABORT();
        }
#endif
        if (image.needRecalc(d[i])) count++;
        d[i] = value;
      }
    }
    __atomic_add(&image.pixel_count,count);
    resetParent();
    return true;
  }
  void drawToTexture(void) const {
    const unsigned int *d = image.getData()+x+y*(image.getScreenWidth());
#ifdef __ANDROID__
    int h = size_y;
    int y = FillRectJob::y;
    do {
      glTexSubImage2D(GL_TEXTURE_2D,0,
                      x,y,size_x,1,
                      GL_RGBA,GL_UNSIGNED_BYTE,d);
      h--;
      y++;
      d += image.getScreenWidth();
    } while (h > 0);
#else
    glTexSubImage2D(GL_TEXTURE_2D,0,
                    x,y,size_x,size_y,
                    GL_RGBA,GL_UNSIGNED_BYTE,d);
#endif
  }
private:
  const unsigned int value;
  static FreeList free_list;
};

FreeList FillRectJob::free_list;


class FullRectJob : public RectJob {
public:
  static FullRectJob *create(Job *parent,
                             const MandelImage &image,
                             int x,int y,int size_x,int size_y) {
    return new FullRectJob(parent,image,x,y,size_x,size_y);
  }
private:
  FullRectJob(Job *parent,
              const MandelImage &image,int x,int y,int size_x,int size_y)
    : RectJob(parent,image,x,y,size_x,size_y) {}
  void print(std::ostream &o) const {
    o << "FullRectJob("
      << x << ',' << y << ',' << size_x << ',' << size_y << ')';
  }
  bool execute(void) {
    if (image.getPrecision() > 0) {
      unsigned int *const image_topleft = image.getData()
                                        + y*image.getScreenWidth()+x;
      unsigned int *d = image_topleft;
      GmpFixedPointLockfree re;
      GmpFixedPointLockfree im;
        // one extra limb so that there is no carry/borrow:
      re.p[image.getPrecision()+1]
        = re.linCombMinusU1(image.getDRe(),x,image.getDIm(),y);
      im.p[image.getPrecision()+1]
        = im.linCombPlusU1(image.getDRe(),y,image.getDIm(),x);
      re.add2(image.getStartRe());
      im.add2(image.getStartIm());
      for (int j=0;;) {
        if (terminate_flag) {
          if (image.getRecalcLimit() > 0 &&
              image.getMaxIter() > image.getRecalcLimit()) {
              // mark remaining black pixels dirty:
            for (;j<size_y;j++,d+=image.getScreenWidth()) {
              for (int i=0;i<size_x;i++) {
                if (d[i] >= image.getRecalcLimit()) d[i] |= 0x80000000;
              }
            }
            break;
          }
          return false;
        }
        GmpFixedPointLockfree new_re;
        GmpFixedPointLockfree new_im;
        new_re.sub2(re,image.getDIm());
        new_im.add2(im,image.getDRe());
        image.thread_pool.queueJob(
                            HorzLineJobGmp::create(
                              this,image,x,y+j,d,re,im,size_x));
        j++;
        if (j >= size_y) break;
        d += image.getScreenWidth();
        re.takeOwnership(new_re);
        im.takeOwnership(new_im);
      }
      return false;
    } else {
      Complex<double> re_im(Complex<double>(x,y)*image.getDReIm());
      unsigned int *const image_topleft = image.getData()
                                        + y*image.getScreenWidth()+x;
      unsigned int *d = image_topleft;
      int w = size_x & (~(image.getVectorSize()-1));
      Complex<double> yh = re_im;
      for (int j=0;j<size_y;
           j++,d+=image.getScreenWidth(),yh+=image.getDReIm().cross()) {
        if (terminate_flag) {
          if (image.getRecalcLimit() > 0 &&
              image.getMaxIter() > image.getRecalcLimit()) {
              // mark remaining black pixels dirty:
            for (;j<size_y;j++,d+=image.getScreenWidth()) {
              for (int i=0;i<w;i++) {
                if (d[i] >= image.getRecalcLimit()) d[i] |= 0x80000000;
              }
            }
            break;
          }
          return false;
        }
        image.thread_pool.queueJob(
                            HorzLineJobDouble::create(
                              this,image,x,y+j,d,yh,w));
      }
      d = image_topleft+w;
      re_im += w*image.getDReIm();
      for (;w<size_x;w++,d++,re_im+=image.getDReIm()) {
        if (terminate_flag) {
          if (image.getRecalcLimit() > 0 &&
              image.getMaxIter() > image.getRecalcLimit()) {
              // mark remaining black pixels dirty:
            w = size_x - w;
            for (int j=0;j<size_y;j++,d+=image.getScreenWidth()) {
              for (int i=0;i<w;i++) {
                if (d[i] >= image.getRecalcLimit()) d[i] |= 0x80000000;
              }
            }
            break;
          }
          return false;
        }
        image.thread_pool.queueJob(
                            VertLineJobDouble::create(
                              this,image,x+w,y,d,re_im,size_y));
      }
      return false;
    }
  }
};

//using __gnu_cxx::__exchange_and_add;
//static volatile _Atomic_word job_id_sequence = 0;
//        id(__exchange_and_add(&job_id_sequence,1)),



class RectContentsJob : public RectJob {
public:
  static RectContentsJob *create(Job *parent,
                                 const MandelImage &image,
                                 int x,int y,int size_x,int size_y) {
    return new RectContentsJob(parent,image,x,y,size_x,size_y);
  }
private:
  RectContentsJob(Job *parent,
                  const MandelImage &image,int x,int y,int size_x,int size_y)
    : RectJob(parent,image,x,y,size_x,size_y) {
//    cout << "RectContentsJob::RectContentsJob("
//         << x << ',' << y << ',' << size_x << ',' << size_y << ')'
//         << endl;
  }
  ~RectContentsJob(void) {
//    cout << "RectContentsJob::~RectContentsJob"
//         << endl;
  }
  void print(std::ostream &o) const {
    o << "RectContentsJob("
      << x << ',' << y << ',' << size_x << ',' << size_y << ')';
  }
  bool execute(void);
  class HorzFirstStageJob : public FirstStageJob {
  public:
    static HorzFirstStageJob *create(RectContentsJob *parent) {
      return new HorzFirstStageJob(parent);
    }
  private:
    HorzFirstStageJob(RectContentsJob *parent) : FirstStageJob(parent) {}
    void print(std::ostream &o) const {o << "HorzFirstStageJob";}
    ~HorzFirstStageJob(void) {
      static_cast<RectContentsJob*>(getParent())->firstStageHorzFinished();
    }
  };
  void firstStageHorzFinished(void);
  class VertFirstStageJob : public FirstStageJob {
  public:
    static VertFirstStageJob *create(RectContentsJob *parent) {
      return new VertFirstStageJob(parent);
    }
  private:
    VertFirstStageJob(RectContentsJob *parent) : FirstStageJob(parent) {}
    void print(std::ostream &o) const {o << "VertFirstStageJob";}
    ~VertFirstStageJob(void) {
      static_cast<RectContentsJob*>(getParent())->firstStageVertFinished();
    }
  };
  void firstStageVertFinished(void);
};

void RectContentsJob::firstStageHorzFinished(void) {
  if (terminate_flag &&
      (image.getRecalcLimit() == 0 ||
       image.getMaxIter() <= image.getRecalcLimit())) return;
  const int wh = size_x / 2;
  const int xwh = x + wh;
  if (xwh < image.getPriorityX()) {
    image.thread_pool.queueJob(
                        RectContentsJob::create(
                                           this,image,x,y,wh+1,size_y));
    image.thread_pool.queueJob(
                        RectContentsJob::create(
                                           this,image,xwh,y,size_x-wh,size_y));
  } else {
    image.thread_pool.queueJob(
                        RectContentsJob::create(
                                           this,image,xwh,y,size_x-wh,size_y));
    image.thread_pool.queueJob(
                        RectContentsJob::create(
                                           this,image,x,y,wh+1,size_y));
  }
}

void RectContentsJob::firstStageVertFinished(void) {
  if (terminate_flag &&
      (image.getRecalcLimit() == 0 ||
       image.getMaxIter() <= image.getRecalcLimit())) return;
  const int hh = size_y / 2;
  const int yhh = y+hh;
  if (yhh < image.getPriorityY()) {
    image.thread_pool.queueJob(
                        RectContentsJob::create(
                                           this,image,x,y,size_x,hh+1));
    image.thread_pool.queueJob(
                        RectContentsJob::create(
                                           this,image,x,yhh,size_x,size_y-hh));
  } else {
    image.thread_pool.queueJob(
                        RectContentsJob::create(
                                           this,image,x,yhh,size_x,size_y-hh));
    image.thread_pool.queueJob(
                        RectContentsJob::create(
                                           this,image,x,y,size_x,hh+1));
  }
}

bool RectContentsJob::execute(void) {
  if (terminate_flag) {
    if (image.getRecalcLimit() > 0 &&
        image.getMaxIter() > image.getRecalcLimit()) {
        // mark remaining black pixels dirty:
      unsigned int *d = image.getData() + (y+1)*image.getScreenWidth() + (x+1);
      for (int j=size_y-3;j>=0;j--,d+=image.getScreenWidth()) {
        for (int i=size_x-3;i>=0;i--) {
          if (d[i] >= image.getRecalcLimit()) d[i] |= 0x80000000;
        }
      }
    }
  } else {
//cout << "RectContentsJob::execute begin" << endl;
      // check if must split
    unsigned int *data = image.getData() + y*image.getScreenWidth() + x;
    const unsigned int *d0 = data;
    const unsigned int *d1 = d0+(size_y-1)*image.getScreenWidth();
    unsigned int check_value = *d0;
    int count_other = 0;
    int count_max = 0;
#ifdef DEBUG
    int count_zero = 0;
    if (*d0 == 0) count_zero++;
    if (*d1 == 0) count_zero++;
#endif
    d0++;
    if (check_value < image.getMaxIter()) {
      if (*d1 != check_value) {count_other++;if (*d1 >= image.getMaxIter()) count_max++;}
      d1++;
      for (int i=1;i<size_x;i++,d0++,d1++) {
#ifdef DEBUG
        if (*d0 == 0) count_zero++;
        if (*d1 == 0) count_zero++;
#endif
        if (*d0 != check_value) {count_other++;if (*d0 >= image.getMaxIter()) count_max++;}
        if (*d1 != check_value) {count_other++;if (*d1 >= image.getMaxIter()) count_max++;}
      }
      d0 = data + image.getScreenWidth();
      d1 = d0 + (size_x-1);
      for (int i=2;i<size_y;
           i++,d0+=image.getScreenWidth(),d1+=image.getScreenWidth()) {
#ifdef DEBUG
        if (*d0 == 0) count_zero++;
        if (*d1 == 0) count_zero++;
#endif
        if (*d0 != check_value) {count_other++;if (*d0 >= image.getMaxIter()) count_max++;}
        if (*d1 != check_value) {count_other++;if (*d1 >= image.getMaxIter()) count_max++;}
      }
    } else {
      check_value = image.getMaxIter();
      if (*d1 < image.getMaxIter()) count_other++;
      d1++;
      for (int i=1;i<size_x;i++,d0++,d1++) {
#ifdef DEBUG
        if (*d0 == 0) count_zero++;
        if (*d1 == 0) count_zero++;
#endif
        if (*d0 < image.getMaxIter()) count_other++;
        if (*d1 < image.getMaxIter()) count_other++;
      }
      d0 = data + image.getScreenWidth();
      d1 = d0 + (size_x-1);
      for (int i=2;i<size_y;
           i++,d0+=image.getScreenWidth(),d1+=image.getScreenWidth()) {
#ifdef DEBUG
        if (*d0 == 0) count_zero++;
        if (*d1 == 0) count_zero++;
#endif
        if (*d0 < image.getMaxIter()) count_other++;
        if (*d1 < image.getMaxIter()) count_other++;
      }
      count_max = 2*(size_x+size_y-2) - count_other;
    }
#ifdef DEBUG
    if (count_zero) {
      cout << "RectContentsJob::execute: count_zero=" << count_zero
           << ", count_other=" << count_other
           << ", count_max=" << count_max
           << endl;
    }
#endif


//cout << "RectContentsJob::execute 200" << endl;
    if (count_other == 0) {
//cout << "RectContentsJob::execute 201" << endl;
        // everything is inside
      image.thread_pool.queueJob(
                          FillRectJob::create(
                                         this,image,
                                         x+1,y+1,size_x-2,size_y-2,
                                         check_value));
    } else if (count_max == 0 && size_x < 20 && size_y < 20) {

//cout << "RectContentsJob::execute 210" << endl;
        // do not bisect any more
      image.thread_pool.queueJob(
                          FullRectJob::create(
                                         this,image,
                                         x+1,y+1,size_x-2,size_y-2));
    } else {
#ifdef DEBUG
      image.assertEmpty(x+1,y+1,size_x-2,size_y-2);
#endif
      if (size_x > size_y) {
          // split horizontally
        if (size_x <= 5) {
          image.thread_pool.queueJob(
                              FullRectJob::create(
                                             this,image,
                                             x+1,y+1,size_x-2,size_y-2));
        } else {
          const int wh = size_x / 2;
          Job *first_stage(HorzFirstStageJob::create(this));
          image.thread_pool.queueJob(
                              VertLineJobDouble::create(
                                             first_stage,image,
                                             x+wh,y+1,size_y-2));
        }
      } else {
          // split vertically
        if (size_y <= 5) {
          image.thread_pool.queueJob(
                              FullRectJob::create(
                                             this,image,
                                             x+1,y+1,size_x-2,size_y-2));
        } else {
          const int hh = size_y / 2;
          Job *first_stage(VertFirstStageJob::create(this));
          image.thread_pool.queueJob(
                              HorzLineJobDouble::create(
                                             first_stage,image,
                                             x+1,y+hh,size_x-2));
        }
      }
    }
  }
//cout << "RectContentsJob::execute end" << endl;
  return false;
}


MainJob::MainJob(const MandelImage &image,int size_x,int size_y)
        :Job(image.thread_pool.terminate_flag),
         image(image),size_x(size_x),size_y(size_y) {
//  cout << "MainJob::MainJob" << endl;
}

MainJob::~MainJob(void) {
//  cout << "MainJob::~MainJob" << endl;
  image.thread_pool.mainJobHasTerminated();
}


class MainFirstStageJob : public FirstStageJob {
public:
  MainFirstStageJob(MainJob *parent) : FirstStageJob(parent) {}
private:
  void print(std::ostream &o) const {o << "MainFirstStageJob";}
  ~MainFirstStageJob(void) {
//    cout << "MainFirstStageJob::~MainFirstStageJob" << endl;
    static_cast<MainJob*>(getParent())->firstStageFinished();
  }
};

bool MainJob::execute(void) {
#ifdef DEBUG
  unsigned int *d = image.getData();
  for (int i=0;i<size_y;i++,d+=image.getScreenWidth()) {
    memset(d,0,sizeof(unsigned int)*size_x);
  }
#endif
  Job *first_stage(new MainFirstStageJob(this));
  image.thread_pool.queueJob(HorzLineJobDouble::create(
                                   first_stage,image,0,0,size_x));
  image.thread_pool.queueJob(HorzLineJobDouble::create(
                                   first_stage,image,0,size_y-1,size_x));
  image.thread_pool.queueJob(VertLineJobDouble::create(
                                   first_stage,image,0,1,size_y-2));
  image.thread_pool.queueJob(VertLineJobDouble::create(
                                   first_stage,image,size_x-1,1,size_y-2));
//  cout << "MainJob::execute finished" << endl;
  return false;
}

void MainJob::firstStageFinished(void) {
//  cout << "MainJob::firstStageFinished" << endl;
  image.thread_pool.queueJob(
                      RectContentsJob
//   FullRectJob
                        ::create(this,image,0,0,size_x,size_y));
}

FreeList MainJob::free_list;

bool JobQueue::NodeIsLess(const JobQueue::Node &a,const JobQueue::Node &b,
                          const void *user_data) {
  return (
    a.job->getDistance((const int*)user_data) <
    b.job->getDistance((const int*)user_data));
}

