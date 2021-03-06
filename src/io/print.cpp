/*
   Copyright (c) 2009-2015, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "rote.hpp"

namespace rote {

template<typename T>
void
Print( const Tensor<T>& A, std::string title, bool all )
{
    std::ostream& os = std::cout;
    if( title != "" )
      os << title << " ";

    const Unsigned order = A.Order();
    Location curLoc(order, 0);

    Unsigned ptr = 0;
    if(order == 0) {
      os.precision(16);
      if (all || mpi::CommRank(MPI_COMM_WORLD) == 0) {
        os << A.Get(curLoc) << " " << std::endl;
      }
      return;
    }
    bool done = order > 0 && !ElemwiseLessThan(curLoc, A.Shape());

    while(!done){
      os.precision(16);
      T val = A.Get(curLoc);
      if (all || mpi::CommRank(MPI_COMM_WORLD) == 0) {
        os << val << " ";
      }
      if(order == 0)
          break;

    	//Update
    	curLoc[ptr]++;
    	while(ptr < order && curLoc[ptr] == A.Dimension(ptr)){
    		curLoc[ptr] = 0;
    		ptr++;
    		if(ptr >= order){
    			done = true;
    			break;
    		}else{
    			curLoc[ptr]++;
    		}
    	}
    	if(done)
    		break;
    	ptr = 0;
    }
    os << std::endl;
}


template<typename T>
void
PrintVector
( const std::vector<T>& vec, std::string title, bool all){
  if (all || mpi::CommRank(MPI_COMM_WORLD) == 0) {
    std::ostream& os = std::cout;
    os << title << ":";

    Unsigned i;
    for(i = 0; i < vec.size(); i++)
      os << " " << vec[i];
    os << std::endl;
  }
}

template<typename T>
void
Print
( const DistTensor<T>& A, std::string title)
{
    std::ostream& os = std::cout;
    if( A.Grid().LinearRank() == 0 && title != "" )
        os << title << std::endl;

    const Unsigned order = A.Order();
    Location curLoc(order, 0);

    if(A.Grid().LinearRank() == 0 && order == 0){
        os.precision(16);
        os << A.Get(curLoc) << " " << std::endl;
        return;
    }

    Unsigned ptr = 0;
    bool done = order > 0 && !ElemwiseLessThan(curLoc, A.Shape());
    T u = T(0);
    while(!done){
        u = A.Get(curLoc);

        if(A.Grid().LinearRank() == 0){
            os.precision(16);
            os << u << " ";
        }

        if(order == 0)
            break;
        //Update
        curLoc[ptr]++;
        while(ptr < order && curLoc[ptr] == A.Dimension(ptr)){
            curLoc[ptr] = 0;
            ptr++;
            if(ptr >= order){
                done = true;
                break;
            }else{
                curLoc[ptr]++;
            }
        }
        if(done)
            break;
        ptr = 0;
    }
    if(A.Grid().LinearRank() == 0){
        os << std::endl;
    }
}

template<typename T>
void
PrintData
( const Tensor<T>& A, std::string title, bool all){
    std::ostream& os = std::cout;
    os << title << std::endl;
    PrintVector(A.Shape(), "    shape", all);
    PrintVector(A.Strides(), "    strides", all);
}

template<typename T>
void
PrintData
( const DistTensor<T>& A, std::string title, bool all){
      std::ostream& os = std::cout;
//    if( A.Grid().LinearRank() == 0 && title != "" ){
        os << title << std::endl;

        PrintVector(A.Shape(), "    shape", all);
        os << "    Distribution: " << A.TensorDist() << std::endl;
        PrintVector(A.Alignments(), "    alignments", all);
        PrintVector(A.ModeShifts(), "    shifts", all);
        PrintVector(A.LocalPermutation().Entries(), "    permutation", all);
        PrintData(A.LockedTensor(), "    tensor data", all);
//    }
}

template<typename T>
void
PrintArray
( const T* dataBuf, const ObjShape& shape, const ObjShape strides, std::string title){
    std::ostream& os = std::cout;
    Unsigned order = shape.size();
    Location curLoc(order, 0);
    Unsigned linLoc = 0;
    Unsigned ptr = 0;

    os << title << ":";

    if(order == 0){
        return;
    }

    bool done = !ElemwiseLessThan(curLoc, shape);

    while(!done){
        os.precision(16);

        os << " " << dataBuf[linLoc];

        //Update
        curLoc[ptr]++;
        linLoc += strides[ptr];
        while(ptr < order && curLoc[ptr] >= shape[ptr]){
            curLoc[ptr] = 0;

            linLoc -= strides[ptr] * (shape[ptr]);
            ptr++;
            if(ptr >= order){
                done = true;
                break;
            }else{
                curLoc[ptr]++;
                linLoc += strides[ptr];
            }
        }
        if(done)
            break;
        ptr = 0;
    }
    os << std::endl;
}

template<typename T>
void
PrintArray
( const T* dataBuf, const ObjShape& loopShape, std::string title){
    PrintArray(dataBuf, loopShape, Dimensions2Strides(loopShape), title);
}

void
PrintPackData
( const PackData& packData, std::string title){
    std::ostream& os = std::cout;
    os << title << std::endl;
    PrintVector(packData.loopShape, "  loopShape");
    PrintVector(packData.srcBufStrides, "  srcBufStrides");
    PrintVector(packData.dstBufStrides, "  dstBufStrides");
}

void
PrintElemScalData
( const ElemScalData& elemScalData, std::string title, bool all){
    std::ostream& os = std::cout;
    os << title << std::endl;
    PrintVector(elemScalData.loopShape, "  loopShape", all);
    PrintVector(elemScalData.src1Strides, "  src1Strides", all);
    PrintVector(elemScalData.src2Strides, "  src2Strides", all);
    PrintVector(elemScalData.dstStrides, "  dstStrides", all);
}

void
PrintHadamardStatCData
( const BlkHadamardStatCInfo& hadamardInfo, std::string title, bool all){
    std::ostream& os = std::cout;
    os << title << std::endl;

    PrintVector(hadamardInfo.blkSizes, "  blkSizes", all);
    PrintVector(hadamardInfo.partModesACA, "  partModesACA", all);
    PrintVector(hadamardInfo.partModesACC, "  partModesACC", all);
    PrintVector(hadamardInfo.partModesBCB, "  partModesBCB", all);
    PrintVector(hadamardInfo.partModesBCC, "  partModesBCC", all);
    // std::cout << "  distIntA: " << hadamardInfo.distIntA << std::endl;
    PrintVector(hadamardInfo.permA.Entries(), "  permA", all);
    // std::cout << "  distIntB: " << hadamardInfo.distIntB << std::endl;
    PrintVector(hadamardInfo.permB.Entries(), "  permB", all);
    // std::cout << "  distIntC: " << hadamardInfo.distIntC << std::endl;
    PrintVector(hadamardInfo.permC.Entries(), "  permC", all);
}

void
PrintHadamardScalData
( const HadamardScalData& hadamardInfo, std::string title, bool all){
    std::ostream& os = std::cout;
    os << title << std::endl;

    PrintVector(hadamardInfo.loopShapeAC, "  loopShapeAC", all);
    PrintVector(hadamardInfo.stridesACA, "  stridesACA", all);
    PrintVector(hadamardInfo.stridesACC, "  stridesACC", all);

    PrintVector(hadamardInfo.loopShapeBC, "  loopShapeBC", all);
    PrintVector(hadamardInfo.stridesBCB, "  stridesBCB", all);
    PrintVector(hadamardInfo.stridesBCC, "  stridesBCC", all);

    PrintVector(hadamardInfo.loopShapeABC, "  loopShapeABC", all);
    PrintVector(hadamardInfo.stridesABCA, "  stridesABCA", all);
    PrintVector(hadamardInfo.stridesABCB, "  stridesABCB", all);
    PrintVector(hadamardInfo.stridesABCC, "  stridesABCC", all);
}

void
PrintRedistPlan
( const RedistPlan& redistPlan, std::string title) {
  if (mpi::CommRank(MPI_COMM_WORLD) != 0) {
    return;
  }
  std::cout << title << std::endl;
  for(Unsigned i = 0; i < redistPlan.size(); i++){
   const Redist redist = redistPlan[i];
   switch(redist.type()){
     case AG:    std::cout << "AG: "; break;
     case A2A:   std::cout << "A2A: "; break;
     case Perm:  std::cout << "Perm: "; break;
     case Local: std::cout << "Local: "; break;
     case RS:    std::cout << "RS: "; break;
     case GTO:   std::cout << "GTO: "; break;
     case RTO:   std::cout << "RTO: "; break;
     case AR:    std::cout << "AR: "; break;
     case BCast:    std::cout << "BCast: "; break;
     case Scatter:    std::cout << "Scatter: "; break;
   }
   std::cout << redist.dB() << " <-- " << redist.dA() << std::endl;
  }
  std::cout << std::endl;
}

void
PrintRedistPlanInfo
( const RedistPlanInfo& redistPlanInfo, std::string title, bool all) {
  if (mpi::CommRank(MPI_COMM_WORLD) != 0) {
    return;
  }
  std::ostream& os = std::cout;
  os << title << std::endl;
  PrintVector(redistPlanInfo.reduceModes(), "Tensor modes reduced");
  PrintVector(redistPlanInfo.reduceGModes(), "Grid modes reduced");
  std::cout << "Added\n";
  for (auto const& kv: redistPlanInfo.added()) {
    std::cout << "  " << kv.first << ": " << kv.second << "\n";
  }
  std::cout << "Removed\n";
  for (auto const& kv: redistPlanInfo.removed()) {
    std::cout << "  " << kv.first << ": " << kv.second << "\n";
  }
  std::cout << "Moved\n";
  for (auto const& kv: redistPlanInfo.moved()) {
    std::cout << "  " << kv.first << ": (" << kv.second.first << ", " << kv.second.second << ")\n";
  }
}

#define FULL(T) \
  template void Print \
  ( const Tensor<T>& A, std::string title, bool all ); \
  template void PrintVector \
  ( const std::vector<T>& vec, std::string title, bool all); \
  template void Print \
  ( const DistTensor<T>& A, std::string title); \
  template void PrintData \
  ( const Tensor<T>& A, std::string title, bool all); \
  template void PrintData \
  ( const DistTensor<T>& A, std::string title, bool all); \
  template void PrintArray \
  ( const T* dataBuf, const ObjShape& shape, const ObjShape strides, std::string title); \
  template void PrintArray \
  ( const T* dataBuf, const ObjShape& shape, std::string title);

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

template void PrintVector
( const std::vector<char>& vec, std::string title, bool all);

} // namespace rote
