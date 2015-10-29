/*
   Copyright (c) 2009-2013, Jack Poulson
                      2013, Jeff Hammond
                      2013, Jed Brown
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "tensormental.hpp"
//#include "/tensormental/mc_mr.hpp"
#include <algorithm>

namespace tmen{

////////////////////////////////
// Workhorse interface
////////////////////////////////

template <typename T>
void
DistTensor<T>::AllGatherRedistFrom(const DistTensor<T>& A, const ModeArray& commModes ){
    PROFILE_SECTION("AGRedist");
    ResizeTo(A);

    ModeArray sortedCommModes = commModes;
    SortVector(sortedCommModes);
    AllGatherCommRedist(A, sortedCommModes);
    PROFILE_STOP;
}

#define PROTO(T) template class DistTensor<T>
#define COPY(T) \
  template DistTensor<T>::DistTensor( const DistTensor<T>& A )
#define FULL(T) \
  PROTO(T);


FULL(Int);
#ifndef DISABLE_FLOAT
FULL(float);
#endif
FULL(double);

#ifndef DISABLE_COMPLEX
#ifndef DISABLE_FLOAT
FULL(std::complex<float>);
#endif
FULL(std::complex<double>);
#endif

} //namespace tmen
