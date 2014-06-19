/*
   Copyright (c) 2009-2013, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef TMEN_CORE_DISTTENSOR_REDISTRIBUTE_AGINTERFACES_HPP
#define TMEN_CORE_DISTTENSOR_REDISTRIBUTE_AGINTERFACES_HPP

#include "tensormental/core/imports/mpi.hpp"
#include "tensormental/core/imports/choice.hpp"
#include "tensormental/core/imports/mpi_choice.hpp"

#include <vector>
#include <iostream>
#include "tensormental/core/dist_tensor/mc_mr.hpp"

namespace tmen{

////////////////
//Check routines
////////////////
template<typename T>
Int CheckAllGatherRedist(const DistTensor<T>& B, const DistTensor<T>& A, const Mode allGatherMode, const ModeArray& redistModes);

template<typename T>
Int CheckAllGatherRedist(const DistTensor<T>& B, const DistTensor<T>& A, const Mode allGatherMode);

/////////////////
//Redist routines
/////////////////
template<typename T>
void AllGatherRedist(DistTensor<T>& B, const DistTensor<T>& A, const Mode allGatherMode, const ModeArray& redistModes);

template<typename T>
void AllGatherRedist(DistTensor<T>& B, const DistTensor<T>& A, const Mode allGatherMode);


}
#endif // ifndef TMEN_CORE_DISTTENSOR_REDISTRIBUTE_AGINTERFACES_HPP
