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

#ifndef JULIA_AVX_H_
#define JULIA_AVX_H_

 //#ifdef __cplusplus
//extern "C" {
//#endif


#ifdef X86_64

#define VECTOR_SIZE 4
#define VECTOR_TYPE double
#define JULIA_FUNC JuliaAVXdouble4
//#define VECTOR_SIZE 8
//#define VECTOR_TYPE float
//#define JULIA_FUNC JuliaAVXfloat8

void JuliaAVXdouble4(const double mr[4],const double mi[4],int max_n,
                     unsigned int result[4]);

void JuliaAVXfloat8(const float mr[8],const float mi[8],int max_n,
                    unsigned int result[8]);


#else

#define VECTOR_SIZE 1
#define VECTOR_TYPE double
#define JULIA_FUNC(mr,mi,max_iter,d) d[0]=JuliaC(mr[0],mi[0],0.0,0.0,max_iter)

unsigned int JuliaC(double mr,double mi,
                    double jr,double ji,
                    unsigned int max_n);

#endif

//#ifdef __cplusplus
//}
//#endif

#endif
