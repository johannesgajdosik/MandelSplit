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

#ifndef GMP_FLOAT_H_
#define GMP_FLOAT_H_

#include <gmp.h>
#include <math.h>

#include <iostream>

static inline int max(int a,int b) {return (a>b) ? a : b;}



class GmpFloat : public __mpf_struct {
public:
  ~GmpFloat(void) {mpf_clear(this);}

    // construction
  GmpFloat(void) {mpf_init_set_ui(this,0);}
  GmpFloat(int x) {mpf_init_set_si(this,x);}
  GmpFloat(unsigned int x) {mpf_init_set_ui(this,x);}
  GmpFloat(long int x) {mpf_init_set_si(this,x);}
  GmpFloat(unsigned long int x) {mpf_init_set_ui(this,x);}
  GmpFloat(float x) {mpf_init_set_d(this,x);}
  GmpFloat(double x) {mpf_init_set_d(this,x);}
  GmpFloat(const GmpFloat &x) {mpf_init_set(this,&x);}
  GmpFloat(int x,int prec) {mpf_init2(this,prec);mpf_set_si(this,x);}
  GmpFloat(unsigned int x,int prec) {mpf_init2(this,prec);mpf_set_ui(this,x);}
  GmpFloat(long int x,int prec) {mpf_init2(this,prec);mpf_set_si(this,x);}
  GmpFloat(unsigned long int x,int prec) {mpf_init2(this,prec);mpf_set_ui(this,x);}
  GmpFloat(float x,int prec) {mpf_init2(this,prec);mpf_set_d(this,x);}
  GmpFloat(double x,int prec) {mpf_init2(this,prec);mpf_set_d(this,x);}
  GmpFloat(const GmpFloat &x,int prec) {mpf_init2(this,prec);mpf_set(this,&x);}
  GmpFloat(int,int,int prec) {mpf_init2(this,prec);}
  GmpFloat(const __mpf_struct *x) {mpf_init_set(this,x);}
  const __mpf_struct *get_mpf_t(void) const {return this;}

    // swap
  void swap(GmpFloat &a) {
    {const int h=_mp_prec;_mp_prec=a._mp_prec;a._mp_prec=h;}
    {const int h=_mp_size;_mp_size=a._mp_size;a._mp_size=h;}
    {const mp_exp_t h=_mp_exp;_mp_exp=a._mp_exp;a._mp_exp=h;}
    {mp_limb_t *const h=_mp_d;_mp_d=a._mp_d;a._mp_d=h;}
  }

    // precision
  int get_prec(void) const {return mpf_get_prec(this);}
  void set_prec(int prec) {mpf_set_prec(this,prec);}

    // assignment,conversion
  const GmpFloat &operator=(int x) {mpf_set_si(this,x);return *this;}
  const GmpFloat &operator=(unsigned int x) {mpf_set_si(this,x);return *this;}
  const GmpFloat &operator=(long int x) {mpf_set_ui(this,x);return *this;}
  const GmpFloat &operator=(unsigned long int x) {mpf_set_ui(this,x);return *this;}
  const GmpFloat &operator=(float x) {mpf_set_d(this,x);return *this;}
  const GmpFloat &operator=(double x) {mpf_set_d(this,x);return *this;}
  const GmpFloat &operator=(const GmpFloat &x) {mpf_set(this,&x);return *this;}
  double get_d(void) const {return mpf_get_d(this);}

    // comparison
  int cmp(const GmpFloat &b) const {return mpf_cmp(this,&b);}
  int cmp_d(double b) const {return mpf_cmp_d(this,b);}
  int cmp_ui(unsigned long int b) const {return mpf_cmp_ui(this,b);}
  int cmp_si(signed long int b) const {return mpf_cmp_si(this,b);}
  bool operator<(const GmpFloat &b) const {return (cmp(b) < 0);}
  bool operator>(const GmpFloat &b) const {return (cmp(b) > 0);}
  bool operator==(const GmpFloat &b) const {return (cmp(b) == 0);}
  bool operator<=(const GmpFloat &b) const {return (cmp(b) <= 0);}
  bool operator>=(const GmpFloat &b) const {return (cmp(b) >= 0);}
  bool operator!=(const GmpFloat &b) const {return (cmp(b) != 0);}
  bool operator<(double b) const {return (cmp_d(b) < 0);}
  bool operator>(double b) const {return (cmp_d(b) > 0);}
  bool operator==(double b) const {return (cmp_d(b) == 0);}
  bool operator<=(double b) const {return (cmp_d(b) <= 0);}
  bool operator>=(double b) const {return (cmp_d(b) >= 0);}
  bool operator!=(double b) const {return (cmp_d(b) != 0);}
  bool operator<(float b) const {return (cmp_d(b) < 0);}
  bool operator>(float b) const {return (cmp_d(b) > 0);}
  bool operator==(float b) const {return (cmp_d(b) == 0);}
  bool operator<=(float b) const {return (cmp_d(b) <= 0);}
  bool operator>=(float b) const {return (cmp_d(b) >= 0);}
  bool operator!=(float b) const {return (cmp_d(b) != 0);}
  bool operator<(unsigned long int b) const {return (cmp_ui(b) < 0);}
  bool operator>(unsigned long int b) const {return (cmp_ui(b) > 0);}
  bool operator==(unsigned long int b) const {return (cmp_ui(b) == 0);}
  bool operator<=(unsigned long int b) const {return (cmp_ui(b) <= 0);}
  bool operator>=(unsigned long int b) const {return (cmp_ui(b) >= 0);}
  bool operator!=(unsigned long int b) const {return (cmp_ui(b) != 0);}
  bool operator<(signed long int b) const {return (cmp_si(b) < 0);}
  bool operator>(signed long int b) const {return (cmp_si(b) > 0);}
  bool operator==(signed long int b) const {return (cmp_si(b) == 0);}
  bool operator<=(signed long int b) const {return (cmp_si(b) <= 0);}
  bool operator>=(signed long int b) const {return (cmp_si(b) >= 0);}
  bool operator!=(signed long int b) const {return (cmp_si(b) != 0);}
  bool operator<(unsigned int b) const {return (cmp_ui(b) < 0);}
  bool operator>(unsigned int b) const {return (cmp_ui(b) > 0);}
  bool operator==(unsigned int b) const {return (cmp_ui(b) == 0);}
  bool operator<=(unsigned int b) const {return (cmp_ui(b) <= 0);}
  bool operator>=(unsigned int b) const {return (cmp_ui(b) >= 0);}
  bool operator!=(unsigned int b) const {return (cmp_ui(b) != 0);}
  bool operator<(signed int b) const {return (cmp_si(b) < 0);}
  bool operator>(signed int b) const {return (cmp_si(b) > 0);}
  bool operator==(signed int b) const {return (cmp_si(b) == 0);}
  bool operator<=(signed int b) const {return (cmp_si(b) <= 0);}
  bool operator>=(signed int b) const {return (cmp_si(b) >= 0);}
  bool operator!=(signed int b) const {return (cmp_si(b) != 0);}

    // arithmetic
  GmpFloat operator-(void) const {
    GmpFloat rval(*this);
    rval._mp_size = -rval._mp_size;
    return rval;
  }
  const GmpFloat &operator+=(const GmpFloat &b) {
    set_prec(max(get_prec(),b.get_prec()));
    mpf_add(this,this,&b);
    return *this;
  }
  const GmpFloat &operator-=(const GmpFloat &b) {
    set_prec(max(get_prec(),b.get_prec()));
    mpf_sub(this,this,&b);
    return *this;
  }
  const GmpFloat &operator*=(const GmpFloat &b) {
    set_prec(max(get_prec(),b.get_prec()));
    mpf_mul(this,this,&b);
    return *this;
  }
  const GmpFloat &operator/=(const GmpFloat &b) {
    set_prec(max(get_prec(),b.get_prec()));
    mpf_div(this,this,&b);
    return *this;
  }
};

static inline
GmpFloat operator+(const GmpFloat &a,const GmpFloat &b) {
  GmpFloat rval(0,0,max(a.get_prec(),b.get_prec()));
  mpf_add(&rval,&a,&b);
  return rval;
}

static inline
GmpFloat operator-(const GmpFloat &a,const GmpFloat &b) {
  GmpFloat rval(0,0,max(a.get_prec(),b.get_prec()));
  mpf_sub(&rval,&a,&b);
  return rval;
}

static inline
GmpFloat operator*(const GmpFloat &a,const GmpFloat &b) {
  GmpFloat rval(0,0,max(a.get_prec(),b.get_prec()));
  mpf_mul(&rval,&a,&b);
  return rval;
}

static inline
GmpFloat operator/(const GmpFloat &a,const GmpFloat &b) {
  GmpFloat rval(0,0,max(a.get_prec(),b.get_prec()));
  mpf_div(&rval,&a,&b);
  return rval;
}

static inline
GmpFloat abs(const GmpFloat &a) {
  GmpFloat rval(a);
  if (rval._mp_size < 0) rval._mp_size = -rval._mp_size;
  return rval;
}

static inline
GmpFloat mul_2exp(const GmpFloat &a,int exp) {
  GmpFloat rval(0,0,a.get_prec());
  if (exp > 0) mpf_mul_2exp(&rval,&a,exp);
  else if (exp < 0) mpf_div_2exp(&rval,&a,-exp);
  return rval;
}

static inline
GmpFloat sqrt(const GmpFloat &a) {
  GmpFloat rval(0,0,a.get_prec());
  mpf_sqrt(&rval,&a);
  return rval;
}

#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

static inline
double ln2(const GmpFloat &a) {
  __mpf_struct tmp(a);
  int rval = tmp._mp_exp * (sizeof(mp_limb_t) * 8);
  tmp._mp_exp = 0;
  return rval + log(mpf_get_d(&tmp))*(1.0/M_LN2);
}

static inline
double atan2(const GmpFloat &y,const GmpFloat &x) {
  __mpf_struct tmp_x(x);
  __mpf_struct tmp_y(y);
  if (tmp_y._mp_size) {
    tmp_x._mp_exp -= tmp_y._mp_exp;
    tmp_y._mp_exp = 0;
  } else {
      // y == 0
    tmp_x._mp_exp = 0;
  }
  return atan2(mpf_get_d(&tmp_y),mpf_get_d(&tmp_x));
}



static inline
GmpFloat floor(const GmpFloat &a) {
  GmpFloat rval(0,0,a.get_prec());
  mpf_floor(&rval,&a);
  return rval;
}


static inline
std::ostream &operator<<(std::ostream &o,const GmpFloat &x) {
  const std::streamsize last_prec = o.precision();
  o.precision((int)floor(x._mp_prec*sizeof(mp_limb_t)*2.40823996531185+1.0) );
  o << static_cast<const __mpf_struct*>(&x);
  o.precision(last_prec);
  return o;
}

static inline
std::istream &operator>>(std::istream &i,GmpFloat &x) {
  return i >> static_cast<__mpf_struct*>(&x);
}

#endif
