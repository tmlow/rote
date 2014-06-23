/*
   Copyright (c) 2009-2013, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef TMEN_BTAS_REDUCE_DECL_HPP
#define TMEN_BTAS_REDUCE_DECL_HPP

namespace tmen{

///////////////////////
// Local routines
///////////////////////

template<typename T>
void LocalReduce(Tensor<T>& B, const Tensor<T>& A, const ModeArray& reduceModes);

template<typename T>
void LocalReduce(Tensor<T>& B, const Tensor<T>& A, const Mode& reduceMode);

///////////////////////
// Global routines
///////////////////////

template<typename T>
void LocalReduce(DistTensor<T>& B, const DistTensor<T>& A, const ModeArray& reduceModes);

template<typename T>
void LocalReduce(DistTensor<T>& B, const DistTensor<T>& A, const Mode& reduceMode);

} // namespace tmen

#endif // ifndef TMEN_BTAS_REDUCE_DECL_HPP
