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

#include "Julia.h"

#ifdef X86_64

#include <immintrin.h>
#include <emmintrin.h>

void JuliaAVXdouble4(const double _mr[4],const double _mi[4],int max_n,
                     unsigned int result[4]) {
  __m256d mr = _mm256_loadu_pd(_mr);
  __m256d mi = _mm256_loadu_pd(_mi);
//  __m256d mi = _mm256_broadcast_sd(&_mi);
  __m256d jr = _mm256_xor_pd(mr,mr);
  __m256d ji = jr;
  __m256d count = jr;
  const double _four = 4.0;
  __m256d four = _mm256_broadcast_sd(&_four);
  const double _one = 1.0;
  __m256d one = _mm256_broadcast_sd(&_one);
  for (;;) {
    __m256d rq = _mm256_mul_pd(jr,jr);
    __m256d iq = _mm256_mul_pd(ji,ji);
    {
      __m256d h = _mm256_cmp_pd(_mm256_add_pd(rq,iq),four,_CMP_LT_OQ);
      if (0 == _mm256_movemask_pd(h)) break;
      count = _mm256_add_pd(count,_mm256_and_pd(h,one));
    } // h no longer needed
    if (--max_n <= 0) break;
    ji = _mm256_mul_pd(ji,jr);
    ji = _mm256_add_pd(mi,_mm256_add_pd(ji,ji));
    jr = _mm256_add_pd(mr,_mm256_sub_pd(rq,iq));
  }
  _mm_storeu_si128((__m128i*)result,_mm256_cvtpd_epi32(count));
}

void JuliaAVXfloat8(const float _mr[8],const float _mi[8],int max_n,
                    unsigned int result[8]) {
  __m256 mr = _mm256_loadu_ps(_mr);
  __m256 mi = _mm256_loadu_ps(_mi);
  __m256 jr = _mm256_xor_ps(mr,mr);
  __m256 ji = jr;
  __m256 count = jr;
  const float _four = 4.0f;
  __m256 four = _mm256_broadcast_ss(&_four);
  const float _one = 1.0f;
  __m256 one = _mm256_broadcast_ss(&_one);
  for (;;) {
    __m256 rq = _mm256_mul_ps(jr,jr);
    __m256 iq = _mm256_mul_ps(ji,ji);
    {
      __m256 h = _mm256_cmp_ps(_mm256_add_ps(rq,iq),four,_CMP_LT_OQ);
      if (0 == _mm256_movemask_ps(h)) break;
      count = _mm256_add_ps(count,_mm256_and_ps(h,one));
    } // h no longer needed
    if (--max_n <= 0) break;
    ji = _mm256_mul_ps(ji,jr);
    ji = _mm256_add_ps(mi,_mm256_add_ps(ji,ji));
    jr = _mm256_add_ps(mr,_mm256_sub_ps(rq,iq));
  }
  _mm256_storeu_si256((__m256i*)result,_mm256_cvtps_epi32(count));
}

#else

unsigned int JuliaC(double mr,double mi,
                    double jr,double ji,
                    unsigned int max_n) {
  unsigned int n = 0;
  while (n < max_n) {
    double rq = jr*jr;
    double iq = ji*ji;
    if (rq+iq > 4.0) return n;
    ji *= jr;
    ji = ji + ji + mi;
    jr = rq - iq + mr;
    n++;
  }
  return n;
//  return (0x10000*n+(max_n/2))/max_n;
}

#endif
