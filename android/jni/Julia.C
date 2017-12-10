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

#include "Julia.H"

#ifdef X86_64

#include <immintrin.h>
#include <emmintrin.h>

void JuliaAVXdouble4(const double _mr[4],const double _mi[4],int max_n,
                     unsigned int result[4]) {
  __m256d mr = _mm256_loadu_pd(_mr);
mr = _mm256_add_pd(mr,mr);
  __m256d mi = _mm256_loadu_pd(_mi);
mi = _mm256_add_pd(mi,mi);
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
mr = _mm256_add_ps(mr,mr);
  __m256 mi = _mm256_loadu_ps(_mi);
mi = _mm256_add_ps(mi,mi);
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

#ifdef __aarch64__
#include <arm_neon.h>

void JuliaNEONdouble2(const double _mr[2],const double _mi[2],unsigned int max_n,
                      unsigned int result[2]) {
  float64x2_t mr = vld1q_f64(_mr);
mr = vaddq_f64(mr,mr);
  float64x2_t mi = vld1q_f64(_mi);
mi = vaddq_f64(mi,mi);
  float64x2_t jr = vsubq_f64(mr,mr);
  float64x2_t ji = jr;
  const uint64x2_t max = vdupq_n_u64(max_n);
  uint64x2_t count = veorq_u64(max,max);
  const const float64x2_t four = vdupq_n_f64(4.0);
  uint64x2_t continue_loop = vdupq_n_s64(-1);
  float64x2_t rq = vmulq_f64(jr,jr);
  float64x2_t iq = vmulq_f64(ji,ji);
  bool cont_loop = true;
  do {
    continue_loop = vandq_u64(continue_loop,vcltq_f64(vaddq_f64(rq,iq),four));
      // moving the result of SIMD calculation to
      // general purpose regs takes some time.
      // Therefore assign finish_loop here and read later:
//      continue_loop = ((vgetq_lane_u32(continue_loop,0) | vgetq_lane_u32(continue_loop,2)));
    count = vsubq_u64(count,continue_loop);
    continue_loop = vandq_u64(continue_loop,vcltq_u64(count,max));
    cont_loop = vget_lane_u32(vorr_u32(vget_low_u32(continue_loop),
                                       vget_high_u32(continue_loop)),0);
    ji = vmulq_f64(ji,jr);
    ji = vaddq_f64(mi,vaddq_f64(ji,ji));
    jr = vaddq_f64(mr,vsubq_f64(rq,iq));
    rq = vmulq_f64(jr,jr);
    iq = vmulq_f64(ji,ji);

// loop unrooling: do it again

    continue_loop = vandq_u64(continue_loop,vcltq_f64(vaddq_f64(rq,iq),four));
    count = vsubq_u64(count,continue_loop);
    continue_loop = vandq_u64(continue_loop,vcltq_u64(count,max));
      
      
    ji = vmulq_f64(ji,jr);
    ji = vaddq_f64(mi,vaddq_f64(ji,ji));
    jr = vaddq_f64(mr,vsubq_f64(rq,iq));
    
    rq = vmulq_f64(jr,jr);
    iq = vmulq_f64(ji,ji);

/*
    continue_loop = vandq_u64(continue_loop,vcltq_f64(vaddq_f64(rq,iq),four));
    continue_loop = vandq_u64(continue_loop,vcltq_u64(count,max));
    count = vsubq_u64(count,continue_loop);
      
      
    ji = vmulq_f64(ji,jr);
    ji = vaddq_f64(mi,vaddq_f64(ji,ji));
    jr = vaddq_f64(mr,vsubq_f64(rq,iq));
    
    rq = vmulq_f64(jr,jr);
    iq = vmulq_f64(ji,ji);
*/
  } while (cont_loop);

//  count = vminq_u32(max,count); // not necessary
  
    // vget_low_u32 just gets the lower quadword.
    // Not the lower int of the lower and higher quadwords.

//    vst1_u32 seems to store one quadword:
//  vst1_u32(result,vget_low_u32(count)); is the same as
//  *((uint32x2_t*)(&(result[0]))) = vget_low_u32(count);

//  result[0] = count[0];
//  result[1] = count[1];
    // same as above:
  result[0] = vgetq_lane_u32(count,0);
  result[1] = vgetq_lane_u32(count,2);
}

#else
unsigned int JuliaC(double mr,double mi,
                    double jr,double ji,
                    unsigned int max_n) {
mr *= 2.0;
mi *= 2.0;
jr *= 2.0;
ji *= 2.0;
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
#endif
