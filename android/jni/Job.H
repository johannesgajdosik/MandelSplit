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

class ThreadPool;
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

class Job {
private:
  ReferenceCount rc1,rc2;
  virtual void lastChildHasReleased(void) = 0;
protected:
  Job(volatile _Atomic_word &terminate_flag) : terminate_flag(terminate_flag) {}
public:
  struct RefCountHandler1 {
    static void retain(Job *j) {j->rc1.retain();j->rc2.retain();}
    static void release(Job *j) {
      if (j->rc1.release()) j->lastChildHasReleased();
      if (j->rc2.release()) delete j;
    }
  };
  struct RefCountHandler2 {
    static void retain(Job *j) {j->rc2.retain();}
    static void release(Job *j) {if (j->rc2.release()) delete j;}
  };
  typedef IntrusivePtr<Job,RefCountHandler1> ChildPtr;
  typedef IntrusivePtr<Job,RefCountHandler2> Ptr;
  virtual ~Job(void) {}
  virtual void print(std::ostream &o) const = 0;
  virtual bool execute(void) = 0;
  virtual bool needsDrawing(int &x,int &y,int &w,int &h) const {return false;}
  volatile _Atomic_word &terminate_flag;
};

static inline
std::ostream &operator<<(std::ostream &o,const Job &j) {
  j.print(o);
  return o;
}

class JobQueueBase {
protected:
  static FreeList free_list;
};

template<class Ptr>
class JobQueue : public JobQueueBase {
public:
  JobQueue(void) : nr_of_waiting_threads(0) {}
  void queue(Job *j) {
    Node *n = new Node(j);
    stack.push(n);
    if (nr_of_waiting_threads) {
      semaphore.post();
    }
  }
  void clear(void) {
    Node *node;
    while ((node = stack.pop())) {
      delete node;
    }
  }
  Ptr dequeueWithoutWaiting(void) {
    for (;;) {
      Node *node = stack.pop();
      if (node) {
        Ptr rval(node->job);
        delete node;
        return rval;
      }
      if (stack.empty()) {
        return Ptr();
      }
    }
  }
  Ptr dequeue(void) {
    for (;;) {
      Node *node = stack.pop();
      if (node) {
        Ptr rval(node->job);
        delete node;
        return rval;
      }
      __atomic_add(&nr_of_waiting_threads,1);
      semaphore.wait();
      __atomic_add(&nr_of_waiting_threads,-1);
    }
  }
private:
  struct Node {
    void *operator new(size_t size) {
      if (size != sizeof(Node)) abort();
      void *const rval = free_list.pop();
      if (rval) return rval;
      return free_list.malloc(size);
    }
    void operator delete(void *p) {free_list.push(p);}
    Node(Job *job) : job(job) {}
    Node *next;
    Ptr job;
  };
  LockfreeStack<Node> stack;
public:
  volatile _Atomic_word nr_of_waiting_threads;
private:
  Semaphore semaphore;
};


class ChildJob : public Job {
protected:
  ChildJob(Job *parent)
    : Job(parent->terminate_flag),parent(parent) {}
  Job *getParent(void) const {return parent.get();}
//  void resetParent(void) {parent.reset();}
  void lastChildHasReleased(void) {parent.reset();}
private:
  Job::ChildPtr parent; // keep parent alive
};

class MainJob : public Job {
public:
  MainJob(ThreadPool &pool,
          const MandelImage &image,
          int size_x,int size_y);
  void *operator new(size_t size) {
    if (size != sizeof(MainJob)) abort();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
private:
  void print(std::ostream &o) const {o << "MainJob" << endl;}
  bool execute(void);
  friend class MainFirstStageJob;
  void firstStageFinished(void);
  void lastChildHasReleased(void);
private:
  ThreadPool &pool;
  const MandelImage &image;
  const int size_x;
  const int size_y;
  static FreeList free_list;
};


#endif
