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

#ifndef MPF_CLASS_H_
#define MPF_CLASS_H_

#include "GmpFloat.H"

typedef GmpFloat FLOAT_TYPE;

/*
#include <gmp.h>
#include "IntrusivePtrCapable.H"
#include "IntrusivePtr.H"

#include <iostream>

struct MpfStruct : public IntrusivePtrCapable, private __mpf_struct {
  friend class MpfClass;
  //mpf_ptr get_mpf_ptr(void) {return static_cast<__mpf_struct*>(this);}
  static MpfStruct *create(int prec) {return new MpfStruct(prec,'\0');}
  explicit MpfStruct(double x) {mpf_init_set_d(this,x);}
  MpfStruct(double x,int prec) {mpf_init2(this,prec);mpf_set_d(this,x);}
  MpfStruct(mpf_t x,int prec) {mpf_init2(this,prec);mpf_set(this,x);}
  ~MpfStruct(void) {mpf_clear(this);}
private:
  explicit MpfStruct(int prec,char) {mpf_init2(this,prec);}
};

static inline int max(int a,int b) {return (a>b) ? a : b;}

class MpfClass : public IntrusivePtr<MpfStruct> {
public:
  MpfClass(void) {}
  explicit MpfClass(int prec,char) : IntrusivePtr<MpfStruct>(MpfStruct::create(prec)) {}
  MpfClass(double x) : IntrusivePtr<MpfStruct>(new MpfStruct(x)) {}
  MpfClass(double x,int prec) : IntrusivePtr<MpfStruct>(new MpfStruct(x,prec)) {}
  MpfClass(mpf_t x,int prec) : IntrusivePtr<MpfStruct>(new MpfStruct(x,prec)) {}
  MpfClass(mpf_t x) : IntrusivePtr<MpfStruct>(new MpfStruct(x,mpf_get_prec(x))) {}
  __mpf_struct *get_mpf_t(void) const {return static_cast<__mpf_struct*>(get());}
  int get_prec(void) const {return mpf_get_prec(get_mpf_t());}
  void set_prec(int prec) {
    if (get()) mpf_set_prec(get_mpf_t(),prec);
    else reset(new MpfStruct(prec));
  }
  MpfClass copy(void) const {return MpfClass(get_mpf_t());}
  MpfClass operator-(void) const {
    MpfClass rval(get_mpf_t());
    rval->_mp_size = -rval->_mp_size;
    return rval;
  }
  MpfClass abs(void) const {
    MpfClass rval(get_mpf_t());
    if (rval->_mp_size < 0) rval->_mp_size = -rval->_mp_size;
    return rval;
  }
  int cmp(const MpfClass &b) const {return mpf_cmp(get_mpf_t(),b.get_mpf_t());}
  int cmp_d(double b) const {return mpf_cmp_d(get_mpf_t(),b);}
  int cmp_ui(unsigned long int b) const {return mpf_cmp_ui(get_mpf_t(),b);}
  int cmp_si(signed long int b) const {return mpf_cmp_si(get_mpf_t(),b);}

  bool operator<(const MpfClass &b) const {return (cmp(b) < 0);}
  bool operator>(const MpfClass &b) const {return (cmp(b) > 0);}
  bool operator==(const MpfClass &b) const {return (cmp(b) == 0);}
  bool operator<=(const MpfClass &b) const {return (cmp(b) <= 0);}
  bool operator>=(const MpfClass &b) const {return (cmp(b) >= 0);}
  bool operator!=(const MpfClass &b) const {return (cmp(b) != 0);}
  bool operator<(double b) const {return (cmp_d(b) < 0);}
  bool operator>(double b) const {return (cmp_d(b) > 0);}
  bool operator==(double b) const {return (cmp_d(b) == 0);}
  bool operator<=(double b) const {return (cmp_d(b) <= 0);}
  bool operator>=(double b) const {return (cmp_d(b) >= 0);}
  bool operator!=(double b) const {return (cmp_d(b) != 0);}
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

  double get_d(void) const {return mpf_get_d(get_mpf_t());}

  const MpfClass &operator+=(const MpfClass &b) {
    if (hasJustOneReference()) {
      set_prec(max(get_prec(),b.get_prec()));
      mpf_add(get_mpf_t(),get_mpf_t(),b.get_mpf_t());
    } else {
      MpfStruct *rval = new MpfStruct(max(get_prec(),b.get_prec()));
      mpf_add(rval,get_mpf_t(),b.get_mpf_t());
      reset(rval);
    }
    return *this;
  }
  const MpfClass &operator-=(const MpfClass &b) {
    if (hasJustOneReference()) {
      set_prec(max(get_prec(),b.get_prec()));
      mpf_sub(get_mpf_t(),get_mpf_t(),b.get_mpf_t());
    } else {
      MpfStruct *rval = new MpfStruct(max(get_prec(),b.get_prec()));
      mpf_sub(rval,get_mpf_t(),b.get_mpf_t());
      reset(rval);
    }
    return *this;
  }
  const MpfClass &operator*=(const MpfClass &b) {
    if (hasJustOneReference()) {
      set_prec(max(get_prec(),b.get_prec()));
      mpf_mul(get_mpf_t(),get_mpf_t(),b.get_mpf_t());
    } else {
      MpfStruct *rval = new MpfStruct(max(get_prec(),b.get_prec()));
      mpf_mul(rval,get_mpf_t(),b.get_mpf_t());
      reset(rval);
    }
    return *this;
  }
  const MpfClass &operator/=(const MpfClass &b) {
    if (hasJustOneReference()) {
      set_prec(max(get_prec(),b.get_prec()));
      mpf_div(get_mpf_t(),get_mpf_t(),b.get_mpf_t());
    } else {
      MpfStruct *rval = new MpfStruct(max(get_prec(),b.get_prec()));
      mpf_div(rval,get_mpf_t(),b.get_mpf_t());
      reset(rval);
    }
    return *this;
  }
private:
  bool hasJustOneReference(void) const {return get()->hasJustOneReference();}
};

typedef MpfClass FLOAT_TYPE;

static inline
MpfClass operator+(const MpfClass &a,const MpfClass &b) {
  MpfClass rval(max(a.get_prec(),b.get_prec()));
  mpf_add(rval.get_mpf_t(),a.get_mpf_t(),b.get_mpf_t());
  return rval;
}

static inline
MpfClass operator+(const MpfClass &a,unsigned long int b) {
  MpfClass rval(a.get_prec());
  mpf_add_ui(rval.get_mpf_t(),a.get_mpf_t(),b);
  return rval;
}

static inline
MpfClass operator+(unsigned long int b,const MpfClass &a) {
  MpfClass rval(a.get_prec());
  mpf_add_ui(rval.get_mpf_t(),a.get_mpf_t(),b);
  return rval;
}

static inline
MpfClass operator-(const MpfClass &a,const MpfClass &b) {
  MpfClass rval(max(a.get_prec(),b.get_prec()));
  mpf_sub(rval.get_mpf_t(),a.get_mpf_t(),b.get_mpf_t());
  return rval;
}

static inline
MpfClass operator-(const MpfClass &a,unsigned long int b) {
  MpfClass rval(a.get_prec());
  mpf_sub_ui(rval.get_mpf_t(),a.get_mpf_t(),b);
  return rval;
}

static inline
MpfClass operator-(unsigned long int b,const MpfClass &a) {
  MpfClass rval(a.get_prec());
  mpf_ui_sub(rval.get_mpf_t(),b,a.get_mpf_t());
  return rval;
}

static inline
MpfClass operator*(const MpfClass &a,const MpfClass &b) {
  MpfClass rval(max(a.get_prec(),b.get_prec()));
  mpf_mul(rval.get_mpf_t(),a.get_mpf_t(),b.get_mpf_t());
  return rval;
}

static inline
MpfClass operator*(const MpfClass &a,unsigned long int b) {
  MpfClass rval(a.get_prec());
  mpf_mul_ui(rval.get_mpf_t(),a.get_mpf_t(),b);
  return rval;
}

static inline
MpfClass operator*(unsigned long int b,const MpfClass &a) {
  MpfClass rval(a.get_prec());
  mpf_mul_ui(rval.get_mpf_t(),a.get_mpf_t(),b);
  return rval;
}

static inline
MpfClass operator/(const MpfClass &a,const MpfClass &b) {
  MpfClass rval(max(a.get_prec(),b.get_prec()));
  mpf_div(rval.get_mpf_t(),a.get_mpf_t(),b.get_mpf_t());
  return rval;
}

static inline
MpfClass operator/(const MpfClass &a,unsigned long int b) {
  MpfClass rval(a.get_prec());
  mpf_div_ui(rval.get_mpf_t(),a.get_mpf_t(),b);
  return rval;
}

static inline
MpfClass operator/(unsigned long int b,const MpfClass &a) {
  MpfClass rval(a.get_prec());
  mpf_ui_div(rval.get_mpf_t(),b,a.get_mpf_t());
  return rval;
}

static inline
MpfClass sqrt(const MpfClass &a) {
  MpfClass rval(a.get_prec());
  mpf_sqrt(rval.get_mpf_t(),a.get_mpf_t());
  return rval;
}

static inline
MpfClass floor(const MpfClass &a) {
  MpfClass rval(a.get_prec());
  mpf_floor(rval.get_mpf_t(),a.get_mpf_t());
  return rval;
}

static inline
MpfClass abs(const MpfClass &a) {
  return a.abs();
}

static inline
MpfClass mul_2exp(const MpfClass &a,int exp) {
  MpfClass rval(a.get_prec());
  if (exp > 0) mpf_mul_2exp(rval.get_mpf_t(),a.get_mpf_t(),exp);
  else if (exp < 0) mpf_div_2exp(rval.get_mpf_t(),a.get_mpf_t(),-exp);
  return rval;
}

inline
std::ostream &operator<<(std::ostream &o,const MpfClass &x) {
  return o << x.get_mpf_t();
}
*/

#endif
