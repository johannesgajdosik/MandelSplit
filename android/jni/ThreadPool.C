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

#include "ThreadPool.H"
#include "Job.H"
#include "Logger.H"
#include "MandelImage.H"

#include "GLee.h"

#include <sstream>

extern void CheckGlError(const char *text);

ThreadPool::ThreadPool(int nr_of_threads)
           :terminate_flag(0),
            nr_of_queued_draw_jobs(0),
            nr_of_threads(nr_of_threads),
            threads(new MyThread[nr_of_threads]),
            expecting_sem_finished(false),
            main_job_is_running(false) {
  for (int i=0;i<nr_of_threads;i++) threads[i].start(this);
}

ThreadPool::~ThreadPool(void) {
  cancelExecution();
  for (int i=0;i<nr_of_threads;i++) queueJob(0);
  for (int i=0;i<nr_of_threads;i++) threads[i].join();
  delete[] threads;
}

void ThreadPool::startExecution(MainJob *j) {
  if (expecting_sem_finished) {
    cout << "ThreadPool::startExecution: you must call cancelExecution first"
         << endl;
    ABORT();
  }
  main_job_is_running = true;
    // initial thread, is supposed to queue may sub-threads
  jobs_not_yet_executed.queue(j);
  expecting_sem_finished = true;
}

void ThreadPool::cancelExecution(void) {
  if (expecting_sem_finished) {
//cout << "ThreadPool::cancelExecution: start waiting" << endl;
    __atomic_add(&terminate_flag,1);
    sem_finished.wait();
//    jobs_not_yet_drawn.clear();
    __atomic_add(&terminate_flag,-1);
    expecting_sem_finished = false;
//cout << "ThreadPool::cancelExecution: waiting finished" << endl;
  } else {
//cout << "ThreadPool::cancelExecution: first time or strange" << endl;
  }
}

void *ThreadPool::MyThread::threadFunc(void) {
  for (;;) {
//    cout << "ThreadPool::MyThread::threadFunc: dequeuing" << endl;
//    Job::ChildPtr
     current_job = pool->jobs_not_yet_executed.dequeue();
    if (current_job) {
        // TODO: draw also when not executed completely
//      cout << "ThreadPool::MyThread::threadFunc 100: " << (*current_job) << endl;
      const bool rc = current_job->execute();
//      cout << "ThreadPool::MyThread::threadFunc 101" << endl;
      if (rc) {
        pool->jobs_not_yet_drawn.queue(current_job.get());
        __atomic_add(&pool->nr_of_queued_draw_jobs,1);
      }
//      cout << "ThreadPool::MyThread::threadFunc 102" << endl;
      current_job.reset();
    } else {
      break;
    }
  }
  return 0;
}

void ThreadPool::mainJobHasTerminated(void) {
  main_job_is_running = false;
  sem_finished.post();
//cout << "ThreadPool::mainJobHasTerminated: sem posted" << endl;
}


static void DrawJob(Job *j,MandelImage *image) {
  int x,y,w,h;
  if (j->needsDrawing(x,y,w,h)) {
#ifdef __ANDROID__
    do {
      glTexSubImage2D(GL_TEXTURE_2D,0,
                      x,y,w,1,
                      GL_RGBA,GL_UNSIGNED_BYTE,
                      image->data+x+y*(image->screen_width));
      h--;
      y++;
    } while (h > 0);
#else
    glTexSubImage2D(GL_TEXTURE_2D,0,
                    x,y,w,h,
                    GL_RGBA,GL_UNSIGNED_BYTE,
                    image->data+x+y*(image->screen_width));
#endif
  }
}

void ThreadPool::draw(MandelImage *image) {
  const int nr_of_jobs = nr_of_queued_draw_jobs;
  if (nr_of_jobs > 300) {
//cout << "ThreadPool::draw: drawing entire image because too many jobs: "
//     << nr_of_jobs << endl;
    while (dequeueDrawJob()) {}
    glTexSubImage2D(GL_TEXTURE_2D,0,
                    0,0,image->screen_width,image->screen_height,
                    GL_RGBA,GL_UNSIGNED_BYTE,
                    image->data);
  } else {
    for (int i=0;i<nr_of_threads;i++) {
      Job::ChildPtr job(threads[i].current_job);
      if (job) {
//        std::ostringstream o;
//        o << *job;
        DrawJob(job.get(),image);
//        CheckGlError(o.str().c_str());
      }
    }
    Job::Ptr job;
    while ((job = dequeueDrawJob())) {
//      std::ostringstream o;
//      o << *job;
      DrawJob(job.get(),image);
//      CheckGlError(o.str().c_str());
    }
  }
}
