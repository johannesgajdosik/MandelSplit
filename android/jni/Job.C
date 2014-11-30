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

#include "Job.H"
#include "MandelImage.H"
#include "ThreadPool.H"

#include <string.h> // memset
#include <stdlib.h>

#include "Logger.H"

#include "GLee.h"

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
  bool execute(void) {return false;}
  static FreeList free_list;
};

FreeList FirstStageJob::free_list;


class LineJob : public ChildJob {
protected:
  LineJob(Job *parent,
          const MandelImage &image,int x,int y,unsigned int *d,
          double re,double im,int size)
    : ChildJob(parent),image(image),x(x),y(y),
      d(d),re(re),im(im),size(size) {}
public:
  void *operator new(size_t size) {
    if (size != sizeof(LineJob)) ABORT();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
protected:
  const MandelImage &image;
  const int x;
  const int y;
  unsigned int *const d;
  double re,im;
  int size;
  static FreeList free_list;
};

FreeList LineJob::free_list;

class HorzLineJob : public LineJob {
public:
  static inline HorzLineJob *create(Job *parent,
                                    const MandelImage &image,int x,int y,
                                    int size_x) {
    return create(parent,image,x,y,image.data+y*image.screen_width+x,
                  x*image.d_re_im,y*image.d_re_im+image.start_im,size_x);
  }
  static inline HorzLineJob *create(Job *parent,
                                    const MandelImage &image,int x,int y,
                                    unsigned int *d,double re,double im,
                                    int size_x);
protected:
  HorzLineJob(Job *parent,
              const MandelImage &image,int x,int y,unsigned int *d,
              double re,double im,int size_x)
    : LineJob(parent,image,x,y,d,re,im,size_x) {}
private:
  void print(std::ostream &o) const {
    o << "HorzLineJob(" << x << ',' << y << ',' << size << ')';
  }
  bool execute(void);
  void drawToTexture(void) const {
    glTexSubImage2D(GL_TEXTURE_2D,0,
                    x,y,size,1,
                    GL_RGBA,GL_UNSIGNED_BYTE,d);
  }
};

class RecalcLimitHorzLineJob : public HorzLineJob {
public:
  RecalcLimitHorzLineJob(Job *parent,
                         const MandelImage &image,int x,int y,unsigned int *d,
                         double re,double im,int size_x)
    : HorzLineJob(parent,image,x,y,d,re,im,size_x) {}
private:
  void print(std::ostream &o) const {
    o << "RecalcLimitHorzLineJob(" << x << ',' << y << ',' << size << ')';
  }
  bool execute(void);
};

inline HorzLineJob *HorzLineJob::create(Job *parent,
                                        const MandelImage &image,int x,int y,
                                        unsigned int *d,double re,double im,
                                        int size_x) {
  return
     (image.recalc_limit > 0)
   ? new RecalcLimitHorzLineJob(parent,image,x,y,d,re,im,size_x)
   : new HorzLineJob(parent,image,x,y,d,re,im,size_x);
}

bool RecalcLimitHorzLineJob::execute(void) {
  VECTOR_TYPE mr[VECTOR_SIZE];
  VECTOR_TYPE mi[VECTOR_SIZE];
  unsigned int tmp[VECTOR_SIZE];
  unsigned int *pos[VECTOR_SIZE];
  for (int i=0;i<VECTOR_SIZE;i++) mi[i] = im;
  unsigned int *d = HorzLineJob::d;
  int x = HorzLineJob::x;
  int size_x = HorzLineJob::size;
  int vector_count = 0;
  int count = 0;
  for (;size_x>0;size_x--,d++,x++,re+=image.d_re_im) {
    if (terminate_flag) {
        // if max_iter has increased, mark remaining black pixels dirty:
      if (image.max_iter > image.recalc_limit) {
        for (int i=0;i<vector_count;i++) *(pos[i]) |= 0x80000000;
        do {
          if (*d >= image.recalc_limit) *d |= 0x80000000;
          d++;
          size_x--;
        } while (size_x > 0);
      }
      goto exit_loop;
    }
    if (size_x > VECTOR_SIZE && image.nr_of_waiting_threads) {
      const int size_x0 = ((size_x/2)+(VECTOR_SIZE-1)) & (~(VECTOR_SIZE-1));
      image.thread_pool.queueJob(new RecalcLimitHorzLineJob(
                                       getParent(),image,x+size_x0,y,
                                       d+size_x0,
                                       re+size_x0*image.d_re_im,im,
                                       size_x-size_x0));
      HorzLineJob::size -= (size_x-size_x0);
      size_x = size_x0;
    }
      // process pixel at (re,im)=(x,y)=*d
  if (image.needRecalc(*d)) {
      pos[vector_count] = d;
      mr[vector_count] = image.start_re+re;
      vector_count++;
      if (vector_count >= VECTOR_SIZE) {
        vector_count = 0;
        JULIA_FUNC(mr,mi,image.max_iter,tmp);
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
    JULIA_FUNC(mr,mi,image.max_iter,tmp);
    count += vector_count;
    for (int i=0;i<vector_count;i++) *(pos[i]) = tmp[i];
  }
  __atomic_add(&image.pixel_count,count);
  exit_loop:
  resetParent();
  return true;
}

bool HorzLineJob::execute(void) {
//if (y == 0)
//cout << "HorzLineJob(" << x << ',' << y << ',' << size
//     << ")::execute begin" << endl;
  VECTOR_TYPE mr[VECTOR_SIZE];
  VECTOR_TYPE mi[VECTOR_SIZE];
  for (int i=0;i<VECTOR_SIZE;i++) mi[i] = im;
  unsigned int *d = HorzLineJob::d;
  int x = HorzLineJob::x;
  int size_x = HorzLineJob::size;
  int count = 0;
  for (;size_x>=VECTOR_SIZE;x+=VECTOR_SIZE,size_x-=VECTOR_SIZE,d+=VECTOR_SIZE) {
    if (terminate_flag) goto exit_loop;
    if (size_x > VECTOR_SIZE && image.nr_of_waiting_threads) {
      const int size_x0 = ((size_x/2)+(VECTOR_SIZE-1)) & (~(VECTOR_SIZE-1));
      image.thread_pool.queueJob(new HorzLineJob(getParent(),image,x+size_x0,y,
                                                 d+size_x0,
                                                 re+size_x0*image.d_re_im,im,
                                                 size_x-size_x0));
//if (y == 0)
//cout << "HorzLineJob: split off: " << x+size_x0 << ", " << size_x-size_x0
//     << endl;
      HorzLineJob::size -= (size_x-size_x0);
      size_x = size_x0;
//if (y == 0)
//cout << "HorzLineJob: split rem: " << HorzLineJob::x
//     << ", " << HorzLineJob::size << endl;
    }
    for (int i=0;i<VECTOR_SIZE;i++,re+=image.d_re_im) {
#ifdef DEBUG
      if (d[i]) {
        cout << "HorzLineJob::execute: double drawing" << endl;
        ABORT();
      }
#endif
      mr[i] = image.start_re+re;
    }
    JULIA_FUNC(mr,mi,image.max_iter,d);
    count += VECTOR_SIZE;
  }
  if (size_x > 0) {
    if (terminate_flag) goto exit_loop;
    unsigned int tmp[VECTOR_SIZE];
    for (int i=0;i<VECTOR_SIZE;i++,re+=image.d_re_im) {
      mr[i] = image.start_re+re;
    }
    JULIA_FUNC(mr,mi,image.max_iter,tmp);
    count += size;
    for (int i=0;i<size_x;i++) {
#ifdef DEBUG
      if (d[i]) {
        cout << "HorzLineJob::execute: double drawing2" << endl;
        ABORT();
      }
#endif
      d[i] = tmp[i];
    }
  }
  __atomic_add(&image.pixel_count,count);
  exit_loop:
  resetParent();
//if (y == 0)
//cout << "HorzLineJob::execute end" << endl;
  return true;
}



class VertLineJob : public LineJob {
public:
  static inline VertLineJob *create(Job *parent,
                                    const MandelImage &image,int x,int y,
                                    int size_y) {
    return create(parent,image,x,y,image.data+y*image.screen_width+x,
                  x*image.d_re_im+image.start_re,y*image.d_re_im,size_y);
  }
  static inline VertLineJob *create(Job *parent,
                                    const MandelImage &image,int x,int y,
                                    unsigned int *d,double re,double im,
                                    int size_y);
protected:
  VertLineJob(Job *parent,
              const MandelImage &image,int x,int y,unsigned int *d,
              double re,double im,int size_y)
    : LineJob(parent,image,x,y,d,re,im,size_y) {}
private:
  void print(std::ostream &o) const {
    o << "VertLineJob(" << x << ',' << y << ',' << size << ')';
  }
  bool execute(void);
  void drawToTexture(void) const {
#ifdef __ANDROID__
    int h = size;
    int y = VertLineJob::y;
    const unsigned int *d = VertLineJob::d;
    do {
      glTexSubImage2D(GL_TEXTURE_2D,0,
                      x,y,1,1,
                      GL_RGBA,GL_UNSIGNED_BYTE,d);
      h--;
      y++;
      d += image.screen_width;
    } while (h > 0);
#else
    glTexSubImage2D(GL_TEXTURE_2D,0,
                    x,y,1,size,
                    GL_RGBA,GL_UNSIGNED_BYTE,d);
#endif
  }
};

class RecalcLimitVertLineJob : public VertLineJob {
public:
  RecalcLimitVertLineJob(Job *parent,
                         const MandelImage &image,int x,int y,unsigned int *d,
                         double re,double im,int size_y)
    : VertLineJob(parent,image,x,y,d,re,im,size_y) {}
private:
  void print(std::ostream &o) const {
    o << "RecalcLimitVertLineJob(" << x << ',' << y << ',' << size << ')';
  }
  bool execute(void);
};

inline VertLineJob *VertLineJob::create(Job *parent,
                                        const MandelImage &image,int x,int y,
                                        unsigned int *d,double re,double im,
                                        int size_y) {
  return
     (image.recalc_limit > 0)
   ? new RecalcLimitVertLineJob(parent,image,x,y,d,re,im,size_y)
   : new VertLineJob(parent,image,x,y,d,re,im,size_y);
}

bool RecalcLimitVertLineJob::execute(void) {
  VECTOR_TYPE mr[VECTOR_SIZE];
  VECTOR_TYPE mi[VECTOR_SIZE];
  unsigned int tmp[VECTOR_SIZE];
  unsigned int *pos[VECTOR_SIZE];
  for (int i=0;i<VECTOR_SIZE;i++) mr[i] = re;
  unsigned int *d = VertLineJob::d;
  int y = VertLineJob::y;
  int size_y = VertLineJob::size;
  int vector_count = 0;
  int count = 0;
  for (;size_y>0;size_y--,d+=image.screen_width,y++,im+=image.d_re_im) {
    if (terminate_flag) {
        // if max_iter has increased, mark remaining black pixels dirty:
      if (image.max_iter > image.recalc_limit) {
        for (int i=0;i<vector_count;i++) *(pos[i]) |= 0x80000000;
        do {
          if (*d >= image.recalc_limit) *d |= 0x80000000;
          d += image.screen_width;
          size_y--;
        } while (size_y > 0);
      }
      goto exit_loop;
    }
    if (size_y > VECTOR_SIZE && image.nr_of_waiting_threads) {
      const int size_y0 = ((size_y/2)+(VECTOR_SIZE-1)) & (~(VECTOR_SIZE-1));
      image.thread_pool.queueJob(new RecalcLimitVertLineJob(
                                       getParent(),image,x,y+size_y0,
                                       d+size_y0*image.screen_width,
                                       re,im+size_y0*image.d_re_im,
                                       size_y-size_y0));
      VertLineJob::size -= (size_y-size_y0);
      size_y = size_y0;
    }
      // process pixel at (re,im)=(x,y)=*d
    if (image.needRecalc(*d)) {
      pos[vector_count] = d;
      mi[vector_count] = image.start_im+im;
      vector_count++;
      if (vector_count >= VECTOR_SIZE) {
        vector_count = 0;
        JULIA_FUNC(mr,mi,image.max_iter,tmp);
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
    JULIA_FUNC(mr,mi,image.max_iter,tmp);
    count += vector_count;
    for (int i=0;i<vector_count;i++) *(pos[i]) = tmp[i];
  }
  __atomic_add(&image.pixel_count,count);
  exit_loop:
  resetParent();
  return true;
}

bool VertLineJob::execute(void) {
//cout << "VertLineJob(" << x << ',' << y << ',' << size
//     << ")::execute begin: " << re << endl;
  if (size <= 0) ABORT();
  VECTOR_TYPE mr[VECTOR_SIZE];
  VECTOR_TYPE mi[VECTOR_SIZE];
  for (int i=0;i<VECTOR_SIZE;i++) mr[i] = re;
  unsigned int tmp[VECTOR_SIZE];
  unsigned int *d = VertLineJob::d;
  int size_y = VertLineJob::size;
  int count = 0;
  for (int y=VertLineJob::y;size_y>VECTOR_SIZE;
       y+=VECTOR_SIZE,size_y-=VECTOR_SIZE) {
    if (terminate_flag) goto exit_loop;
    if (image.nr_of_waiting_threads) {
      const int size_y0 = ((size_y/2)+(VECTOR_SIZE-1)) & (~(VECTOR_SIZE-1));
      image.thread_pool.queueJob(new VertLineJob(
                                       getParent(),image,x,y+size_y0,
                                       d+size_y0*image.screen_width,
                                       re,im+size_y0*image.d_re_im,
                                       size_y-size_y0));
//if (x == 399)
//cout << "VertLineJob: split off: " << y+size_y0 << ", " << size_y-size_y0
//     << endl;
      VertLineJob::size -= (size_y-size_y0);
      size_y = size_y0;
      if (size_y <= VECTOR_SIZE) break;
    }
    for (int j=0;j<VECTOR_SIZE;j++,im+=image.d_re_im) {
      mi[j] = image.start_im+im;
    }
    JULIA_FUNC(mr,mi,image.max_iter,tmp);
    count += VECTOR_SIZE;
    for (int j=0;j<VECTOR_SIZE;j++,d+=image.screen_width) {
      *d = tmp[j];
    }
  }
  if (terminate_flag) goto exit_loop;
  for (int j=0;j<VECTOR_SIZE;j++,im+=image.d_re_im) {
    mi[j] = image.start_im+im;
  }
  JULIA_FUNC(mr,mi,image.max_iter,tmp);
  count += size_y;
  for (int j=0;j<size_y;j++,d+=image.screen_width) {
    *d = tmp[j];
  }
  __atomic_add(&image.pixel_count,count);
  exit_loop:
  resetParent();
//cout << "VertLineJob(" << VertLineJob::x << ',' << VertLineJob::y
//     << ")::execute end" << endl;
  return true;
}


class FillRectJob : public ChildJob {
public:
  FillRectJob(Job *parent,
              const MandelImage &image,int x,int y,int size_x,int size_y)
    : ChildJob(parent),
      image(image),x(x),y(y),size_x(size_x),size_y(size_y) {}
  void *operator new(size_t size) {
    if (size != sizeof(FillRectJob)) ABORT();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
private:
  void print(std::ostream &o) const {
    o << "FillRectJob(" << x << ',' << y << ',' << size_x << ',' << size_y << ')';
  }
  bool execute(void) {
    int count = 0;
    unsigned int *d = image.data + y*image.screen_width + x;
    for (int j=size_y;j>0;j--,d+=image.screen_width) {
      for (int i=0;i<size_x;i++) {
#ifdef DEBUG
        if (d[i]) {
          cout << "FillRectJob::execute: double drawing" << endl;
          ABORT();
        }
#endif
        if (image.needRecalc(d[i])) count++;
        d[i] =
#ifdef DEBUG
                   0xFFFFFFFF;
#else
                   image.max_iter;
#endif
      }
    }
    __atomic_add(&image.pixel_count,count);
    resetParent();
    return true;
  }
  void drawToTexture(void) const {
    const unsigned int *d = image.data+x+y*(image.screen_width);
#ifdef __ANDROID__
    int h = size_y;
    int y = FillRectJob::y;
    do {
      glTexSubImage2D(GL_TEXTURE_2D,0,
                      x,y,size_x,1,
                      GL_RGBA,GL_UNSIGNED_BYTE,d);
      h--;
      y++;
      d += image.screen_width;
    } while (h > 0);
#else
    glTexSubImage2D(GL_TEXTURE_2D,0,
                    x,y,size_x,size_y,
                    GL_RGBA,GL_UNSIGNED_BYTE,d);
#endif
  }
private:
  const MandelImage &image;
  const int x;
  const int y;
  const int size_x;
  const int size_y;
  static FreeList free_list;
};

FreeList FillRectJob::free_list;



class FullRectJob : public ChildJob {
public:
  FullRectJob(Job *parent,
              const MandelImage &image,int x,int y,int size_x,int size_y)
    : ChildJob(parent),image(image),x(x),y(y),
      d(image.data+y*image.screen_width+x),
      re(x*image.d_re_im),im(y*image.d_re_im),
//      re(x*image.d_re_im+image.start_re),im(y*image.d_re_im+image.start_im),
      size_x(size_x),size_y(size_y) {}
//  FullRectJob(Job *parent,
//              const MandelImage &image,int x,int y,unsigned int *d,
//              double re,double im,int size_x,int size_y)
//    : ChildJob(parent),image(image),x(x),y(y),
//      d(d),re(re),im(im),
//      size_x(size_x),size_y(size_y) {}
  void *operator new(size_t size) {
    if (size != sizeof(VertLineJob)) ABORT();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
private:
  void print(std::ostream &o) const {o << "FullRectJob" << endl;}
  bool execute(void);
private:
  const MandelImage &image;
  const int x;
  const int y;
  unsigned int *const d;
  double re,im;
  const int size_x;
  const int size_y;
  static FreeList free_list;
};

FreeList FullRectJob::free_list;

bool FullRectJob::execute(void) {
  unsigned int *d = FullRectJob::d;
  int w = size_x & (~(VECTOR_SIZE-1));
  double yh = im;
  for (int j=0;j<size_y;j++,d+=image.screen_width,yh+=image.d_re_im) {
    if (terminate_flag) {
      if (image.recalc_limit > 0 && image.max_iter > image.recalc_limit) {
          // mark remaining black pixels dirty:
        for (;j<size_y;j++,d+=image.screen_width) {
          for (int i=0;i<w;i++) {
            if (d[i] >= image.recalc_limit) d[i] |= 0x80000000;
          }
        }
        break;
      }
      return false;
    }
    image.thread_pool.queueJob(
                        HorzLineJob::create(
                          this,image,x,y+j,d,re,yh+image.start_im,w));
  }
  d = FullRectJob::d+w;
  re += w*image.d_re_im;
  for (;w<size_x;w++,d++,re+=image.d_re_im) {
    if (terminate_flag) {
      if (image.recalc_limit > 0 && image.max_iter > image.recalc_limit) {
          // mark remaining black pixels dirty:
        w = size_x - w;
        for (int j=0;j<size_y;j++,d+=image.screen_width) {
          for (int i=0;i<w;i++) {
            if (d[i] >= image.recalc_limit) d[i] |= 0x80000000;
          }
        }
        break;
      }
      return false;
    }
    image.thread_pool.queueJob(
                        VertLineJob::create(
                          this,image,x+w,y,d,re+image.start_re,im,size_y));
  }
  return false;
}

//using __gnu_cxx::__exchange_and_add;
//static volatile _Atomic_word job_id_sequence = 0;
//        id(__exchange_and_add(&job_id_sequence,1)),

class RectContentsJob : public ChildJob {
public:
  RectContentsJob(Job *parent,
                  const MandelImage &image,int x,int y,int size_x,int size_y)
      : ChildJob(parent),
        image(image),x(x),y(y),size_x(size_x),size_y(size_y) {
//    cout << "RectContentsJob::RectContentsJob("
//         << x << ',' << y << ',' << size_x << ',' << size_y << ')'
//         << endl;
  }
  ~RectContentsJob(void) {
//    cout << "RectContentsJob::~RectContentsJob"
//         << endl;
  }
  void *operator new(size_t size) {
    if (size != sizeof(RectContentsJob)) ABORT();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
private:
  void print(std::ostream &o) const {o << "RectContentsJob";}
  bool execute(void);
  class HorzFirstStageJob : public FirstStageJob {
  public:
    HorzFirstStageJob(RectContentsJob *parent) : FirstStageJob(parent) {}
  private:
    void print(std::ostream &o) const {o << "HorzFirstStageJob";}
    ~HorzFirstStageJob(void) {
      static_cast<RectContentsJob*>(getParent())->firstStageHorzFinished();
    }
  };
  void firstStageHorzFinished(void);
  class VertFirstStageJob : public FirstStageJob {
  public:
    VertFirstStageJob(RectContentsJob *parent) : FirstStageJob(parent) {}
  private:
    void print(std::ostream &o) const {o << "VertFirstStageJob";}
    ~VertFirstStageJob(void) {
      static_cast<RectContentsJob*>(getParent())->firstStageVertFinished();
    }
  };
  void firstStageVertFinished(void);
private:
  const MandelImage &image;
  const int x;
  const int y;
  const int size_x;
  const int size_y;
  static FreeList free_list;
};

FreeList RectContentsJob::free_list;

void RectContentsJob::firstStageHorzFinished(void) {
  if (terminate_flag &&
      (image.recalc_limit == 0 || image.max_iter <= image.recalc_limit)) return;
  const int wh = size_x / 2;
  image.thread_pool.queueJob(new RectContentsJob(
                                   this,image,x,y,wh+1,size_y));
  image.thread_pool.queueJob(new RectContentsJob(
                                   this,image,x+wh,y,size_x-wh,size_y));
}

void RectContentsJob::firstStageVertFinished(void) {
  if (terminate_flag &&
      (image.recalc_limit == 0 || image.max_iter <= image.recalc_limit)) return;
  const int hh = size_y / 2;
  image.thread_pool.queueJob(new RectContentsJob(
                                   this,image,x,y,size_x,hh+1));
  image.thread_pool.queueJob(new RectContentsJob(
                                   this,image,x,y+hh,size_x,size_y-hh));
}

bool RectContentsJob::execute(void) {
  if (terminate_flag) {
    if (image.recalc_limit > 0 && image.max_iter > image.recalc_limit) {
        // mark remaining black pixels dirty:
      unsigned int *d = image.data + (y+1)*image.screen_width + (x+1);
      for (int j=size_y-3;j>=0;j--,d+=image.screen_width) {
        for (int i=size_x-3;i>=0;i--) {
          if (d[i] >= image.recalc_limit) d[i] |= 0x80000000;
        }
      }
    }
  } else {
      //cout << "RectContentsJob::execute begin" << endl;
      // check if must split
    unsigned int *data = image.data + y*image.screen_width + x;
    const unsigned int *d0 = data;
    const unsigned int *d1 = d0+(size_y-1)*image.screen_width;
    int count_max = 0;
    int count_other = 0;
#ifdef DEBUG
    int count_zero = 0;
#endif
    for (int i=0;i<size_x;i++,d0++,d1++) {
#ifdef DEBUG
      if (*d0 == 0) count_zero++;
      if (*d1 == 0) count_zero++;
#endif
      if (*d0 < image.max_iter) count_other++;
      else count_max++;
      if (*d1 < image.max_iter) count_other++;
      else count_max++;
    }
    d0 = data + image.screen_width;
    d1 = d0 + (size_x-1);
    for (int i=2;i<size_y;i++,d0+=image.screen_width,d1+=image.screen_width) {
#ifdef DEBUG
      if (*d0 == 0) count_zero++;
      if (*d1 == 0) count_zero++;
#endif
      if (*d0 < image.max_iter) count_other++;
      else count_max++;
      if (*d1 < image.max_iter) count_other++;
      else count_max++;
    }
#ifdef DEBUG
    if (count_zero) {
      cout << "RectContentsJob::execute: count_zero=" << count_zero
           << ", count_other=" << count_other
           << ", count_max=" << count_max
           << endl;
    }
#endif
    if (count_other == 0) {
        // everything is inside
      image.thread_pool.queueJob(new FillRectJob(
                                       this,image,x+1,y+1,size_x-2,size_y-2));
    } else if (count_max == 0 && size_x < 100 && size_y < 100) {
        // do not bisect any more
      image.thread_pool.queueJob(new FullRectJob(
                                       this,image,x+1,y+1,size_x-2,size_y-2));
    } else {
#ifdef DEBUG
      image.assertEmpty(x+1,y+1,size_x-2,size_y-2);
#endif
      if (size_x > size_y) {
          // split horizontally
        if (size_x <= 10) {
            // finish recursion, just draw
    //      image.fullRect(x+1,y+1,size_x-2,size_y-2);
          image.thread_pool.queueJob(new FullRectJob(
                                           this,image,
                                           x+1,y+1,size_x-2,size_y-2));
        } else {
          const int wh = size_x / 2;
          Job *first_stage(new HorzFirstStageJob(this));
          image.thread_pool.queueJob(VertLineJob::create(
                                                    first_stage,image,
                                                    x+wh,y+1,size_y-2));
        }
      } else {
          // split vertically
        if (size_y <= 10) {
            // finish recursion, just draw
    //      image.fullRect(x+1,y+1,size_x-2,size_y-2);
          image.thread_pool.queueJob(new FullRectJob(
                                           this,image,
                                           x+1,y+1,size_x-2,size_y-2));
        } else {
          const int hh = size_y / 2;
          Job *first_stage(new VertFirstStageJob(this));
          image.thread_pool.queueJob(HorzLineJob::create(
                                                    first_stage,image,
                                                    x+1,y+hh,size_x-2));
        }
      }
    }
  }
//cout << "RectContentsJob::execute end" << endl;
  return false;
}


MainJob::MainJob(const MandelImage &image,
                 int size_x,int size_y)
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
  unsigned int *d = image.data;
  for (int i=0;i<size_y;i++,d+=image.screen_width) {
    memset(d,0,sizeof(unsigned int)*size_x);
  }
#endif
  Job *first_stage(new MainFirstStageJob(this));
  image.thread_pool.queueJob(HorzLineJob::create(
                                   first_stage,image,0,0,size_x));
  image.thread_pool.queueJob(HorzLineJob::create(
                                   first_stage,image,0,size_y-1,size_x));
  image.thread_pool.queueJob(VertLineJob::create(
                                   first_stage,image,0,1,size_y-2));
  image.thread_pool.queueJob(VertLineJob::create(
                                   first_stage,image,size_x-1,1,size_y-2));
//  cout << "MainJob::execute finished" << endl;
  return false;
}

void MainJob::firstStageFinished(void) {
//  cout << "MainJob::firstStageFinished" << endl;
  image.thread_pool.queueJob(new RectContentsJob(
                                   this,image,0,0,size_x,size_y));
}

FreeList MainJob::free_list;

