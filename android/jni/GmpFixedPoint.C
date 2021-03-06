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

//#include <stdio.h>
////#include <stdarg.h>
#include "GmpFixedPoint.H"
// from gmp_impl.h
extern "C"
double __gmpn_get_d(const mp_limb_t*,mp_size_t size,mp_size_t sign,long exp);

#include <math.h>
#include <stdlib.h>

static inline char NibbleToChar(mp_limb_t x) {
  return ((x <= 9) ? '0' : ('a'-10)) + x;
}

void GmpFixedPoint::print(mp_size_t n,char *tmp) const {
    // tmp must contain at least (2*sizeof(mp_limb_t)+1)*n+1 bytes
  *tmp++ = sign ? '-' : '+';
  if (n <= 0) {
    *tmp = '\0';
    return;
  }
  tmp += (2*sizeof(mp_limb_t)+1)*n-1;
  *tmp = '\0';
  int i = 0;
  for (;;) {
    mp_limb_t x = p[i];
    for (int j=2*sizeof(mp_limb_t);j>0;j--) {
      *--tmp = NibbleToChar(x & 0x0F);
      x>>=4;
    }
    if (++i >= n) break;
    *--tmp = ' ';
  }
//    gmp_snprintf(tmp,tmp_size,"%Nx\n",p,n);
}

static inline double CalcLimbFactor(void) {
  double factor = 1.0;
  for (int i=GmpFixedPoint::bits_per_limb;i>0;i--) {factor += factor;}
  return factor;
}

static const double limb_factor = CalcLimbFactor();
static const double inv_limb_factor = 1.0/limb_factor;

mp_limb_t GmpFixedPoint::AssignFromDouble(mp_size_t n,mp_limb_t *p,bool &sign,
                                          double x) {
  if ( (sign = (x < 0.0)) ) {x = -x;}
  mp_limb_t rval = (mp_limb_t)floor(x);
  x = (x-rval)*limb_factor;
  for (int i=n-1;i>=0;i--) {
    const double h = floor(x);
    p[i] = (mp_limb_t)h;
    x = (x-h)*limb_factor;
  }
  if (x+x >= limb_factor) {
      // round up
    rval += mpn_add_1(p,p,n,1);
  }
  return rval;
}

double GmpFixedPoint::ConvertToDouble(const mp_size_t n,
                                      const mp_limb_t *const p,
                                      const bool sign) {
  double scale = inv_limb_factor;
  double rval = p[n-1];
  if (n > 1) {
    mp_size_t i = n-2;
    for (;;) {
      rval = limb_factor*rval + p[i];
      scale *= inv_limb_factor;
      if (i == 0) break;
      i--;
    }
  }
  rval *= scale;
  return (sign) ? -rval : rval;
}

double GmpFixedPoint::ConvertToDoubleNew(mp_size_t n,
                                         const mp_limb_t *const p,
                                         const bool sign) {
  int exp = 0;
  while (p[n-1] == 0) {
    n--;
    if (n == 0) return 0.0;
    exp -= (8*sizeof(mp_limb_t));
//cout << "E: " << exp << endl;
  }
#ifdef X__x86_64
  union MyIeeeDouble {
    struct {
      unsigned int manl:32;
      unsigned int manh:20;
      unsigned int exp:11;
      unsigned int sig:1;
    } s32;
    struct {
      unsigned long long int man:52;
      unsigned int exp:11;
      unsigned int sig:1;
    } s;
    double d;
  } rval;
  if (p[n-1] >= (1ULL<<53)) {
    mp_limb_t q = p[n-1]>>1;
    exp++;
//cout << "E: " << exp << ", M: " << std::hex << q << std::dec << endl;
    if (q >= (1ULL<<(52+6))) {
      q >>= 6;
      exp += 6;
//cout << "E: " << exp << ", M: " << std::hex << q << std::dec << endl;
    }
    if (q >= (1ULL<<(52+3))) {
      q >>= 3;
      exp += 3;
//cout << "E: " << exp << ", M: " << std::hex << q << std::dec << endl;
    }
    while (q >= (1ULL<<53)) {
      q >>= 1;
      exp++;
//cout << "E: " << exp << ", M: " << std::hex << q << std::dec << endl;
    }
    rval.s.man = q;
//cout << "M: " << std::hex << rval.s.man << std::dec << endl;
  } else
  if (p[n-1] < (1ULL<<52)) {
    if (n == 1) {
        // TODO: check for denormalization
      mp_limb_t q = p[n-1]<<1;
      exp--;
      if (q < (1ULL<<(53-26))) {
        q <<= 26;
        exp -= 26;
      }
        // q >= (1ULL<<(53-27)
      if (q < (1ULL<<(53-13))) {
        q <<= 13;
        exp -= 13;
      }
        // q >= (1ULL<<39)
      if (q < (1ULL<<(53-7))) {
        q <<= 7;
        exp -= 7;
      }
        // q >= (1ULL<<46)
      if (q < (1ULL<<(53-3))) {
        q <<= 3;
        exp -= 3;
      }
        // q >= (1ULL<<49)
      while (q < (1ULL<<(53-1))) {
        q <<= 1;
        exp--;
      }
      rval.s.man = q;
    } else {
      mp_limb_t q[2];
      if (p[n-1] >= (1ULL<<40)) {
          // (1ULL<<40) <= p[n-1] < (1ULL<<52)
        mpn_lshift(q,p+n-2,2,12);
        exp -= 12;
          // (1ULL<<52) <= q[1] < (1ULL<<64)
        if (q[1] >= (1ULL<<(52+6))) {
          q[1] >>= 6;
          exp += 6;
        }
        if (q[1] >= (1ULL<<(52+3))) {
          q[1] >>= 3;
          exp += 3;
        }
        while (q[1] >= (1ULL<<53)) {
          q[1] >>= 1;
          exp++;
        }
        rval.s.man = q[1];
      } else { // p[n-1] < (1ULL<<40)
        if (p[n-1] >= (1ULL<<20)) {
          if (p[n-1] >= (1ULL<<30)) {
              // (1ULL<<30) <= p[n-1] < (1ULL<<40)
            mpn_rshift(q,p+n-2,2,42);
            exp -= (64-42);
          } else {
              // (1ULL<<20) <= p[n-1] < (1ULL<<30)
            mpn_rshift(q,p+n-2,2,32);
            exp -= (64-32);
          }
        } else { // p[n-1] < (1ULL<<20)
          if (p[n-1] >= (1ULL<<10)) {
              // (1ULL<<10) <= p[n-1] < (1ULL<<20)
            mpn_rshift(q,p+n-2,2,22);
            exp -= (64-22);
              // q[1] == 0
              // (1ULL<<(64+10-22=52)) <= q[0] < (1ULL<<(64+20-22=62))
          } else {
              // (1ULL<<0) <= p[n-1] < (1ULL<<10)
            mpn_rshift(q,p+n-2,2,12);
            exp -= (64-12);
              // q[1] == 0
              // (1ULL<<52) <= q[0] < (1ULL<<62)
          }
        }
          // (1ULL<<52) <= q[0] < (1ULL<<62)
        if (q[0] >= (1ULL<<(52+5))) {
          q[0] >>= 5;
          exp += 5;
        }
        if (q[0] >= (1ULL<<(52+3))) {
          q[0] >>= 3;
          exp += 3;
        }
        while (q[0] >= (1ULL<<53)) {
          q[0] >>= 1;
          exp++;
        }
        rval.s.man = q[0];
      }
    }
  } else {
    rval.s.man = p[n-1];
  }
  rval.s.exp = exp+(1024+51);
  rval.s.sig = sign;
  return rval.d;
#else
  return __gmpn_get_d(p,n,sign,exp+64+n*(-8*sizeof(*p)));
#endif
}

//void GmpFixedPoint::assignFromInt(mp_size_t n,long int x) {
//  if (x < 0) {
//    sign = true;
//    p[0] = -x;
//  } else {
//    sign = false;
//    p[0] = x;
//  }
//  mpn_zero(p+1,n-1);
//}

bool GmpFixedPoint::assign2FromMpf(const mp_size_t n,const mpf_t x) {
/*
  int size;
  if (x->_mp_size < 0) {size = -x->_mp_size;sign = true;}
  else {size = x->_mp_size;sign = false;}
  int exp = x->_mp_exp;
  const mp_limb_t *s = x->_mp_d;
  while (0 < size && 0 == s[size-1]) {
    size--;
    exp--;
  }
  if (exp > 1) return false;
  s += size;
  mp_limb_t *d = p + n + 2;
  for (int i=exp;i<=0;i++) {
    *--d = 0;
    if (d <= p) return true;
  }
  for (;;) {
    *--d = *--s;
    if (d <= p) return true;
    if (s <= x->_mp_d) {
      for (;;) {
        *--d = 0;
        if (d <= p) return true;
      }
    }
  }
*/
//    if (n < 1) ABORT();
  if (x->_mp_exp > 1) return false;
  sign = (x->_mp_size < 0);
  int size;
  if (x->_mp_size < 0) {size = -x->_mp_size;sign = true;}
  else {size = x->_mp_size;sign = false;}
  const mp_limb_t *s = x->_mp_d+size;
  mp_limb_t *d = p + n + 2;
  for (int i=x->_mp_exp;i<=0;i++) {
    *--d = 0;
    if (d <= p) return true;
  }
  for (;;) {
    if (d <= p) return true;
    if (s <= x->_mp_d) {
      for (;;) {
        *--d = 0;
        if (d <= p) return true;
      }
    }
    *--d = *--s;
  }
}

//bool GmpFixedPoint::assignFromMpf(const mp_size_t n,const mpf_t x) {
////    if (n < 1) ABORT();
//  if (x->_mp_exp > 0) return false;
//  sign = (x->_mp_size < 0);
//  int size;
//  if (x->_mp_size < 0) {size = -x->_mp_size;sign = true;}
//  else {size = x->_mp_size;sign = false;}
//  const mp_limb_t *s = x->_mp_d+size;
//  mp_limb_t *d = p + n;
//  for (int i=x->_mp_exp;i<0;i++) {
//    *--d = 0;
//    if (d <= p) return true;
//  }
//  for (;;) {
//    if (d <= p) return true;
//    if (s <= x->_mp_d) {
//      for (;;) {
//        *--d = 0;
//        if (d <= p) return true;
//      }
//    }
//    *--d = *--s;
//  }
//}


mp_size_t GmpFixedPoint::n = 0;
LockfreeStack<GmpFixedPointLockfree::Node> GmpFixedPointLockfree::mem_stack;
Mutex GmpFixedPointLockfree::stack_mutex;
std::stack<void*> GmpFixedPointLockfree::allocated_mem;



unsigned int GmpFixedPoint::GmpMandel2(const GmpFixedPoint &cr,
                                       const GmpFixedPoint &ci,
                                       const unsigned int max_iter) {
//cout << "GmpMandel: " << n << ": " << PrintableGmpFixedPoint(n+2,cr) << "; " << PrintableGmpFixedPoint(n+2,ci) << endl;
  const mp_limb_t *const cr_p(cr.p+1);
  if (cr_p[n]) return 1;
  const mp_limb_t *const ci_p(ci.p+1);
  if (ci_p[n]) return 1;

  const mp_size_t n2 = 2*n;
  unsigned int iter = 1;
  mp_limb_t xr[n];
  mp_limb_t xi[n];
  mp_limb_t xr2[n2];
  mp_limb_t xi2[n2];
  mp_limb_t tmp[n2];
  mp_limb_t overflow;
  bool sign_xr = cr.sign;
  bool sign_xi = ci.sign;
  bool sign_tmp;
  mpn_copyi(xr,cr_p,n);
  mpn_copyi(xi,ci_p,n);

  for (;;) {
//std::cout << iter << ": " << ConvertToDouble(n,xr,sign_xr)
//     << ' ' << ConvertToDouble(n,xi,sign_xi) << std::endl;
      // compare xr2+xi2 < 4:
    mpn_sqr(xr2,xr,n); // Re(x)*Re(x)*(1<<(n2*GmpFixedPoint::bits_per_limb-2))
    mpn_sqr(xi2,xi,n); // Im(x)*Im(x)*(1<<(n2*GmpFixedPoint::bits_per_limb-2))
    if (mpn_add_n(tmp,xr2,xi2,n2)) break;
      // tmp = (Re(c)*Re(c)+Im(c)*Im(c))*(1<<(n2*GmpFixedPoint::bits_per_limb-2)).
      // overflow means Re(c)*Re(c)+Im(c)*Im(c))>=4

      // check max_iter:
    iter++;
    if (iter >= max_iter) break;

      // update xi := 2*xr*xi+ci_p
    mpn_mul_n(tmp,xr,xi,n);
    sign_tmp = (sign_xr!=sign_xi);
      // tmp*sign_tmp = (Re(x)*Im(x))*(1<<(n2*GmpFixedPoint::bits_per_limb-2)).
    overflow = mpn_lshift(tmp+n-1,tmp+n-1,n+1,2);
      // (tmp+n)*sign_tmp = (2*Re(x)*Im(x))*(1<<(n*GmpFixedPoint::bits_per_limb-1)))
    if (overflow > 1) {
        // 2*Abs(Re(x)*Im(x)) >= 4
      break;
    }
      // rounding
    if (((((mp_limb_t)1)<<(GmpFixedPoint::bits_per_limb-1)) & tmp[n-1])) {
      if (mpn_add_1(tmp+n,tmp+n,n,1)) {
          // rounded 2*Re(x)*Im(x) == 2
        if (overflow) abort();
        overflow = 1;
      }
    }
    if (overflow) {
        // 2*Abs(Re(x)*Im(x)) >= 2
      if (sign_tmp == ci.sign) break;
        // (tmp+n)*sign_tmp = (2*Re(x)*Im(x)-2)*(1<<(n*GmpFixedPoint::bits_per_limb-1)))
      if (mpn_sub_n(xi,tmp+n,ci_p,n)) {
          // ok
        sign_xi = sign_tmp;
      } else {
        break;
      }
    } else {
      if (sign_tmp == ci.sign) {
        if (mpn_add_n(xi,tmp+n,ci_p,n)) break;
        sign_xi = sign_tmp;
      } else {
        if (mpn_sub_n(xi,tmp+n,ci_p,n)) {
          mpn_neg(xi,xi,n);
          sign_xi = ci.sign;
        } else {
          sign_xi = sign_tmp;
        }
      }
    }
    
      // update xr := xr2-xi2+cr_p
    if (mpn_sub_n(tmp,xr2,xi2,n2)) { // underflow
      sign_tmp = true;
      mpn_neg(tmp,tmp,n2);
    } else {
      sign_tmp = false;
    }
      // tmp*sign_tmp = (Re(x)*Re(x)-Im(x)*Im(x))*(1<<(n2*GmpFixedPoint::bits_per_limb-2)).
    if (mpn_lshift(tmp+n-1,tmp+n-1,n+1,1)) {
        // Abs(Re(x)*Re(x)-Im(x)*Im(x)) >= 2
      if (sign_tmp == cr.sign) break;
        // rounding
      if (((((mp_limb_t)1)<<(GmpFixedPoint::bits_per_limb-1)) & tmp[n-1])) {
        if (mpn_add_1(tmp+n,tmp+n,n,1)) {
            // rounded Abs(Re(x)*Re(x)-Im(x)*Im(x)) >= 2
          if (sign_tmp == cr.sign) break;
        }
      }
      if (mpn_sub_n(xr,tmp+n,cr_p,n)) {
          // good: underflow cancels previous overflow
        sign_xr = sign_tmp;
      } else {
          // Abs(Re(x)*Re(x)-Im(x)*Im(x)+cr_p) >= 2
        break;
      }
    } else {
        // Abs(Re(x)*Re(x)-Im(x)*Im(x)) < 2
        // rounding:
      if ((((((mp_limb_t)1)<<(GmpFixedPoint::bits_per_limb-1)) & tmp[n-1])) &&
          (mpn_add_1(tmp+n,tmp+n,n,1))) {
          // rounded Abs(Re(x)*Re(x)-Im(x)*Im(x)) == 2
        if (sign_tmp == cr.sign) break;
          // xr = 2*sign_tmp+cr_p
        mpn_neg(xr,cr_p,n);
        sign_xr = sign_tmp;
      } else {
          // Rond(Abs(Re(x)*Re(x)-Im(x)*Im(x))) < 2
        if (sign_tmp == cr.sign) {
          if (mpn_add_n(xr,tmp+n,cr_p,n)) break;
          sign_xr = sign_tmp;
        } else {
          if (mpn_sub_n(xr,tmp+n,cr_p,n)) {
            mpn_neg(xr,xr,n);
            sign_xr = cr.sign;
          } else {
            sign_xr = sign_tmp;
          }
        }
      }
    }
      // next step of iteration
  }
  return iter;
}


