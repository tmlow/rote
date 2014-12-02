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
#include <algorithm>

namespace tmen{

template<typename T>
void
DistTensor<T>::GatherToOneRedistFrom(const DistTensor<T>& A, const Mode gMode)
{
#ifndef RELEASE
    CallStackEntry cse("DistTesnor::GatherToOneRedistFrom");
#endif
    ModeArray gModes(1);
    gModes[0] = gMode;
    std::vector<ModeArray> commGroups(1);
    commGroups[0] = A.ModeDist(gMode);
    GatherToOneRedistFrom(A, gModes, commGroups);
}

template <typename T>
void DistTensor<T>::GatherToOneRedistFrom(const DistTensor<T>& A, const Mode gMode, const ModeArray& gridModes){
    ModeArray gModes(1);
    gModes[0] = gMode;
    std::vector<ModeArray> commGroups(1);
    commGroups[0] = gridModes;
    GatherToOneRedistFrom(A, gModes, commGroups);
}

template <typename T>
void DistTensor<T>::GatherToOneRedistFrom(const DistTensor<T>& A, const ModeArray& gModes, const std::vector<ModeArray>& gridModes){
    Unsigned i;
    ResizeTo(A);
    ModeArray commModes;
    for(i = 0; i < gridModes.size(); i++)
        commModes.insert(commModes.end(), gridModes[i].begin(), gridModes[i].end());
    std::sort(commModes.begin(), commModes.end());
    GatherToOneCommRedist(A, commModes);
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
