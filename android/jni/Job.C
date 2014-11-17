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

FreeList JobQueueBase::free_list;

class FirstStageJob : public ChildJob {
public:
  FirstStageJob(Job *parent) : ChildJob(parent) {}
  void *operator new(size_t size) {
    if (size != sizeof(FirstStageJob)) abort();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
private:
  void print(std::ostream &o) const {o << "FirstStageJob" << endl;}
  bool execute(void) {return false;}
  static FreeList free_list;
};

FreeList __attribute__((aligned(16))) FirstStageJob::free_list;


class HorzLineJob : public ChildJob {
public:
  HorzLineJob(Job *parent,
              const MandelImage &image,int x,int y,int size_x)
    : ChildJob(parent),image(image),x(x),y(y),
      d(image.data+y*image.screen_width+x),
      re(x*image.d_re_im),im(y*image.d_re_im+image.start_im),size_x(size_x) {
//cout << "HorzLineJob::HorzLineJob(" << x << ',' << y << ',' << size_x
//     << ')' << endl;
  }
  HorzLineJob(Job *parent,
              const MandelImage &image,int x,int y,unsigned int *d,
              double re,double im,int size_x)
    : ChildJob(parent),image(image),x(x),y(y),
      d(d),re(re),im(im),size_x(size_x) {}
  ~HorzLineJob(void) {
//cout << "HorzLineJob" << x << ',' << y
//     << "::~HorzLineJob()" << endl;
  }
  void *operator new(size_t size) {
    if (size != sizeof(HorzLineJob)) abort();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
private:
  void print(std::ostream &o) const {
    o << "HorzLineJob(" << x << ',' << y << ',' << size_x << ')' << endl;
  }
  bool execute(void);
  bool needsDrawing(int &x,int &y,int &w,int &h) const {
    x = HorzLineJob::x;
    y = HorzLineJob::y;
    w = size_x;
    h = 1;
    return true;
  }
private:
  const MandelImage &image;
  const int x;
  const int y;
  unsigned int *const d;
  double re,im;
  int size_x;
  static FreeList free_list;
};

FreeList HorzLineJob::free_list;

bool HorzLineJob::execute(void) {
//if (y == 0)
//cout << "HorzLineJob(" << x << ',' << y << ',' << size_x
//     << ")::execute begin" << endl;

  VECTOR_TYPE mr[VECTOR_SIZE];
  VECTOR_TYPE mi[VECTOR_SIZE];
  for (int i=0;i<VECTOR_SIZE;i++) mi[i] = im;
  unsigned int *d = HorzLineJob::d;
  int x = HorzLineJob::x;
  int size_x = HorzLineJob::size_x;
  for (;size_x>=VECTOR_SIZE;x+=VECTOR_SIZE,size_x-=VECTOR_SIZE,d+=VECTOR_SIZE) {
    if (image.terminate_flag) goto exit_loop;
    if (size_x > VECTOR_SIZE && image.nr_of_waiting_threads) {
      const int size_x0 = ((size_x/2)+(VECTOR_SIZE-1)) & (~(VECTOR_SIZE-1));
      image.thread_pool.queueJob(new HorzLineJob(getParent(),image,x+size_x0,y,
                                                 d+size_x0,
                                                 re+size_x0*image.d_re_im,im,
                                                 size_x-size_x0));
//if (y == 0)
//cout << "HorzLineJob: split off: " << x+size_x0 << ", " << size_x-size_x0
//     << endl;
      HorzLineJob::size_x -= (size_x-size_x0);
      size_x = size_x0;
//if (y == 0)
//cout << "HorzLineJob: split rem: " << HorzLineJob::x
//     << ", " << HorzLineJob::size_x << endl;
    }
    for (int i=0;i<VECTOR_SIZE;i++,re+=image.d_re_im) {
#ifdef DEBUG
      if (d[i]) {
        cout << "HorzLineJob::execute: double drawing" << endl;
        abort();
      }
#endif
      mr[i] = image.start_re+re;
    }
    JULIA_FUNC(mr,mi,image.max_iter,d);
  }
  if (size_x > 0) {
    if (terminate_flag) goto exit_loop;
    unsigned int tmp[VECTOR_SIZE];
    for (int i=0;i<VECTOR_SIZE;i++,re+=image.d_re_im) {
      mr[i] = image.start_re+re;
    }
    JULIA_FUNC(mr,mi,image.max_iter,tmp);
    for (int i=0;i<size_x;i++) {
#ifdef DEBUG
      if (d[i]) {
        cout << "HorzLineJob::execute: double drawing2" << endl;
        abort();
      }
#endif
      d[i] = tmp[i];
    }
  }
  exit_loop:
//  resetParent();
//if (y == 0)
//cout << "HorzLineJob::execute end" << endl;
  return true;
}



class VertLineJob : public ChildJob {
public:
  VertLineJob(Job *parent,
              const MandelImage &image,int x,int y,int size_y)
    : ChildJob(parent),image(image),x(x),y(y),
      d(image.data+y*image.screen_width+x),
      re(x*image.d_re_im+image.start_re),im(y*image.d_re_im),size_y(size_y) {
//    cout << "VertLineJob(" << x << ',' << y << ',' << size_y << "): " << re << endl;
  }
  VertLineJob(Job *parent,
              const MandelImage &image,int x,int y,unsigned int *d,
              double re,double im,int size_y)
    : ChildJob(parent),image(image),x(x),y(y),
      d(d),re(re),im(im),size_y(size_y) {
//    cout << "VertLineJob2(" << x << ',' << y << ',' << size_y << "): " << re << endl;
  }
  void *operator new(size_t size) {
    if (size != sizeof(VertLineJob)) abort();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
private:
  void print(std::ostream &o) const {
    o << "VertLineJob(" << x << ',' << y << ',' << size_y << ')' << endl;
  }
  bool execute(void);
  bool needsDrawing(int &x,int &y,int &w,int &h) const {
    x = VertLineJob::x;
    y = VertLineJob::y;
    w = 1;
    h = size_y;
    return true;
  }
private:
  const MandelImage &image;
  const int x;
  const int y;
  unsigned int *const d;
  double re,im;
  int size_y;
  static FreeList free_list;
};

FreeList VertLineJob::free_list;

bool VertLineJob::execute(void) {
//cout << "VertLineJob(" << x << ',' << y << ',' << size_y
//     << ")::execute begin: " << re << endl;
  if (size_y <= 0) abort();
  VECTOR_TYPE mr[VECTOR_SIZE];
  VECTOR_TYPE mi[VECTOR_SIZE];
  for (int i=0;i<VECTOR_SIZE;i++) mr[i] = re;
  unsigned int tmp[VECTOR_SIZE];
  unsigned int *d = VertLineJob::d;
  int size_y = VertLineJob::size_y;
  for (int y=VertLineJob::y;;y+=VECTOR_SIZE) {
    if (terminate_flag) goto exit_loop;
    if (size_y > VECTOR_SIZE && image.nr_of_waiting_threads) {
      const int size_y0 = ((size_y/2)+(VECTOR_SIZE-1)) & (~(VECTOR_SIZE-1));
      image.thread_pool.queueJob(new VertLineJob(getParent(),image,x,y+size_y0,
                                                 d+size_y0*image.screen_width,
                                                 re,im+size_y0*image.d_re_im,
                                                 size_y-size_y0));
//if (x == 399)
//cout << "VertLineJob: split off: " << y+size_y0 << ", " << size_y-size_y0
//     << endl;
      VertLineJob::size_y -= (size_y-size_y0);
      size_y = size_y0;
    }
    for (int j=0;j<VECTOR_SIZE;j++,im+=image.d_re_im) {
      mi[j] = image.start_im+im;
    }
    JULIA_FUNC(mr,mi,image.max_iter,tmp);
//cout << "VertLineJob(" << x << ',' << y << ',' << size_y
//     << ")::execute: " << mr[0] << ',' << mi[0] << ',' << tmp[0] << endl;
    for (int j=0;j<VECTOR_SIZE;j++,d+=image.screen_width) {
      *d = tmp[j];
      if (--size_y <= 0) goto exit_loop;
    }
  }
exit_loop:
//  resetParent();
//cout << "VertLineJob(" << VertLineJob::x << ',' << VertLineJob::y
//     << ")::execute end" << endl;
  return true;
}



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
    if (size != sizeof(VertLineJob)) abort();
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
    image.thread_pool.queueJob(new HorzLineJob(this,image,x,y+j,d,re,yh+image.start_im,w));
    if (terminate_flag) return false;
  }

  d = FullRectJob::d+w;
  re += w*image.d_re_im;
  for (;w<size_x;w++,d++,re+=image.d_re_im) {
    image.thread_pool.queueJob(new VertLineJob(this,image,x+w,y,d,re+image.start_re,im,size_y));
    if (terminate_flag) return false;
  }
  return false;
}

class FillRectJob : public ChildJob {
public:
  FillRectJob(Job *parent,
              const MandelImage &image,int x,int y,int size_x,int size_y)
    : ChildJob(parent),
      image(image),x(x),y(y),size_x(size_x),size_y(size_y) {}
  void *operator new(size_t size) {
    if (size != sizeof(FillRectJob)) abort();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
private:
  void print(std::ostream &o) const {
    o << "FillRectJob(" << x << ',' << y << ',' << size_x << ',' << size_y << ')' << endl;
  }
  bool execute(void) {
    image.fillRect(x,y,size_x,size_y,
#ifdef DEBUG
                   0xFFFFFFFF
#else
                   image.max_iter
#endif
                  );
    return true;
  }
  bool needsDrawing(int &x,int &y,int &w,int &h) const {
    x = FillRectJob::x;
    y = FillRectJob::y;
    w = size_x;
    h = size_y;
    return true;
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


using __gnu_cxx::__exchange_and_add;
static volatile _Atomic_word job_id = 0;

class RectContentsJob : public ChildJob {
public:
  RectContentsJob(Job *parent,ThreadPool &pool,
                  const MandelImage &image,int x,int y,int size_x,int size_y)
      : ChildJob(parent),
        id(__exchange_and_add(&job_id,1)),
        pool(pool),
        image(image),x(x),y(y),size_x(size_x),size_y(size_y) {
//    cout << "RectContentsJob::RectContentsJob(" << id
//         << ';' << x << ',' << y << ',' << size_x << ',' << size_y << ')'
//         << endl;
  }
  ~RectContentsJob(void) {
//    cout << "RectContentsJob::~RectContentsJob(" << id << ')'
//         << endl;
  }
  void *operator new(size_t size) {
    if (size != sizeof(RectContentsJob)) abort();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
private:
  void print(std::ostream &o) const {o << "RectContentsJob" << endl;}
  bool execute(void);
  class HorzFirstStageJob : public FirstStageJob {
  public:
    HorzFirstStageJob(RectContentsJob *parent)
      : FirstStageJob(parent) {
    }
  private:
    void print(std::ostream &o) const {o << "HorzFirstStageJob" << endl;}
    void lastChildHasReleased(void) {
      static_cast<RectContentsJob*>(getParent())->firstStageHorzFinished();
      FirstStageJob::lastChildHasReleased();
    }
  };
  void firstStageHorzFinished(void);
  class VertFirstStageJob : public FirstStageJob {
  public:
    VertFirstStageJob(RectContentsJob *parent)
      : FirstStageJob(parent) {}
  private:
    void print(std::ostream &o) const {o << "VertFirstStageJob" << endl;}
    void lastChildHasReleased(void) {
      static_cast<RectContentsJob*>(getParent())->firstStageVertFinished();
      FirstStageJob::lastChildHasReleased();
    }
  };
  void firstStageVertFinished(void);
private:
  const unsigned int id;
  ThreadPool &pool;
  const MandelImage &image;
  const int x;
  const int y;
  const int size_x;
  const int size_y;
  static FreeList free_list;
};

FreeList RectContentsJob::free_list;

void RectContentsJob::firstStageHorzFinished(void) {
  if (terminate_flag) return;
  const int wh = size_x / 2;
  pool.queueJob(new RectContentsJob(this,pool,image,
                                    x,y,wh,size_y));
  pool.queueJob(new RectContentsJob(this,pool,image,
                                    x+(wh-1),y,size_x-(wh-1),size_y));
}

void RectContentsJob::firstStageVertFinished(void) {
  if (terminate_flag) return;
  const int hh = size_y / 2;
  pool.queueJob(new RectContentsJob(this,pool,image,
                                    x,y,size_x,hh));
  pool.queueJob(new RectContentsJob(this,pool,image,
                                    x,y+(hh-1),size_x,size_y-(hh-1)));
}

bool RectContentsJob::execute(void) {
//cout << "RectContentsJob(" << id
//     << ")::execute begin" << endl;
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
    pool.queueJob(new FillRectJob(this,image,x+1,y+1,size_x-2,size_y-2));
  } else if (count_max == 0 && size_x < 100 && size_y < 100) {
      // do not bisect any more
    pool.queueJob(new FullRectJob(this,image,x+1,y+1,size_x-2,size_y-2));
  } else {
#ifdef DEBUG
    image.assertEmpty(x+1,y+1,size_x-2,size_y-2);
#endif
    if (!terminate_flag) {
      if (size_x > size_y) {
          // split horizontally
        if (size_x <= 10) {
            // finish recursion, just draw
    //      image.fullRect(x+1,y+1,size_x-2,size_y-2);
          pool.queueJob(new FullRectJob(this,image,x+1,y+1,size_x-2,size_y-2));
        } else {
          const int wh = size_x / 2;
          Job *first_stage(new HorzFirstStageJob(this));
          pool.queueJob(new VertLineJob(first_stage,image,x+wh-1,y+1,size_y-2));
        }
      } else {
          // split vertically
        if (size_y <= 10) {
            // finish recursion, just draw
    //      image.fullRect(x+1,y+1,size_x-2,size_y-2);
          pool.queueJob(new FullRectJob(this,image,x+1,y+1,size_x-2,size_y-2));
        } else {
          const int hh = size_y / 2;
          Job *first_stage(new VertFirstStageJob(this));
          pool.queueJob(new HorzLineJob(first_stage,image,x+1,y+hh-1,size_x-2));
        }
      }
    }
  }
//cout << "RectContentsJob(" << id
//     << ")::execute end" << endl;
  return false;
}


MainJob::MainJob(ThreadPool &pool,
                 const MandelImage &image,
                 int size_x,int size_y)
        :Job(pool.terminate_flag),
         pool(pool),
         image(image),size_x(size_x),size_y(size_y) {
//  cout << "MainJob::MainJob" << endl;
}

void MainJob::lastChildHasReleased(void) {
//  cout << "MainJob::lastChildHasReleased" << endl;
  pool.mainJobHasTerminated();
}

class MainFirstStageJob : public FirstStageJob {
public:
  MainFirstStageJob(MainJob *parent)
    : FirstStageJob(parent) {}
  ~MainFirstStageJob(void) {
//    cout << "MainFirstStageJob::~MainFirstStageJob" << endl;
  }
private:
  void lastChildHasReleased(void) {
//    cout << "MainFirstStageJob::lastChildHasReleased" << endl;
    static_cast<MainJob*>(getParent())->firstStageFinished();
    FirstStageJob::lastChildHasReleased();
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
  pool.queueJob(new HorzLineJob(first_stage,image,0,0,size_x));
  pool.queueJob(new HorzLineJob(first_stage,image,0,size_y-1,size_x));
  pool.queueJob(new VertLineJob(first_stage,image,0,1,size_y-2));
  pool.queueJob(new VertLineJob(first_stage,image,size_x-1,1,size_y-2));
//  cout << "MainJob::execute finished" << endl;
  return false;
}

void MainJob::firstStageFinished(void) {
//  cout << "MainJob::firstStageFinished" << endl;
  if (!terminate_flag) {
    pool.queueJob(new RectContentsJob(this,pool,image,0,0,size_x,size_y));
  }
}

FreeList MainJob::free_list;

