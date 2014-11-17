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

#ifndef _THREAD_H_
#define _THREAD_H_

#include <pthread.h>

#include <string.h> // strerror,memset
#include <stdlib.h> // abort

class Thread {
public:
  Thread(void) {
    memset(&thread,0,sizeof(pthread_t));
  }
  virtual ~Thread(void) {
    if (!isJoined()) {
      abort();
    }
  }
  bool isJoined(void) const {
    for (int i=sizeof(pthread_t)-1;i>=0;--i) {
      if (((char*)&thread)[i]) return false;
    }
    return true;
  }
  void start(void *(func)(void*),void *context) {
    const int rc = pthread_create(&thread,0,func,context);
    if (rc) {
      abort();
    }
  }
  void *join(void) {
    if (isJoined()) {
      abort();
    }
    void *rval = 0;
    const int rc = pthread_join(thread,&rval);
    if (rc) {
      abort();
    }
    memset(&thread,0,sizeof(pthread_t));
    return rval;
  }
private:
  pthread_t thread;
    // no copies:
  Thread(const Thread&);
  const Thread &operator=(const Thread&);
};

#endif
