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

////////////////////////////////
// Workhorse interface
////////////////////////////////

template<typename T>
void
DistTensor<T>::ReduceToOneUpdateRedistFrom(const T alpha, const DistTensor<T>& A, const T beta, const ModeArray& rModes)
{
#ifndef RELEASE
    CallStackEntry cse("DistTensor::ReduceToOneUpdateRedistFrom");
#endif
    PROFILE_SECTION("RTORedist");
    Unsigned i, j;
    const tmen::GridView gv = A.GetGridView();
    const tmen::Grid& g = A.Grid();
    TensorDistribution dist = A.TensorDist();
    ModeDistribution blank(0);

    //Sort the rModes just in case
    ModeArray sortedRModes = rModes;
    std::sort(sortedRModes.begin(), sortedRModes.end());

    //Set up tmp for holding alpha*A
    DistTensor<T> tmp(A.TensorDist(), g);
    tmp.AlignWith(A);

    //Set up tmp2 for holding beta*B
    ObjShape tmp2Shape = Shape();
    TensorDistribution tmp2Dist = TensorDist();
    std::vector<Unsigned> tmp2Aligns = Alignments();
    Permutation tmp2Perm = localPerm_;
    std::vector<Unsigned> tmp2Strides = LocalStrides();

    for(i = 0; i < sortedRModes.size(); i++){
        Mode rMode = sortedRModes[i];
        tmp2Dist.insert(tmp2Dist.begin() + rMode, blank);
        tmp2Aligns.insert(tmp2Aligns.begin() + rMode, A.ModeAlignment(rMode));

        for(j = 0; j < tmp2Perm.size(); j++)
            if(tmp2Perm[j] >= rMode)
                tmp2Perm[j]++;
        tmp2Perm.insert(tmp2Perm.begin() + rMode, rMode);

        if(rMode == tmp2Strides.size()){
            if(rMode == 0){
                tmp2Strides.insert(tmp2Strides.begin() + rMode, 1);
            }else{
                tmp2Strides.insert(tmp2Strides.begin() + rMode, tmp2Strides[tmp2Strides.size() - 1] * tmp2Shape[tmp2Shape.size() - 1]);
            }
        }else{
            tmp2Strides.insert(tmp2Strides.begin() + rMode, tmp2Strides[rMode]);
        }
        tmp2Shape.insert(tmp2Shape.begin() + rMode, Min(1, A.Dimension(rMode)));
    }

    DistTensor<T> tmp2(tmp2Shape, tmen::TensorDistToString(tmp2Dist), g);
    tmp2.SetLocalPermutation(tmp2Perm);

    tmp2.Attach(tmp2Shape, tmp2Aligns, Buffer(), tmp2Strides, g);

    if(alpha == T(0))
        Zero(tmp);
    else
        LocalReduce(A, tmp, rModes);

    ModeArray commModes;
    for(i = 0; i < rModes.size(); i++){
        ModeDistribution modeDist = A.ModeDist(rModes[i]);
        commModes.insert(commModes.end(), modeDist.begin(), modeDist.end());
    }
    std::sort(commModes.begin(), commModes.end());

//    PrintData(tmp, "tmp data");
//    PrintData(tmp2, "tmp2 data");
//    Print(tmp, "tmp before RTO");
//    Print(tmp2, "tmp2 before RTO");
    tmp2.ReduceToOneUpdateCommRedist(alpha, tmp, beta, rModes, commModes);

    PROFILE_STOP;
}

////////////////////////////////
// Set interfaces
////////////////////////////////

template <typename T>
void DistTensor<T>::ReduceToOneRedistFrom(const DistTensor<T>& A, const ModeArray& rModes){

    ReduceToOneUpdateRedistFrom(T(1), A, T(0), rModes);
}

template <typename T>
void DistTensor<T>::ReduceToOneRedistFrom(const DistTensor<T>& A, const Mode rMode){
    ModeArray rModes(1);
    rModes[0] = rMode;

    ReduceToOneRedistFrom(A, rModes);
}

template<typename T>
void
DistTensor<T>::ReduceToOneUpdateRedistFrom(const DistTensor<T>& A, const T beta, const ModeArray& reduceModes)
{
#ifndef RELEASE
    CallStackEntry cse("DistTensor::ReduceToOneUpdateRedistFrom");
#endif

    ReduceToOneUpdateRedistFrom(T(1), A, beta, reduceModes);
}

template <typename T>
void DistTensor<T>::ReduceToOneUpdateRedistFrom(const DistTensor<T>& A, const T beta, const Mode rMode){
    ModeArray rModes(1);
    rModes[0] = rMode;

    ReduceToOneUpdateRedistFrom(A, beta, rModes);
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
