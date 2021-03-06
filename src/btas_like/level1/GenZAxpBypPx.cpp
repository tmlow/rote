/*
   Copyright (c) 2009-2013, Jack Poulson
                      2013, Jeff Hammond
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "rote.hpp"

namespace rote{

template<typename T>
bool CheckZAxpPxArgs(const DistTensor<T>& X, const DistTensor<T>& Y, const Permutation& perm, const DistTensor<T>& Z){
	const TensorDistribution xDist = X.TensorDist();
	const TensorDistribution yDist = Y.TensorDist();
	const TensorDistribution zDist = Z.TensorDist();

	bool ret = true;
	ret &= CheckOrder(X.Order(), Y.Order());
	ret &= CheckOrder(X.Order(), Z.Order());
	ret &= (yDist == xDist) && (xDist == zDist);
	ret &= CheckIsValidPermutation(X.Order(), perm);
	return ret;
}

template <typename T>
void GenZAxpBypPx( T alpha, const DistTensor<T>& X, T beta, const DistTensor<T>& Y, const Permutation& perm, DistTensor<T>& Z ){
	if(!CheckZAxpPxArgs(X, Y, perm, Z))
		LogicError("AllToAllDoubleModeRedist: Invalid redistribution request");
	TensorDistribution copy = X.TensorDist();
	std::vector<ModeDistribution> newDistEntries(copy.size());

	for(Unsigned i = 0; i < X.Order(); i++)
		newDistEntries[i] = copy[perm[i]];
	newDistEntries[newDistEntries.size() - 1] = copy[copy.size() - 1];
	TensorDistribution newDist(newDistEntries);

	Z.ResizeTo(X);
	DistTensor<T> tmp(newDist, X.Grid());
	tmp.RedistFrom(X);

	ZAxpBypPx(alpha, X, beta, Y, tmp, perm, Z);
}

//Non-template functions
#define PROTO(T) \
	template bool CheckZAxpPxArgs(const DistTensor<T>& X, const DistTensor<T>& Y, const Permutation& perm, const DistTensor<T>& Z); \
	template void GenZAxpBypPx( T alpha, const DistTensor<T>& X, T beta, const DistTensor<T>& Y, const Permutation& perm, DistTensor<T>& Z );

//PROTO(Unsigned)
//PROTO(Int)
PROTO(float)
PROTO(double)
//PROTO(char)

#ifndef DISABLE_COMPLEX
#ifndef DISABLE_FLOAT
PROTO(std::complex<float>)
#endif
PROTO(std::complex<double>)
#endif

} // namespace rote
