/*
    Author and Copyright: Johannes Gajdosik, 2015

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

#ifndef DUMMY_DATA_H_
#define DUMMY_DATA_H_

#include <stdlib.h> // abort

#include <stdint.h> // uintptr_t

static inline void InitDummyData(uintptr_t *dummy,int data_size) {
  uintptr_t x = (uintptr_t)dummy;
  for (int i=0;i<data_size;i++) {
    dummy[i] = x;
    x = 1078323213*x + 5621108;
  }
}



static inline void CheckDummyData(const uintptr_t *dummy,int data_size) {
  uintptr_t x = (uintptr_t)dummy;
  for (int i=0;i<data_size;i++) {
    if (dummy[i] != x) abort();
    x = 1078323213*x + 5621108;
  }
}

template<int data_size>
class DummyData {
public:
  DummyData(void) {init();}
  DummyData(const DummyData &d) {d.check();init();}
  const DummyData &operator=(const DummyData &d)
    {check();d.check();return *this;}
  ~DummyData(void) {
    check();
    for (int i=0;i<data_size;i++) {dummy[i] = 0;}
  }
  void check(void) const {CheckDummyData(dummy,data_size);}
private:
  void init(void) {InitDummyData(dummy,data_size);}
  uintptr_t dummy[data_size];
};

#endif

