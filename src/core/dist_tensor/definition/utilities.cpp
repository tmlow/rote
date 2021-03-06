/*
   Copyright (c) 2009-2013, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "rote.hpp"
namespace rote {

template<typename T>
void
DistTensorBase<T>::ComplainIfReal() const
{
    if( !IsComplex<T>::val )
        LogicError("Called complex-only routine with real data");
}

//TODO: FIX THIS CHECK
template<typename T>
Location
DistTensorBase<T>::DetermineOwner(const Location& loc) const
{
    Unsigned i;
    const rote::GridView gv = GetGridView();
    Location ownerLoc = Alignments();

    for(i = 0; i < gv.ParticipatingOrder(); i++){
        ownerLoc[i] = (loc[i] + ModeAlignment(i)) % ModeStride(i);
    }
    return ownerLoc;
}

template<typename T>
Location
DistTensorBase<T>::DetermineOwnerNewAlignment(const Location& loc, std::vector<Unsigned>& newAlignment) const
{
    Unsigned i;
    const rote::GridView gv = GetGridView();
    Location ownerLoc = Alignments();

    for(i = 0; i < gv.ParticipatingOrder(); i++){
        ownerLoc[i] = (loc[i] + newAlignment[i]) % ModeStride(i);
    }
    return ownerLoc;
}

//TODO: Change Global2LocalIndex to incorporate localPerm_ info
template<typename T>
Location
DistTensorBase<T>::Global2LocalIndex(const Location& globalLoc) const
{
#ifndef RELEASE
    AssertValidEntry( globalLoc );
#endif
    Unsigned i;
    Location localLoc(globalLoc.size());
    for(i = 0; i < globalLoc.size(); i++){
        localLoc[i] = (globalLoc[i]-ModeShift(i) + ModeAlignment(i)) / ModeStride(i);
    }
    return localLoc;
}

template<typename T>
mpi::Comm
DistTensorBase<T>::GetCommunicatorForModes(const ModeArray& commModes, const rote::Grid& grid)
{
    ModeArray sortedCommModes = commModes;
    SortVector(sortedCommModes);

    if(commMap_->count(sortedCommModes) == 0){
        mpi::Comm comm;
        const Location gridLoc = grid.Loc();
        const ObjShape gridShape = grid.Shape();

        //Determine which communicator subgroup I belong to
        ObjShape gridSliceNegShape = NegFilterVector(gridShape, sortedCommModes);
        Location gridSliceNegLoc = NegFilterVector(gridLoc, sortedCommModes);

        //Determine my rank within the communicator subgroup I belong to
        ObjShape gridSliceShape = FilterVector(gridShape, sortedCommModes);
        Location gridSliceLoc = FilterVector(gridLoc, sortedCommModes);

        //Set the comm key and color for splitting
        const Unsigned commKey = Loc2LinearLoc(gridSliceLoc, gridSliceShape);
        const Unsigned commColor = Loc2LinearLoc(gridSliceNegLoc, gridSliceNegShape);

        mpi::CommSplit(grid.OwningComm(), commColor, commKey, comm);
        (*commMap_)[sortedCommModes] = comm;
    }
    return (*commMap_)[sortedCommModes];
}

template<typename T>
void
DistTensorBase<T>::SetParticipatingComm()
{
    ModeArray commModes = gridView_.UsedModes();
    SortVector(commModes);

    const rote::Grid& grid = Grid();
    participatingComm_ = GetCommunicatorForModes(commModes, grid);
}

template<typename T>
void
DistTensorBase<T>::CopyLocalBuffer(const DistTensorBase<T>& A)
{
    tensor_.CopyBuffer(A.LockedTensor(), A.localPerm_, localPerm_);
}

template<typename T>
void
DistTensorBase<T>::ClearCommMap()
{
    rote::mpi::CommMap::iterator it;
    for(it = commMap_->begin(); it != commMap_->end(); it++){
        mpi::CommFree(it->second);
    }
    commMap_->clear();
}

template<typename T>
Unsigned
DistTensorBase<T>::CommMapSize()
{
    return commMap_->size();
}

template<typename T>
Location
DistTensorBase<T>::DetermineFirstElem(const Location& gridViewLoc) const
{
    Unsigned i;

    const GridView gv = GetGridView();
    const ObjShape participatingShape = gv.ParticipatingShape();
    Location ret(gridViewLoc.size());
    for(i = 0; i < gridViewLoc.size(); i++){
        ret[i] = gridViewLoc[i] - modeAlignments_[i];

        if(gridViewLoc[i] < modeAlignments_[i])
            ret[i] += ModeStride(i);
    }
//    Location ret(gridViewLoc.size());
//    for(i = 0; i < gridViewLoc.size(); i++){
//        ret[i] = Shift(gridViewLoc[i], modeAlignments_[i], ModeStride(i));
//    }

    return ret;
}

template<typename T>
Location
DistTensorBase<T>::DetermineFirstUnalignedElem(const Location& gridViewLoc, const std::vector<Unsigned>& alignmentDiff) const
{
    Unsigned i;
    Location ret(gridViewLoc.size());
    for(i = 0; i < gridViewLoc.size(); i++){
        ret[i] = Shift(gridViewLoc[i], alignmentDiff[i], ModeStride(i));
    }
    return ret;
}

template<typename T>
bool
DistTensor<T>::AlignCommBufRedist(const DistTensor<T>& A, const T* unalignedSendBuf, const Unsigned sendSize, T* alignedSendBuf, const Unsigned recvSize)
{
    const rote::Grid& g = this->Grid();
    GridView gvA = A.GetGridView();
    GridView gvB = this->GetGridView();

    Location firstOwnerA = gvA.ToGridLoc(A.Alignments());
    Location firstOwnerB = gvB.ToGridLoc(this->Alignments());

    if(!AnyElemwiseNotEqual(firstOwnerA, firstOwnerB))
    	return false;

    std::vector<Unsigned> alignA = A.Alignments();
    std::vector<Unsigned> alignB = this->Alignments();

    std::vector<Unsigned> alignBinA = g.ToParticipatingGridViewLoc(firstOwnerB, gvA);

    Location alignedFirstOwnerA = g.ToGridViewLoc(firstOwnerB, gvA);
    Location myFirstElemLocA = A.DetermineFirstElem(gvA.ParticipatingLoc());
    Location myFirstElemLocAligned = A.DetermineFirstUnalignedElem(gvA.ParticipatingLoc(), alignBinA);

    Location sendGridLoc = gvA.ToGridLoc(A.DetermineOwnerNewAlignment(myFirstElemLocA, alignBinA));
    Location recvGridLoc = gvA.ToGridLoc(A.DetermineOwner(myFirstElemLocAligned));

    //Create the communicator to involve all processes we need to fix misalignment
    ModeDistribution misalignedModesDist;
    for(Unsigned i = 0; i < alignA.size(); i++){
        if(alignBinA[i] != alignA[i]){
            misalignedModesDist += A.ModeDist(i);
        }
    }
    ModeArray misalignedModes = misalignedModesDist.Entries();
    SortVector(misalignedModes);

    mpi::Comm sendRecvComm = this->GetCommunicatorForModes(misalignedModes, g);

    Location sendSliceLoc = FilterVector(sendGridLoc, misalignedModes);
    Location recvSliceLoc = FilterVector(recvGridLoc, misalignedModes);
    ObjShape gridSliceShape = FilterVector(g.Shape(), misalignedModes);

    Unsigned sendLinLoc = Loc2LinearLoc(sendSliceLoc, gridSliceShape);
    Unsigned recvLinLoc = Loc2LinearLoc(recvSliceLoc, gridSliceShape);

    mpi::SendRecv(unalignedSendBuf, sendSize, sendLinLoc,
                  alignedSendBuf, recvSize, recvLinLoc, sendRecvComm);
    return true;
}

template<typename T>
ModeArray
DistTensorBase<T>::GetMisalignedModes(const DistTensor<T>& B) const {
	// DEBUG
	if (this->Order() != B.Order()) {
		LogicError("Tensors have different order");
	}
	// END DEBUG

	std::vector<Unsigned> alignA = this->Alignments();
	std::vector<Unsigned> alignB = B.Alignments();

	ModeArray misalignedModes;
	for(Unsigned i = 0; i < alignA.size(); i++) {
		if(alignA[i] != alignB[i]) {
			ModeArray modeDistEntries = this->ModeDist(i).Entries();
			for(Unsigned j = 0; j < modeDistEntries.size(); j++) {
				if (!Contains(misalignedModes, modeDistEntries[j])) {
					misalignedModes.push_back(modeDistEntries[j]);
				}
			}
		}
	}
	return misalignedModes;
}

#define PROTOBASE(T) template class DistTensorBase<T>
#define COPYBASE(T) \
  template DistTensorBase<T>::DistTensorBase( const DistTensorBase<T>& A )
#define FULLBASE(T) \
  PROTOBASE(T);

#define PROTO(T) template class DistTensor<T>
#define COPY(T) \
  template DistTensor<T>::DistTensor( const DistTensor<T>& A )
#define FULL(T) \
  PROTO(T);

FULLBASE(Int)
#ifndef DISABLE_FLOAT
FULLBASE(float)
#endif
FULLBASE(double)

#ifndef DISABLE_COMPLEX
#ifndef DISABLE_FLOAT
FULLBASE(std::complex<float>)
#endif
FULLBASE(std::complex<double>)
#endif

FULL(Int)
#ifndef DISABLE_FLOAT
FULL(float)
#endif
FULL(double)

#ifndef DISABLE_COMPLEX
#ifndef DISABLE_FLOAT
FULL(std::complex<float>)
#endif
FULL(std::complex<double>)
#endif


} // namespace rote
