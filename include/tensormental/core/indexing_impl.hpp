/*
   Copyright (c) 2009-2013, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef TMEN_CORE_INDEXING_IMPL_HPP
#define TMEN_CORE_INDEXING_IMPL_HPP

namespace tmen {
/*
inline Int
DiagonalLength( const std::vector<Int>& dims, Int offset )
{
    if( offset > 0 )
    {
        Int remainingWidth = Max(width-offset,0);
        return Min(height,remainingWidth);
    }
    else
    {
        Int remainingHeight = Max(height+offset,0);
        return Min(remainingHeight,width);
    }
}
*/
inline Int GCD( Int a, Int b )
{
#ifndef RELEASE
    if( a < 0 || b < 0 )
        LogicError("GCD called with negative argument");
#endif
    return GCD_( a, b );
}

inline Int GCD_( Int a, Int b )
{
    if( b == 0 )
        return a;
    else
        return GCD_( b, a-b*(a/b) );
}

inline Int LCM(Int a, Int b)
{
#ifndef RELEASE
    if( a < 0 || b < 0 )
        LogicError("LCM called with negative argument");
#endif
    return LCM_(a, b);
}

inline Int LCM_( Int a, Int b )
{
    if(a == 0 || b == 0)
        return 0;
    return a*b/(GCD(a, b));
}

inline Int Length( Int n, Int shift, Int stride )
{
#ifndef RELEASE
    CallStackEntry entry("Length");
    if( n < 0 )
        LogicError("n must be non-negative");
    if( shift < 0 || shift >= stride )
    {
        std::ostringstream msg;
        msg << "Invalid shift: "
            << "shift=" << shift << ", stride=" << stride;
        LogicError( msg.str() );
    }
    if( stride <= 0 )
        LogicError("Modulus must be positive");
#endif
    return Length_( n, shift, stride );
}

inline Int Length_( Int n, Int shift, Int stride )
{
    return ( n > shift ? (n - shift - 1)/stride + 1 : 0 );
}

inline Int
Length( Int n, Int rank, Int alignment, Int stride )
{
#ifndef RELEASE
    CallStackEntry entry("Length");
#endif
    Int shift = Shift( rank, alignment, stride );
    return Length( n, shift, stride );
}

inline Int Length_
( Int n, Int rank, Int alignment, Int stride )
{
    Int shift = Shift_( rank, alignment, stride );
    return Length_( n, shift, stride );
}

inline std::vector<Int>
Lengths( const std::vector<Int>& dimensions, const std::vector<Int>& shifts, const std::vector<Int>& strides )
{
#ifndef RELEASE
	int i;
	int order;
    CallStackEntry entry("Length");
    if(!(dimensions.size() == shifts.size() && dimensions.size() == strides.size())){
    	LogicError("dimensions, shifts, strides must contain same number of elements.");
    }
    order = dimensions.size();
    for(i = 0; i < order; i++){
		if( dimensions[i] < 0 )
			LogicError("dimensions must be non-negative");
		if( shifts[i] < 0 || shifts[i] >= strides[i] )
		{
			std::ostringstream msg;
			msg << "Invalid shift: "
				<< "shift[" << i << "]=" << shifts[i] << ", stride[" << i << "]=" << strides[i];
			LogicError( msg.str() );
		}
		if( strides[i] <= 0 )
			LogicError("Modulus must be positive");
    }
#endif
    return Lengths_( dimensions, shifts, strides );
}

inline std::vector<Int> Lengths_
(const std::vector<Int>& dimensions, const std::vector<Int>& shifts, const std::vector<Int>& strides){
	Unsigned i;
	std::vector<Int> lengths(dimensions.size());
	for(i = 0; i < dimensions.size(); i++)
		lengths[i] = Length(dimensions[i], shifts[i], strides[i]);
	return lengths;
}

inline Int MaxLength( Int n, Int stride )
{
#ifndef RELEASE
    CallStackEntry entry("MaxLength");
    if( n < 0 )
        LogicError("n must be non-negative");
    if( stride <= 0 )
        LogicError("Modulus must be positive");
#endif
    return MaxLength_( n, stride );
}

inline Int MaxLength_( Int n, Int stride )
{
    return ( n > 0 ? (n - 1)/stride + 1 : 0 );
}

inline
std::vector<Int> MaxLengths( const std::vector<Int>& shape, const std::vector<Int>& wrapShape)
{
#ifndef RELEASE
    CallStackEntry entry("MaxLengths");
    if(wrapShape.size() != shape.size())
        LogicError("shape order and wrapShape order must be the same");
    if( AnyNegativeElem(shape) )
        LogicError("shape entries must be non-negative");
    if( AnyNonPositiveElem(wrapShape) )
        LogicError("wrapShape entries must be positive");
#endif
    return MaxLengths_( shape, wrapShape );
}

inline
std::vector<Int> MaxLengths_( const std::vector<Int>& shape, const std::vector<Int>& wrapShape)
{
    std::vector<Int> ret(shape.size());
    for(Unsigned i = 0; i < ret.size(); i++)
        ret[i] = MaxLength(shape[i], wrapShape[i]);
    return ret;
}
// For determining the first index assigned to a given rank
inline Int Shift( Int rank, Int alignment, Int stride )
{
#ifndef RELEASE
    CallStackEntry entry("Shift");
    if( rank < 0 || rank >= stride )
    {
        std::ostringstream msg;
        msg << "Invalid rank: "
            << "rank=" << rank << ", stride=" << stride;
        LogicError( msg.str() );
    }
    if( alignment < 0 || alignment >= stride )
    {
        std::ostringstream msg;
        msg << "Invalid alignment: "
            << "alignment=" << alignment << ", stride=" << stride;
        LogicError( msg.str() );
    }
    if( stride <= 0 )
        LogicError("Stride must be positive");
#endif
    return Shift_( rank, alignment, stride );
}

inline Int Shift_( Int rank, Int alignment, Int stride )
{ return (rank + stride - alignment) % stride; }

inline std::vector<Int> Dimensions2Strides(const std::vector<Int>& dimensions)
{
  std::vector<Int> strides(dimensions.size());
  if(strides.size() > 0){
	  strides[0] = 1;
	  for(Unsigned i = 1; i < strides.size(); i++){
		  strides[i] = strides[i-1]*dimensions[i-1];
	  }
  }
  return strides;
}

inline Int LinearIndex_(const std::vector<Int>& index, const std::vector<Int>& strides)
{
	Int linearInd = 0;
	for(Unsigned i = 0; i < index.size(); i++)
		linearInd += index[i] * strides[i];
	return linearInd;
}

inline Int LinearIndex(const std::vector<Int>& index, const std::vector<Int>& strides)
{
	if(index.size() !=strides.size())
		LogicError( "Invalid index+stride combination");
	if(AnyNegativeElem(index) || AnyNegativeElem(strides)){
		LogicError( "Supplied index and strides must be non-negative");
	}
	return LinearIndex_(index, strides);
}

//TODO: Clean up FilterVector with () as filter
inline std::vector<Int> LinearLoc2Loc_(const int linearLoc, const std::vector<int>& shape, const std::vector<int>& permutation)
{
	const int order = shape.size();
	std::vector<Int> ret(order);
    int remainder = linearLoc;
    if(permutation.size() == 0){
        const std::vector<int> strides = Dimensions2Strides(shape);

    	for(int i = order - 1; i >= 0; i--){
    	        const int indexLoc = remainder / strides[i];
    	        ret[i] = indexLoc;
    	        remainder -= indexLoc * strides[i];
        }
    }else{
        const std::vector<int> permutedShape = FilterVector(shape, permutation);
        const std::vector<int> strides = Dimensions2Strides(permutedShape);

		for(int i = order - 1; i >= 0; i--){
			const int indexLoc = remainder / strides[i];
			ret[permutation[i]] = indexLoc;
			remainder -= indexLoc * strides[i];
		}
    }
    return ret;
}

inline std::vector<Int> LinearLoc2Loc(const int linearLoc, const std::vector<int>& shape, const std::vector<int>& permutation)
{
    if(permutation.size() > 0 && shape.size() != permutation.size())
        LogicError("Shape and Permutation orders differ.");
    if(shape.size() == 0 && linearLoc != 0)
        LogicError("Combination of linearIndex=0 and strides incompatible");
    if(linearLoc < 0)
        LogicError( "Linear index must be >= 0");
    return LinearLoc2Loc_(linearLoc, shape, permutation);
}

inline int GridViewLoc2GridLinearLoc(const std::vector<int>& gridViewLoc, const GridView& gridView){
	if(gridViewLoc.size() != gridView.Order())
		LogicError("Supplied loc must be same order as gridView");
	return GridViewLoc2GridLinearLoc_(gridViewLoc, gridView);
}


inline int GridViewLoc2GridLinearLoc_(const std::vector<int>& gridViewLoc, const GridView& gridView){

	const int gvOrder = gridView.Order();
	const TensorDistribution tensorDist = gridView.Distribution();

	const tmen::Grid* grid = gridView.Grid();
	const int gridOrder = grid->Order();
	const std::vector<int> gridShape = grid->Shape();
	Unsigned i, j;

	std::vector<int> gridLoc(gridOrder);
	for(i = 0; i < gvOrder; i++){

		const ModeDistribution modeDist = tensorDist[i];
		const std::vector<int> gridSlice = FilterVector(gridShape, modeDist);
		std::vector<int> sliceLoc = LinearLoc2Loc(gridViewLoc[i], gridSlice);

		for(j = 0; j < sliceLoc.size(); j++){
			gridLoc[modeDist[j]] = sliceLoc[j];
		}
	}
	return LinearIndex(gridLoc, Dimensions2Strides(gridShape));
}

} // namespace tmen

#endif // ifndef TMEN_CORE_INDEXING_IMPL_HPP
