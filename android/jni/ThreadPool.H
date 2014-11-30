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

class ThreadPool {
public:
  ThreadPool(int nr_of_threads);
  ~ThreadPool(void);
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
