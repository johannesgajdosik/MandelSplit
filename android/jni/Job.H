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
  Job(volatile _Atomic_word &terminate_flag) : terminate_flag(terminate_flag) {}
public:
  typedef IntrusivePtr<Job> Ptr;
  virtual ~Job(void) {}
  virtual void print(std::ostream &o) const = 0;
  virtual bool execute(void) = 0;
  virtual void drawToTexture(void) const {}
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

class JobQueue : public JobQueueBase {
public:
  JobQueue(void) : nr_of_waiting_threads(0) {}
  void queue(const Job::Ptr &j) {
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
  Job::Ptr dequeueWithoutWaiting(void) {
    for (;;) {
      Node *node = stack.pop();
      if (node) {
        Job::Ptr rval(node->job);
        delete node;
        return rval;
      }
      if (stack.empty()) {
        return Job::Ptr();
      }
    }
  }
  Job::Ptr dequeue(void) {
    for (;;) {
      Node *node = stack.pop();
      if (node) {
        Job::Ptr rval(node->job);
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
    Node(const Job::Ptr &job) : job(job) {}
    Node *next;
    Job::Ptr job;
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
  void resetParent(void) {parent.reset();}
private:
  Job::Ptr parent; // keep parent alive
};

class MainJob : public Job {
public:
  MainJob(const MandelImage &image,
          int size_x,int size_y);
  void *operator new(size_t size) {
    if (size != sizeof(MainJob)) abort();
    void *const rval = free_list.pop();
    if (rval) return rval;
    return free_list.malloc(size);
  }
  void operator delete(void *p) {free_list.push(p);}
private:
  ~MainJob(void);
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
