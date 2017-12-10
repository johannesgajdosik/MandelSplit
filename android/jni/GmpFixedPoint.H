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

#ifndef GMP_FIXED_POINT_H_
#define GMP_FIXED_POINT_H_

#ifndef __ANDROID__
#include "DummyData.h"
#endif
#include "Logger.H"
#include "MpfClass.H"
#include "LockfreeStack.H"
#include "Mutex.H"

#include <gmp.h>
//#include <gmpxx.h>
#include <alloca.h>

#include <iostream>
#include <stack>

class GmpFixedPoint {
public:
  static mp_size_t n;
  mp_limb_t *p; // size must always be passed from outside
  bool sign; // true..minus
private:
  GmpFixedPoint(const GmpFixedPoint&);
  const GmpFixedPoint &operator=(const GmpFixedPoint&);
protected:
  explicit GmpFixedPoint(mp_limb_t *mem) : p(mem) {}
  GmpFixedPoint(mp_limb_t *mem,bool sign) : p(mem),sign(sign) {}
public:
  enum {bits_per_limb = 8*sizeof(mp_limb_t)};

    // 1<<(n*bits_per_limb-1) means +1
    // 1<<(n*bits_per_limb) means +2, cannot be represented.
    // rounding is done away from zero (0.4=>0, 0.5=>1, -0.5=>-1)

      // tmp must contain at least sizeof(mp_limb_t)*n2+2 bytes
  void print(mp_size_t n,char *tmp) const;

    // returns true on overflow
  static mp_limb_t AssignFromDouble(mp_size_t n,mp_limb_t *p,bool &sign,
                                    double x);
  bool assignFromDouble(mp_size_t n,double x) {
    return AssignFromDouble(n,p,sign,x);
  }
  void assign2FromDouble(const mp_size_t n,double x) {
    p[n+1] = AssignFromDouble(n+1,p,sign,x);
  }
  static double ConvertToDouble(const mp_size_t n,const mp_limb_t *p,bool sign);
  static double ConvertToDoubleNew(const mp_size_t n,const mp_limb_t *p,bool sign);
  double convertToDouble(mp_size_t n) const {
    return ConvertToDouble(n,p,sign);
  }
  double convertToDoubleNew(mp_size_t n) const {
    return ConvertToDoubleNew(n,p,sign);
  }
//  void assignFromInt(mp_size_t n,long int x);
  
  void assign2(const GmpFixedPoint &a) {
    sign = a.sign;
    mpn_copyi(p,a.p,n+2);
  }
//  void assign(mp_size_t n,const void *a,bool sign) {
//    GmpFixedPoint::sign = sign;
//    mpn_copyi(p,(const mp_limb_t*)a,n);
//  }

  FLOAT_TYPE convert2ToMpf(const mp_size_t n) const {
    __mpf_struct h;
    h._mp_size = n+2;
    h._mp_prec = n+2;
    h._mp_exp = 1;
    h._mp_d = p;
    while (0 == h._mp_d[0]) {
      h._mp_size--;
      if (0 == h._mp_size) {
//TODO:WHATS THIS?        h._mp_exp == 0;
        h._mp_exp = 0;
        return FLOAT_TYPE(&h);
      }
      h._mp_d++;
    }

      // h._mp_size > 0, h._mp_d[0] != 0

    while (0 == h._mp_d[h._mp_size-1]) {
      h._mp_size--;
      h._mp_exp--;
    }

    if (sign) h._mp_size = -h._mp_size;
    return FLOAT_TYPE(&h);
  }
  
  bool assign2FromMpf(const mp_size_t n,const mpf_t x);
//  bool assignFromMpf(const mp_size_t n,const mpf_t x);

//  int dump(mp_size_t n,void *x) const {
//    mpn_copyi((mp_limb_t*)x,p,n);
//    return n*sizeof(mp_limb_t);
//  }

/*
  int compare(mp_size_t n,const GmpFixedPoint &a) const {
    if (sign) {
      if (a.sign) {
        return -mpn_cmp(p,a.p,n);
      } else {
        return -1;
      }
    } else {
      if (a.sign) {
        return 1;
      } else {
        return mpn_cmp(p,a.p,n);
      }
    }
  }
*/
    // calculate a+b, returns true on overflow
  mp_limb_t add2(const GmpFixedPoint &b) {return add2(*this,b);}
  mp_limb_t add2(const GmpFixedPoint &a,const GmpFixedPoint &b) {
    if (a.sign) {
      if (b.sign) {
        sign = true;
        return mpn_add_n(p,a.p,b.p,n+2);
      } else { // b >= 0
        if (mpn_sub_n(p,a.p,b.p,n+2)) { // (-a)-(b) = -(a+b) < 0, a+b > 0
          mpn_neg(p,p,n+2);
          sign = false;
        } else {
          sign = true;
        }
        return 0;
      }
    } else {
      if (b.sign) {
        if (mpn_sub_n(p,a.p,b.p,n+2)) { // (a)-(-b) = a+b < 0
          mpn_neg(p,p,n+2);
          sign = true;
        } else {
          sign = false;
        }
        return 0;
      } else {
        sign = false;
        return mpn_add_n(p,a.p,b.p,n+2);
      }
    }
  }
  mp_limb_t sub2(const GmpFixedPoint &b) {return sub2(*this,b);}
  mp_limb_t sub2(const GmpFixedPoint &a,const GmpFixedPoint &b) {
    if (a.sign) {
      if (b.sign) {
        if (mpn_sub_n(p,a.p,b.p,n+2)) { // (-a)-(b) = -(a+b) < 0, a+b > 0
          mpn_neg(p,p,n+2);
          sign = false;
        } else {
          sign = true;
        }
        return 0;
      } else { // b >= 0
        sign = true;
        return mpn_add_n(p,a.p,b.p,n+2);
      }
    } else {
      if (b.sign) {
        sign = false;
        return mpn_add_n(p,a.p,b.p,n+2);
      } else {
        if (mpn_sub_n(p,a.p,b.p,n+2)) { // (a)-(-b) = a+b < 0
          mpn_neg(p,p,n+2);
          sign = true;
        } else {
          sign = false;
        }
        return 0;
      }
    }
  }
/*
    // calculate (*this)+b*f, returns true on overflow
  mp_limb_t addMulS(mp_size_t n,const GmpFixedPoint &b,const long int f) {
    if (sign) { // a < 0
      if (b.sign) { // b < 0
        if (f < 0) {
          mp_limb_t rc = mpn_submul_1(p,b.p,n,-f);
          if (rc) { // (-a)-(-b)*(-f) = -a-b*f = -(a+b*f) < 0, a+b*f>0
            rc -= mpn_neg(p,p,n);
            sign = false;
          } else {
            sign = true;
          }
          return rc;
        } else { // f >= 0
          sign = true;
          return mpn_addmul_1(p,b.p,n,f);
        }
      } else { // b >= 0
        if (f < 0) {
          sign = true;
          return mpn_addmul_1(p,b.p,n,-f);
        } else { // f >= 0
          mp_limb_t rc = mpn_submul_1(p,b.p,n,f);
          if (rc) { // (-a)-(b)*(f) = -a-b*f = -(a+b*f) < 0, a+b*f>0
            rc -= mpn_neg(p,p,n);
            sign = false;
          } else {
            sign = true;
          }
          return rc;
        }
      }
    } else { // a >= 0
      if (b.sign) {
        if (f < 0) {
          sign = false;
          return mpn_addmul_1(p,b.p,n,-f);
        } else { // f >= 0
          mp_limb_t rc = mpn_submul_1(p,b.p,n,f);
          if (rc) { // (a)-(-b)*(f) = a+b*f < 0
            rc -= mpn_neg(p,p,n);
            sign = true;
          } else {
            sign = false;
          }
          return rc;
        }
      } else { // b >= 0
        if (f < 0) {
          mp_limb_t rc = mpn_submul_1(p,b.p,n,-f);
          if (rc) { // (a)-(b)*(-f) = a+b*f < 0
            rc -= mpn_neg(p,p,n);
            sign = true;
          } else {
            sign = false;
          }
          return rc;
        } else { // f >= 0
          sign = false;
          return mpn_addmul_1(p,b.p,n,f);
        }
      }
    }
  }
*/
    // calculate (*this)+b*f, returns true on overflow
  mp_limb_t addMulU2(const GmpFixedPoint &b,const unsigned long int f) {
    if (sign) { // a < 0
      if (b.sign) { // b < 0
        sign = true;
        return mpn_addmul_1(p,b.p,n+2,f);
      } else { // b >= 0
        mp_limb_t rc = mpn_submul_1(p,b.p,n+2,f);
        if (rc) { // (-a)-(b)*(f) = -a-b*f = -(a+b*f) < 0, a+b*f>0
          rc -= mpn_neg(p,p,n+2);
          sign = false;
        } else {
          sign = true;
        }
        return rc;
      }
    } else { // a >= 0
      if (b.sign) {
        mp_limb_t rc = mpn_submul_1(p,b.p,n+2,f);
        if (rc) { // (a)-(-b)*(f) = a+b*f < 0
          rc -= mpn_neg(p,p,n+2);
          sign = true;
        } else {
          sign = false;
        }
        return rc;
      } else { // b >= 0
        sign = false;
        return mpn_addmul_1(p,b.p,n+2,f);
      }
    }
  }

    // calculate (*this)-b*f, returns true on overflow
  mp_limb_t subMulU2(const GmpFixedPoint &b,const unsigned long int f) {
    if (sign) { // a < 0
      if (b.sign) { // b < 0
        mp_limb_t rc = mpn_submul_1(p,b.p,n+2,f);
        if (rc) {
          rc -= mpn_neg(p,p,n+2);
          sign = false;
        } else {
          sign = true;
        }
        return rc;
      } else { // b >= 0
        sign = true;
        return mpn_addmul_1(p,b.p,n+2,f);
      }
    } else { // a >= 0
      if (b.sign) {
        sign = false;
        return mpn_addmul_1(p,b.p,n+2,f);
      } else { // b >= 0
        mp_limb_t rc = mpn_submul_1(p,b.p,n+2,f);
        if (rc) {
          rc -= mpn_neg(p,p,n+2);
          sign = true;
        } else {
          sign = false;
        }
        return rc;
      }
    }
  }

    // this will not correctly show overflow when fa,fb are really big
    // assign a*f+b*fb
  long int linCombPlusU1(const GmpFixedPoint &a,const unsigned long int fa,
                         const GmpFixedPoint &b,const unsigned long int fb) {
    long int rc = mpn_mul_1(p,a.p,n+1,fa);
    if (a.sign) { // a < 0
      if (b.sign) { // b < 0
        sign = true;
        rc += mpn_addmul_1(p,b.p,n+1,fb); // hopefully this gives no overflow
      } else { // b >= 0
        rc -= mpn_submul_1(p,b.p,n+1,fb);
        if (rc < 0) {
          rc += mpn_neg(p,p,n+1);
          rc = -rc;
          sign = false;
        } else {
          sign = true;
        }
      }
    } else { // a >= 0
      if (b.sign) {
        rc -= mpn_submul_1(p,b.p,n+1,fb);
        if (rc < 0) {
          rc += mpn_neg(p,p,n+1);
          rc = -rc;
          sign = true;
        } else {
          sign = false;
        }
      } else { // b >= 0
        sign = false;
        rc += mpn_addmul_1(p,b.p,n+1,fb); // hopefully this gives no overflow
      }
    }
    return rc;
  }

    // assign a*fa-b*fb
  long int linCombMinusU1(const GmpFixedPoint &a,const unsigned long int fa,
                          const GmpFixedPoint &b,const unsigned long int fb) {
    long int rc = mpn_mul_1(p,a.p,n+1,fa);
    if (a.sign) { // a < 0
      if (b.sign) { // b < 0
        rc -= mpn_submul_1(p,b.p,n+1,fb);
        if (rc < 0) {
          rc += mpn_neg(p,p,n+1);
          rc = -rc;
          sign = false;
        } else {
          sign = true;
        }
      } else { // b >= 0
        sign = true;
        rc += mpn_addmul_1(p,b.p,n+1,fb); // hopefully this gives no overflow
      }
    } else { // a >= 0
      if (b.sign) {
        sign = false;
        rc += mpn_addmul_1(p,b.p,n+1,fb); // hopefully this gives no overflow
      } else { // b >= 0
        rc -= mpn_submul_1(p,b.p,n+1,fb);
        if (rc < 0) {
          rc += mpn_neg(p,p,n+1);
          rc = -rc;
          sign = true;
        } else {
          sign = false;
        }
      }
    }
    return rc;
  }
/*
  long int linCombPlusS1(const GmpFixedPoint &a,const long int fa,
                         const GmpFixedPoint &b,const long int fb) {
    if (fa >= 0) {
      if (fb >= 0) {
        return linCombPlusU1(a,fa,b,fb);
      } else {
        return linCombMinusU1(a,fa,b,-fb);
      }
    } else {
      long int rc;
      if (fb >= 0) {
        rc = linCombMinusU1(b,fb,a,-fa);
      } else {
        rc = linCombPlusU1(a,-fa,b,-fb);
      }
      sign = !sign;
      return -rc;
    }
  }
  long int linCombMinusS1(const GmpFixedPoint &a,const long int fa,
                          const GmpFixedPoint &b,const long int fb) {
    if (fa >= 0) {
      if (fb >= 0) {
        return linCombMinusU1(a,fa,b,fb);
      } else {
        return linCombPlusU1(a,fa,b,-fb);
      }
    } else {
      long int rc;
      if (fb >= 0) {
        rc = linCombPlusU1(a,-fa,b,fb);
      } else {
        rc = linCombMinusU1(a,-fa,b,-fb);
      }
      sign = !sign;
      return -rc;
    }
  }

    // calculate b*f, returns overflow
  mp_limb_t mulU(mp_size_t n,const GmpFixedPoint &b,const unsigned long int f) {
    sign = b.sign;
    return mpn_mul_1(p,b.p,n,f);
  }
  void sqr(mp_size_t n,const GmpFixedPoint &x) {
      // p needs 2*n limbs
    mpn_sqr(p,x.p,n);
  }
*/  
  static unsigned int GmpMandel2(const GmpFixedPoint &cr,
                                 const GmpFixedPoint &ci,
                                 const unsigned int max_iter);
};

class GmpFixedPointExternalMem : public GmpFixedPoint {
public:
  GmpFixedPointExternalMem(mp_limb_t *p) : GmpFixedPoint(p) {}
};


#ifndef __ANDROID__
static int dummy_size = 100;

static inline mp_size_t GetDummySize(mp_size_t n) {
  return sizeof(uintptr_t)*dummy_size
       + sizeof(mp_size_t)
       + sizeof(mp_limb_t)*n
       + sizeof(uintptr_t)*dummy_size;
}

static mp_limb_t *InitDummy(void *const data,const mp_size_t n) {
  if (data == 0) abort();
  if (n <= 0) abort();
  char *p = (char*)data;
  if (p == 0) {
//      cout << "malloc failed" << endl;
    abort();
  }
  InitDummyData((uintptr_t*)p,dummy_size);
  p += sizeof(uintptr_t)*dummy_size;
  *((mp_size_t*)p) = n;
  p += sizeof(mp_size_t);
  mp_limb_t *rval = (mp_limb_t*)p;
  p += sizeof(mp_limb_t)*n;
  InitDummyData((uintptr_t*)p,dummy_size);
  return rval;
}

static void *CheckDummy(mp_limb_t *const d) {
  char *p = (char*)d;
  p -= sizeof(mp_size_t);
  const mp_size_t n = *((mp_size_t*)p);
  p -= sizeof(uintptr_t)*dummy_size;
  CheckDummyData((uintptr_t*)p,dummy_size);
  CheckDummyData((uintptr_t*)(d+n),dummy_size);
  return p;
}
#else
static inline mp_size_t GetDummySize(mp_size_t n) {
  return sizeof(mp_limb_t)*n;
}

static mp_limb_t *InitDummy(void *const data,const mp_size_t n) {
  return (mp_limb_t*)data;
}

static void *CheckDummy(mp_limb_t *const d) {
  return d;
}
#endif

class GmpFixedPointHeap : public GmpFixedPoint {
  GmpFixedPointHeap(const GmpFixedPointHeap &a);
  const GmpFixedPointHeap &operator=(const GmpFixedPointHeap &a);
public:
  static mp_limb_t *AllocLimbs(mp_size_t n) {
    if (n <= 0) abort();
    void *p = malloc(GetDummySize(n));
    return InitDummy(p,n);
  }
  static void FreeLimbs(mp_limb_t *const d) {
    if (d) {
      void *p = CheckDummy(d);
      free(p);
    }
  }
//  GmpFixedPointHeap(const GmpFixedPointHeap &a)
//    : GmpFixedPoint(a.p) {sign = a.sign;} // take ownership
//  const GmpFixedPointHeap &operator=(const GmpFixedPointHeap &a)
//    {p = a.p;sign = a.sign;} // take ownership
  void takeOwnership(GmpFixedPoint &a) {p = a.p;sign = a.sign;a.p = 0;}
  void assign2(const GmpFixedPoint &a) {
    if (p == 0) p = AllocLimbs(n+2);
    GmpFixedPoint::assign2(a);
  }
  GmpFixedPointHeap(void) : GmpFixedPoint((mp_limb_t*)0) {}
  GmpFixedPointHeap(mp_size_t n) : GmpFixedPoint((n>0) ? AllocLimbs(n) : 0) {}
  ~GmpFixedPointHeap(void) {
//    cout << "~GmpFixedPointHeap start" << endl;
    FreeLimbs(p);
//    cout << "~GmpFixedPointHeap end" << endl;
  }
  void check(void) const {
    if (p) CheckDummy(p);
  }
    // returns true on overflow
  bool changePrecision(mp_size_t cur_size,mp_size_t new_size) {
    if (cur_size != new_size) {
      mp_limb_t *new_p = 0;
      if (new_size > 0) {
        new_p = AllocLimbs(new_size);
        if (cur_size < new_size) { // increase precision
          mpn_zero(new_p,new_size-cur_size);
          mpn_copyi(new_p+new_size-cur_size,p,cur_size);
        } else { // decrease precision
//            // rounding
//          if (((((mp_limb_t)1)<<(bits_per_limb-1)) & p[cur_size-new_size-1])) {
//            const bool rval = mpn_add_1(new_p,p+cur_size-new_size,new_size,1);
//            FreeLimbs(p);
//            p = new_p;
//            return rval;
//          } else {
            mpn_copyi(new_p,p+cur_size-new_size,new_size);
//          }
        }
      }
      if (cur_size > 0) FreeLimbs(p);
      p = new_p;
    }
    return false;
  }
};

class GmpFreeList {
private:
  struct Node {Node *next;};
  LockfreeStack<Node> stack;
public:
  static void *malloc(size_t size) {return ::operator new(size);}
  static void free(void *data) {::operator delete(data);}
  void push(void *data) {stack.push(reinterpret_cast<Node*>(data));}
  void *pop(void) {return stack.pop();}
  ~GmpFreeList(void) {void *p;while ((p=stack.popNonThreadSafe())) free(p);}
};


class GmpFixedPointLockfree : public GmpFixedPoint {
  const GmpFixedPointLockfree &operator=(const GmpFixedPointLockfree &a);
private:
  struct Node {Node *next;};
  static LockfreeStack<Node> mem_stack;
  static Mutex stack_mutex;
  static std::stack<void*> allocated_mem;
private:
  static mp_limb_t *AllocList(const int nr_of_elements) {
    mp_limb_t *rval = (mp_limb_t*)
                      malloc(nr_of_elements*(n+2)*sizeof(mp_limb_t));
    allocated_mem.push(rval);
    for (int i=1;i<nr_of_elements;i++) {
      ((Node*)(rval+(i-1)*(n+2)))->next = ((Node*)(rval+i*(n+2)));
    }
    return rval;
  }
  static mp_limb_t *AllocLimbs(void) {
    mp_limb_t *rval = (mp_limb_t*)mem_stack.pop();
    if (rval) {
//cout << "AllocLimbs1: " << rval << endl;
      return rval;
    }
    MutexLock lock(stack_mutex);
      // maybe some other thread has allocated in the meantime
    rval = (mp_limb_t*)mem_stack.pop();
    if (rval) {
//cout << "AllocLimbs2: " << rval << endl;
      return rval;
    }
      // no other thread has allocated mem
    const int elements = 1024;
    mp_limb_t *const list = AllocList(elements);
    if (elements > 1) {
      mem_stack.pushList((Node*)(list+             (n+2)),
                         (Node*)(list+(elements-1)*(n+2)));
    }
//cout << "AllocLimbs3: " << list << endl;
    return list;
  }
  static void FreeLimbs(mp_limb_t *const d) {
//cout << "FreeLimbs: " << d << endl;
    if (d) mem_stack.push((Node*)d);
  }
public:
  static void changeNrOfLimbs(mp_size_t new_n) {
    if (new_n > 0 && n != new_n) {
//cout << "changeNrOfLimbs: " << n << "->" << new_n << endl;
      while (!allocated_mem.empty()) {
        free(allocated_mem.top());
        allocated_mem.pop();
      }
      n = new_n;
      mem_stack.clearNonThreadSafe();
//      mp_limb_t *const list = AllocList(1024);
//      mem_stack.pushList((Node*)list,
//                         (Node*)(list+(1024-1)*(n+2)*sizeof(mp_limb_t)));
    }
  }
  GmpFixedPointLockfree(GmpFixedPointLockfree &a) : GmpFixedPoint(a.p,a.sign) {
    a.p = 0; // takes ownership
  }
  void takeOwnership(GmpFixedPointLockfree &a) {p=a.p;sign=a.sign;a.p=0;}
  void assign2(const GmpFixedPoint &a) {
    if (p == 0) p = AllocLimbs();
    GmpFixedPoint::assign2(a);
  }
  GmpFixedPointLockfree(void) : GmpFixedPoint(AllocLimbs()) {}
  ~GmpFixedPointLockfree(void) {
//    cout << "~GmpFixedPointLockfree start" << endl;
    FreeLimbs(p);
//    cout << "~GmpFixedPointLockfree end" << endl;
  }
};


//#define DEFINE_GmpFixedPointAlloca(name,size) \
//GmpFixedPointAllocaHelper name(alloca(GetDummySize(size)),size)
//
//class GmpFixedPointAllocaHelper : public GmpFixedPoint {
//public:
//  GmpFixedPointAllocaHelper(void *d,mp_size_t n)
//    : GmpFixedPoint(InitDummy(d,n)) {}
//  ~GmpFixedPointAllocaHelper(void) {
//    cout << "~GmpFixedPointAllocaHelper start" << endl;
//    CheckDummy(p);
//    cout << "~GmpFixedPointAllocaHelper end" << endl;
//  }
//};


unsigned int GmpMandel(mp_size_t n,
                         // size n+2. one limb for extra precision,
                         // one limb for extra range
                       const GmpFixedPoint &cr,
                       const GmpFixedPoint &ci,
                       const unsigned int max_iter);

class PrintableGmpFixedPoint {
public:
  PrintableGmpFixedPoint(mp_size_t n,const GmpFixedPoint &a) : n(n),a(a) {}
  void print(std::ostream &o) const {
    char tmp[(2*sizeof(mp_limb_t)+1)*n+1];
    a.print(n,tmp);
    o << tmp;
  }
private:
  const mp_size_t n;
  const GmpFixedPoint &a;
};

static inline
std::ostream &operator<<(std::ostream &o,const PrintableGmpFixedPoint &a) {
  a.print(o);
  return o;
}

#endif
