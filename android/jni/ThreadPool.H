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

#ifndef THREAD_POOL_H_
#define THREAD_POOL_H_

#include "Job.H"
#include "Semaphore.H"
#include "IntrusivePtr.H"
#include "Thread.H"

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


class Job;
class MainJob;
class MandelImage;

class JobQueueBase {
protected:
  static FreeList free_list;
};



class JobQueue : public JobQueueBase {
public:
  JobQueue(void) : nr_of_waiting_threads(0),
                   pause_flag(false) {}
  void startPause(int nr_of_threads) {
    if (pause_flag) abort();
    pause_flag = true;
    for (int i=0;i<nr_of_threads;i++) {
      if (nr_of_waiting_threads) semaphore.post();
      pause_ack_sem.wait();
    }
  }
  void finishPause(int nr_of_threads) {
    if (!pause_flag) abort();
    pause_flag = false;
    for (int i=0;i<nr_of_threads;i++) pause_finish_sem.post();
  }
  void sort(const int priority_xy[2]) {
    stack.sortNonThreadSafe(NodeIsLess,priority_xy);
  }
  void queue(const Job::Ptr &j) {
    Node *n = new Node(j);
    stack.push(n);
//      Was passiert, wenn die Worker-Threads langsam abholen?
//        Denn wird semaphore viel zu oft gepostet,
//        und nachher beim dequeue() tun sie idle loopen,
//        bis der Wert von semaphore wieder 0 ist. Auch ok.
//      Was passiert aber, wenn nr_of_waiting_threads zu frueh gelesen wird?
//        Dann wird nicht gepostet, obwohl ein thread wartet.
//        Beim naechsten Job wird dann gepostet. Auch ok.
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
      if (pause_flag) {
        pause_ack_sem.post();
        pause_finish_sem.wait();
      } else {
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
  static bool NodeIsLess(const Node &a,const Node &b,const void *user_data);
  LockfreeStack<Node> stack;
public:
  volatile _Atomic_word nr_of_waiting_threads;
private:
  volatile bool pause_flag;
  Semaphore semaphore;
  Semaphore pause_ack_sem;
  Semaphore pause_finish_sem;
};


class ThreadPool {
public:
  ThreadPool(int nr_of_threads);
  ~ThreadPool(void);
  void startPause(void) {jobs_not_yet_executed.startPause(nr_of_threads);}
  void finishPause(void) {jobs_not_yet_executed.finishPause(nr_of_threads);}
  void sortJobqueue(const int priority_xy[2])
    {jobs_not_yet_executed.sort(priority_xy);}
  void startExecution(MainJob *j);
  void cancelExecution(void);
  bool workIsFinished(void) const {
    return (!main_job_is_running);
  }
  void queueJob(Job *j) {
    jobs_not_yet_executed.queue(j);
  }
  Job::Ptr dequeueDrawJob(void) { // called from only one thread
    Job::Ptr rval(jobs_not_yet_drawn.dequeueWithoutWaiting());
    if (rval) __atomic_add(&nr_of_queued_draw_jobs,-1);
    return rval;
  }
  void draw(MandelImage *image);
private:
  friend class MainJob;
  void mainJobHasTerminated(void);
private:
  static void *ThreadFunc(void *context);
public:
  volatile _Atomic_word terminate_flag;
  volatile _Atomic_word nr_of_queued_draw_jobs;
  volatile _Atomic_word &getNrOfWaitingThreads(void) {
    return jobs_not_yet_executed.nr_of_waiting_threads;
  }
private:
  const int nr_of_threads;
  class MyThread : public Thread {
  public:
    MyThread(void) : pool(0) {}
    void start(ThreadPool *pool) {
      MyThread::pool = pool;
      Thread::start(ThreadFunc,this);
    }
    static void *ThreadFunc(void *context) {
      return reinterpret_cast<MyThread*>(context)->threadFunc();
    }
    void *threadFunc(void);
    ThreadPool *pool;
    Job::Ptr current_job;
  };
  MyThread *const threads;
  JobQueue jobs_not_yet_executed;
  JobQueue jobs_not_yet_drawn;
  Semaphore sem_start;
  Semaphore sem_finished;
  bool expecting_sem_finished;
  bool main_job_is_running;
};


#endif
