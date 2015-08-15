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

#ifndef JOB_H_
#define JOB_H_

#include "IntrusivePtrCapable.H"
#include "LockfreeStack.H"
#include "Semaphore.H"
#include "IntrusivePtr.H"
#include "Logger.H"

#if (__GNUC__ >= 4)
  #if (__GNUC_MINOR__ >= 2)
    #include <ext/atomicity.h>
  #else
    #include <bits/atomicity.h>
  #endif
#else
  #include <bits/atomicity.h>
#endif

using __gnu_cxx::__atomic_add;
using __gnu_cxx::__exchange_and_add;

class MandelImage;

class FreeList {
private:
  struct Node {Node *next;};
  LockfreeStack<Node> stack;
public:
  static void *malloc(size_t size) {return ::operator new(size);}
  static void free(void *data) {::operator delete(data);}
  void push(void *data) {stack.push(reinterpret_cast<Node*>(data));}
  void *pop(void) {return stack.pop();}
  ~FreeList(void) {void *p;while ((p=stack.popNonThreadSafe())) free(p);}
};

class Job : public IntrusivePtrCapable {
protected:
  Job(volatile _Atomic_word &terminate_flag)
    : terminate_flag(terminate_flag) {}
public:
  typedef IntrusivePtr<Job> Ptr;
  virtual ~Job(void) {}
  virtual bool isFirstStageJob(void) const {return false;}
  virtual int getDistance(const int xy[2]) const = 0;
  virtual void print(std::ostream &o) const = 0;
  virtual bool execute(void) = 0;
  virtual void drawToTexture(void) const {}
public:
  volatile _Atomic_word &terminate_flag;
};

static inline
std::ostream &operator<<(std::ostream &o,const Job &j) {
  j.print(o);
  return o;
}

class ChildJob : public Job {
public:
  Job *getParent(void) const {return parent.get();}
protected:
  ChildJob(Job *parent)
    : Job(parent->terminate_flag),parent(parent) {}
  void resetParent(void) {parent.reset();}
private:
  Job::Ptr parent; // keep parent alive
};

class MainJob : public Job {
public:
  static MainJob *create(const MandelImage &image,int size_x,int size_y) {
    return new MainJob(image,size_x,size_y);
  }
  void *operator new(size_t size) {
    if (size != sizeof(MainJob)) abort();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
private:
  MainJob(const MandelImage &image,int size_x,int size_y);
  ~MainJob(void);
  int getDistance(const int xy[2]) const {return 0;}
  void print(std::ostream &o) const {o << "MainJob";}
  bool execute(void);
  friend class MainFirstStageJob;
  void firstStageFinished(void);
private:
  const MandelImage &image;
  const int size_x;
  const int size_y;
  static FreeList free_list;
};



#endif
