/*
   Copyright (c) 2009-2013, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "tensormental.hpp"

namespace tmen {

template<typename T>
void
DistTensor<T>::ComplainIfReal() const
{
    if( !IsComplex<T>::val )
        LogicError("Called complex-only routine with real data");
}

template<typename T>
Location
DistTensor<T>::DetermineOwner(const Location& loc) const
{
#ifndef RELEASE
    CallStackEntry entry("DistTensor::DetermineOwner");
    this->AssertValidEntry( loc );
#endif
    const tmen::GridView gv = GetGridView();
    Location ownerLoc(gv.ParticipatingOrder());

    for(Int i = 0; i < gv.ParticipatingOrder(); i++){
        ownerLoc[i] = (loc[i] + this->ModeAlignment(i)) % this->ModeStride(i);
    }
    return ownerLoc;
}

template<typename T>
Location
DistTensor<T>::Global2LocalIndex(const Location& globalLoc) const
{
#ifndef RELEASE
    CallStackEntry entry("DistTensor::Global2LocalIndex");
    this->AssertValidEntry( globalLoc );
#endif
    Unsigned i;
    Location localLoc(globalLoc.size());
    for(i = 0; i < globalLoc.size(); i++){
        localLoc[i] = (globalLoc[i]-this->ModeShift(i)) / this->ModeStride(i);
    }
    return localLoc;
}

//TODO: Differentiate between index and mode
template<typename T>
mpi::Comm
DistTensor<T>::GetCommunicator(Mode mode) const
{
    mpi::Comm comm;
    ObjShape gridViewSliceShape = this->GridViewShape();
    Location gridViewSliceLoc = this->GridViewLoc();
    const Unsigned commKey = gridViewSliceLoc[mode];

    //Color is defined by the linear index into the logical grid EXCLUDING the index being distributed
    gridViewSliceShape.erase(gridViewSliceShape.begin() + mode);
    gridViewSliceLoc.erase(gridViewSliceLoc.begin() + mode);
    const Unsigned commColor = Loc2LinearLoc(gridViewSliceLoc, gridViewSliceShape);

    mpi::CommSplit(participatingComm_, commColor, commKey, comm);
    return comm;
}

template<typename T>
mpi::Comm
DistTensor<T>::GetCommunicatorForModes(const ModeArray& commModes, const tmen::Grid& grid)
{
    ModeArray sortedCommModes = commModes;
    std::sort(sortedCommModes.begin(), sortedCommModes.end());
//    return grid_->GetCommunicatorForModes(commModes);
//    mpi::Comm comm;
//    const Location gridLoc = grid_->Loc();
//    const ObjShape gridShape = grid_->Shape();
//
//    ObjShape gridSliceShape = FilterVector(gridShape, commModes);
//    ObjShape gridSliceNegShape = NegFilterVector(gridShape, commModes);
//    Location gridSliceLoc = FilterVector(gridLoc, commModes);
//    Location gridSliceNegLoc = NegFilterVector(gridLoc, commModes);
//
//    const Unsigned commKey = Loc2LinearLoc(gridSliceLoc, gridSliceShape);
//    const Unsigned commColor = Loc2LinearLoc(gridSliceNegLoc, gridSliceNegShape);
//
//    mpi::CommSplit(participatingComm_, commColor, commKey, comm);
    if(commMap_->count(sortedCommModes) == 0){
        mpi::Comm comm;
        const Location gridLoc = grid.Loc();
        const ObjShape gridShape = grid.Shape();

        ObjShape gridSliceShape = FilterVector(gridShape, sortedCommModes);
        ObjShape gridSliceNegShape = NegFilterVector(gridShape, sortedCommModes);
        Location gridSliceLoc = FilterVector(gridLoc, sortedCommModes);
        Location gridSliceNegLoc = NegFilterVector(gridLoc, sortedCommModes);

//        PrintVector(gridSliceShape, "gridSliceShape");
//        PrintVector(gridSliceNegShape, "gridSliceNegShape");
//        PrintVector(gridSliceLoc, "gridSliceLoc");
//        PrintVector(gridSliceNegLoc, "gridSliceNegLoc");
        const Unsigned commKey = Loc2LinearLoc(gridSliceLoc, gridSliceShape);
        const Unsigned commColor = Loc2LinearLoc(gridSliceNegLoc, gridSliceNegShape);

        //Check this, original was commented line with participating
        mpi::CommSplit(grid.OwningComm(), commColor, commKey, comm);
//        std::cout << "made size " << mpi::CommSize(comm) << " comm\n";
        (*commMap_)[sortedCommModes] = comm;

    }
    return (*commMap_)[sortedCommModes];
}

template<typename T>
void
DistTensor<T>::SetParticipatingComm()
{
#ifndef RELEASE
    CallStackEntry cse("DistTensor::GetParticipatingComm");
#endif
    ModeArray commModes = ConcatenateVectors(gridView_.FreeModes(), gridView_.BoundModes());
    std::sort(commModes.begin(), commModes.end());

    mpi::Comm comm;
    const Location gridLoc = grid_->Loc();
    const ObjShape gridShape = grid_->Shape();

    ObjShape gridSliceShape = FilterVector(gridShape, commModes);
    ObjShape gridSliceNegShape = NegFilterVector(gridShape, commModes);
    Location gridSliceLoc = FilterVector(gridLoc, commModes);
    Location gridSliceNegLoc = NegFilterVector(gridLoc, commModes);

    const Unsigned commKey = Loc2LinearLoc(gridSliceLoc, gridSliceShape);
    const Unsigned commColor = Loc2LinearLoc(gridSliceNegLoc, gridSliceNegShape);

    mpi::CommSplit(Grid().OwningComm(), commColor, commKey, comm);
    participatingComm_ = comm;
}

template<typename T>
void
DistTensor<T>::CopyLocalBuffer(const DistTensor<T>& A)
{
#ifndef RELEASE
    CallStackEntry cse("DistTensor::CopyBuffer");
#endif
    tensor_.CopyBuffer(A.LockedTensor());
}


template<typename T>
void DistTensor<T>::PackCommHelper(const PackData& packData, const Mode packMode, T const * const srcBuf, T * const dstBuf){
    Unsigned packSlice;
    const Unsigned loopEnd = packData.loopShape[packMode];
    const Unsigned dstBufStride = packData.dstBufStrides[packMode];
    const Unsigned srcBufStride = packData.srcBufStrides[packMode];
    const Unsigned loopStart = packData.loopStarts[packMode];
    const Unsigned loopInc = packData.loopIncs[packMode];
    Unsigned dstBufPtr = 0;
    Unsigned srcBufPtr = 0;

    if(packMode == 0){
        if(dstBufStride == 1 && srcBufStride == 1){
            MemCopy(&(dstBuf[0]), &(srcBuf[0]), loopEnd);
        }else{
            for(packSlice = loopStart; packSlice < loopEnd; packSlice += loopInc){
                dstBuf[dstBufPtr] = srcBuf[srcBufPtr];
                dstBufPtr += dstBufStride;
                srcBufPtr += srcBufStride;
            }
        }
    }else{
        for(packSlice = loopStart; packSlice < loopEnd; packSlice += loopInc){
            PackCommHelper(packData, packMode-1, &(srcBuf[srcBufPtr]), &(dstBuf[dstBufPtr]));
            dstBufPtr += dstBufStride;
            srcBufPtr += srcBufStride;
        }
    }
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
FULL(Complex<float>);
#endif
FULL(Complex<double>);
#endif 


} // namespace tmen
