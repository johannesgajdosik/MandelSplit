/*
 * Author and Copyright
 * Johannes Gajdosik, 2008
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

#include <semaphore.h>

#include <errno.h>
#include <stdlib.h> // abort

#ifdef __ANDROID__
#include <linux/time.h> // timespec
#endif

class Semaphore {
public:
  Semaphore(unsigned int value = 0) {
      // behaves as if post() had been called already <value> times
    if (0 < sem_init(&sem,0,value)) {
      abort();
    }
  }
  ~Semaphore(void) {
    if (0 < sem_destroy(&sem)) {
      abort();
    }
  }
  int getValue(void) const {
    int value;
    if (0 < sem_getvalue(const_cast<sem_t*>(&sem),&value)) {
      abort();
    }
    return value;
  }
  void post(void) {
    if (0 < sem_post(&sem)) {
      abort();
    }
  }
  void multiPost(unsigned int count) {
    while (count > 0) {
      post();
      --count;
    }
  }
  void wait(void) {
    while (0 > sem_wait(&sem)) {
      if (errno != EINTR) {
        abort();
      }
    }
  }
  void multiWait(unsigned int count) {
    while (count > 0) {
      wait();
      --count;
    }
  }
  bool wait(long long int end_time) {
    struct timespec abs_timeout = {
      tv_sec: end_time/1000000LL,
      tv_nsec: 1000*((long)(end_time%1000000LL))
    };
//      cannot happen because end_time > 0=1970.01.01
//    if (abs_timeout.tv_nsec < 0) {
//      abs_timeout.tv_nsec += 1000000000;
//      abs_timeout.tv_sec--;
//    }
    while (0 > sem_timedwait(&sem,&abs_timeout)) {
      switch (errno) {
        case ETIMEDOUT: 
          return false;
        case EINTR:
        case EAGAIN: break;
        default:
          abort();
      }
    }
    return true;
  }
private:
  sem_t sem;
private:
    // no copies:
  Semaphore(const Semaphore&);
  const Semaphore &operator=(const Semaphore&);
};

#endif
