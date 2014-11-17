/*
 * Author and Copyright
 * Johannes Gajdosik, 2008
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * Commercial programmers BE WARNED: when you incorporate this software
 * wholy or partly into your program, your program becomes a work derived
 * from this software. As a consequence, whenever you distribute your
 * program, you must distribute it under the terms of the GPL, too.
 * See the full text of the GPL for details.
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

#ifndef _INTRUSIVE_PTR_CAPABLE_H_
#define _INTRUSIVE_PTR_CAPABLE_H_

#include <boost/atomic.hpp>

class ReferenceCount {
public:
  ReferenceCount(void) : reference_count(0) {}
//  int getValue(void) const {
//    return reference_count.fetch();
//  }
  void retain(void) const {
    reference_count.fetch_add(1,boost::memory_order_relaxed);
  }
  bool release(void) const {
    if (reference_count.fetch_sub(1, boost::memory_order_release) == 1) {
      boost::atomic_thread_fence(boost::memory_order_acquire);
      return true;
    }
    return false;
  }
private:
  mutable boost::atomic<int> reference_count;
  ReferenceCount(const ReferenceCount&);
  const ReferenceCount &operator=(const ReferenceCount&);
};

class IntrusivePtrCapable : private ReferenceCount {
public:
  void retain(void) const {ReferenceCount::retain();}
  void release(void) const {if (ReferenceCount::release()) delete this;}
protected:
  IntrusivePtrCapable(void) {}
  virtual ~IntrusivePtrCapable(void) {}
};

#endif
