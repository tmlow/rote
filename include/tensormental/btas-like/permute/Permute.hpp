/*
   Copyright (c) 2009-2013, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef TMEN_BTAS_PERMUTE_HPP
#define TMEN_BTAS_PERMUTE_HPP

namespace tmen{


//Loop based permutation
template<typename T>
void Permute(Tensor<T>& B, const Tensor<T>& A, const Permutation& perm){
    Unsigned order = A.Order();
    T* dstBuf = B.Buffer();
    const T * srcBuf = A.LockedBuffer();

    Location zeros(order, 0);
    Location ones(order, 1);
    Permutation invperm = DetermineInversePermutation(perm);
    PackData data;
    data.loopShape = A.Shape();
    data.srcBufStrides = A.Strides();
    data.dstBufStrides = FilterVector(B.Strides(), invperm);
    data.loopStarts = zeros;
    data.loopIncs = ones;

    B.PackCommHelper(data, order - 1, &(srcBuf[0]), &(dstBuf[0]));
}

} // namespace tmen

#endif // ifndef TMEN_BTAS_PERMUTE_HPP
