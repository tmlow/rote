/*
   Copyright (c) 2009-2015, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "tensormental.hpp"

#include "./Read/Ascii.hpp"
#include "./Read/AsciiMatlab.hpp"
#include "./Read/Binary.hpp"
#include "./Read/BinaryFlat.hpp"

namespace tmen {

template<typename T>
void Read( Tensor<T>& A, const std::string filename, FileFormat format )
{
    if( format == AUTO )
        format = DetectFormat( filename );

    switch( format )
    {
    case ASCII:
        read::Ascii( A, filename );
        break;
//    case ASCII_MATLAB:
//        read::AsciiMatlab( A, filename );
//        break;
//    case BINARY:
//        read::Binary( A, filename );
//        break;
//    case BINARY_FLAT:
//        read::BinaryFlat( A, A.Shape, filename );
//        break;
    default:
        LogicError("Format unsupported for reading");
    }
}

template<typename T>
inline void
ReadBinarySeqPack(const DistTensor<T>& A, const ObjShape& packShape, const ObjShape& commGridViewShape, const Unsigned& nElemsPerProc, const Location& fileLoc, const ObjShape& gblDataShape, std::ifstream& fileStream, T* sendBuf)
{
    Unsigned order = packShape.size();

    //Info about the data tensor we're reading
    Unsigned gblDataPtr = 0;
    Location gblDataLoc = fileLoc;

    //Info about how much data we have packed that we possibly can pack (memory constraint)
    Unsigned packedPtr = 0;
    Location packedShape(order, 0);

    //Info about which process we should be packing.
    Unsigned procPtr = 0;
    Unsigned whichProc = 0;

    Location procLoc(order, 0);
    std::vector<Unsigned> nElemsPackedPerProc(prod(commGridViewShape), 0);

    bool done = !ElemwiseLessThan(gblDataLoc, gblDataShape);

//    PrintVector(packShape, "packShape");
    while(!done){
//        PrintVector(gblDataLoc, "gblDataLoc");
//        PrintVector(packedShape, "packedShape");
        T value;
        fileStream.read((char*)&value, sizeof(T));
//        printf("new val: %.3f\n", value);
        Location procGVLoc = A.DetermineOwner(gblDataLoc);
//        PrintVector(procGVLoc, "procGVLoc");
        Unsigned whichProc = GridViewLoc2ParticipatingLinearLoc(procGVLoc, A.GetGridView());
//        PrintVector(procGVLoc, "procLoc");
//        printf("copying to proc: %d with offset: %d at location: %d\n", whichProc, whichProc*nElemsPerProc, nElemsPackedPerProc[whichProc]);
        sendBuf[whichProc*nElemsPerProc + nElemsPackedPerProc[whichProc]] = value;
//        PrintArray(sendBuf, packShape, "current sendBuf");

        //Update
        nElemsPackedPerProc[whichProc]++;
        gblDataLoc[gblDataPtr]++;
        packedShape[packedPtr]++;

        //Update the pack counter
        while (packedPtr < packShape.size() && packedShape[packedPtr] >= packShape[packedPtr]) {
            packedShape[gblDataPtr] = 0;

            packedPtr++;
            //packShape[i] can only be either gblDataShape[i] or < gblDataShape[i]
            //if packShape[i] !=0 when gblDataShape[i] == 0
            //we've run off the edge
            if (packedPtr >= packShape.size()) {
                done = true;
                break;
            } else {
                packedShape[packedPtr]++;
            }
        }
        if (done)
            break;

        //Update the counters
        while (gblDataPtr < order && gblDataLoc[gblDataPtr] >= gblDataShape[gblDataPtr]) {
            //packShape[i] can only be either gblDataShape[i] or < gblDataShape[i]
            //if packShape[i] !=0 when resetting gblDataShape[i]
            //we've run off the edge
            if(gblDataPtr < packShape.size() && packedShape[gblDataPtr] != 0){
                done = true;
                break;
            }
            gblDataLoc[gblDataPtr] = 0;

            gblDataPtr++;
            if (gblDataPtr >= order) {
                done = true;
                break;
            } else {
                gblDataLoc[gblDataPtr]++;
            }
        }
        if (done)
            break;
        gblDataPtr = 0;
        packedPtr = 0;
    }
}

template<typename T>
inline void
ReadAsciiSeqPack(const DistTensor<T>& A, const ObjShape& packShape, const ObjShape& commGridViewShape, const Unsigned& nElemsPerProc, const Location& fileLoc, const ObjShape& gblDataShape, std::stringstream& dataStream, T* sendBuf)
{
    Unsigned order = packShape.size();

    //Info about the data tensor we're reading
    Unsigned gblDataPtr = 0;
    Location gblDataLoc = fileLoc;

    //Info about how much data we have packed that we possibly can pack (memory constraint)
    Unsigned packedPtr = 0;
    Location packedShape(order, 0);

    //Info about which process we should be packing.
    Unsigned procPtr = 0;
    Unsigned whichProc = 0;

    Location procLoc(order, 0);
    std::vector<Unsigned> nElemsPackedPerProc(prod(commGridViewShape), 0);

    bool done = !ElemwiseLessThan(gblDataLoc, gblDataShape);

//    PrintVector(packShape, "packShape");
    while(!done){
//        PrintVector(gblDataLoc, "gblDataLoc");
//        PrintVector(packedShape, "packedShape");
        T value;
        dataStream >> value;
//        printf("new val: %.3f\n", value);
        Location procGVLoc = A.DetermineOwner(gblDataLoc);
//        PrintVector(procGVLoc, "procGVLoc");
        Unsigned whichProc = GridViewLoc2ParticipatingLinearLoc(procGVLoc, A.GetGridView());
//        PrintVector(procGVLoc, "procLoc");
//        printf("copying to proc: %d with offset: %d at location: %d\n", whichProc, whichProc*nElemsPerProc, nElemsPackedPerProc[whichProc]);
        sendBuf[whichProc*nElemsPerProc + nElemsPackedPerProc[whichProc]] = value;
//        PrintArray(sendBuf, packShape, "current sendBuf");

        //Update
        nElemsPackedPerProc[whichProc]++;
        gblDataLoc[gblDataPtr]++;
        packedShape[packedPtr]++;

        //Update the pack counter
        while (packedPtr < packShape.size() && packedShape[packedPtr] >= packShape[packedPtr]) {
            packedShape[gblDataPtr] = 0;

            packedPtr++;
            //packShape[i] can only be either gblDataShape[i] or < gblDataShape[i]
            //if packShape[i] !=0 when gblDataShape[i] == 0
            //we've run off the edge
            if (packedPtr >= packShape.size()) {
                done = true;
                break;
            } else {
                packedShape[packedPtr]++;
            }
        }
        if (done)
            break;

        //Update the counters
        while (gblDataPtr < order && gblDataLoc[gblDataPtr] >= gblDataShape[gblDataPtr]) {
            //packShape[i] can only be either gblDataShape[i] or < gblDataShape[i]
            //if packShape[i] !=0 when resetting gblDataShape[i]
            //we've run off the edge
            if(gblDataPtr < packShape.size() && packedShape[gblDataPtr] != 0){
                done = true;
                break;
            }
            gblDataLoc[gblDataPtr] = 0;

            gblDataPtr++;
            if (gblDataPtr >= order) {
                done = true;
                break;
            } else {
                gblDataLoc[gblDataPtr]++;
            }
        }
        if (done)
            break;
        gblDataPtr = 0;
        packedPtr = 0;
    }
}

template<typename T>
inline void
ReadSeqUnpack(DistTensor<T>& A, const Location& firstGblLoc, const ObjShape& packetShape, const T* recvBuf){
    //packetShape.size() always less than A.Order()
    Unsigned order = packetShape.size();
    Location firstLocalLocUnpack = A.Global2LocalIndex(firstGblLoc);
    //TODO:  Respect permutations...
    T* dstBuf = A.Buffer(firstLocalLocUnpack);
    ObjShape localShape = A.LocalShape();

    Unsigned dstBufPtr = 0;
    std::vector<Unsigned> dstBufStrides = A.LocalStrides();
    Unsigned recvBufPtr = 0;
    std::vector<Unsigned> recvBufStrides = Dimensions2Strides(packetShape);

    Unsigned localUnpackPtr = 0;
    Unsigned recvPtr = 0;
    Location localUnpackLoc = firstLocalLocUnpack;
    Location recvLoc(packetShape.size(), 0);

    bool done = !(ElemwiseLessThan(localUnpackLoc, localShape) && ElemwiseLessThan(recvLoc, packetShape));

    //TODO: Detect "nice" strides and use MemCopy
    //NOTE: This is basically a copy of PackCommHelper routine
    //      but modified as the termination condition depends
    //      on both the packShape and the local shape
//    PrintVector(packetShape, "packetShape");
//    PrintVector(localShape, "localShape");
    while (!done) {
        PrintVector(localUnpackLoc, "localLoc");
        PrintVector(recvLoc, "recvLoc");
        dstBuf[dstBufPtr] = recvBuf[recvBufPtr];
        printf("recvBuf val: %.3f\n", recvBuf[recvBufPtr]);
        printf("copying to location: %d\n", dstBufPtr);
        //Update
        localUnpackLoc[localUnpackPtr]++;
        recvLoc[recvPtr]++;

        dstBufPtr += dstBufStrides[0];
        recvBufPtr += recvBufStrides[0];

        while (recvPtr < order && recvLoc[recvPtr] >= packetShape[recvPtr]) {
            recvLoc[recvPtr] = 0;

            recvBufPtr -= recvBufStrides[recvPtr] * packetShape[recvPtr];
            recvPtr++;
            if (recvPtr >= order) {
                done = true;
                break;
            } else {
                recvLoc[recvPtr]++;
                recvBufPtr += recvBufStrides[localUnpackPtr];
            }
        }
        if (done)
            break;
        recvPtr = 0;

        while (localUnpackPtr < order && localUnpackLoc[localUnpackPtr] >= localShape[localUnpackPtr]) {
            localUnpackLoc[localUnpackPtr] = firstLocalLocUnpack[localUnpackPtr];

            dstBufPtr -= dstBufStrides[localUnpackPtr] * localShape[localUnpackPtr];
            localUnpackPtr++;
            if (localUnpackPtr >= order) {
                done = true;
                break;
            } else {
                localUnpackLoc[localUnpackPtr]++;
                dstBufPtr += dstBufStrides[localUnpackPtr];
            }
        }
        if (done)
            break;
        localUnpackPtr = 0;
    }
}

template<typename T>
void
ReadSeq(DistTensor<T>& A, const std::string filename, FileFormat format)
{
    std::ifstream file;
    switch(format){
        case ASCII_MATLAB:
        case ASCII: file.open(filename); break;
        case BINARY_FLAT:
        case BINARY: file.open(filename, std::ios::binary); break;
        default: LogicError("Unsupported distributed read format");
    }
    if( !file.is_open() ){
        std::string msg = "Could not open " + filename;
        RuntimeError(msg);
    }
    //Get the shape of the object we are trying to fill
    ObjShape dataShape;
    if(format == ASCII_MATLAB || format == ASCII){
        std::string line;
        std::getline(file, line);
        std::stringstream dataShapeStream(line);
        Unsigned value;
        while( dataShapeStream >> value ) dataShape.push_back(value);
        A.ResizeTo(dataShape);
    }else if(format == BINARY){
        //Ignore tensor order?
        Unsigned i;
        Unsigned order;
        file.read( (char*)&order, sizeof(Unsigned) );
        dataShape.resize(order);
        Unsigned value;
        for(i = 0; i < order; i++)
            file.read( (char*)&(dataShape[i]), sizeof(Unsigned));
        A.ResizeTo(dataShape);
    }

    Unsigned i;
    Unsigned order = A.Order();
    const tmen::GridView& gvA = A.GetGridView();

    //Determine the max shape we can pack with available memory
    //Figure out the first mode that a packet does not fully pack
    ObjShape packetShape(A.Shape());
    PrintVector(A.Shape(), "shapeA");
    Unsigned firstPartialPackMode = order - 1;

    //MAX_ELEM_PER_PROC must account for entire send buffer size
    Unsigned remainder = MaxLength(MAX_ELEM_PER_PROC, prod(gvA.ParticipatingShape()));
    Unsigned readStride = 1;
    for(i = 0; i < order; i++){
        printf("remainder: %d\n", remainder);
        printf("read stride: %d\n", readStride);
        PrintVector(packetShape, "gblSendShape");

        if(remainder < (A.Dimension(i) * readStride)){
            packetShape[i] = Max(1, remainder);
        }

        if(remainder == 0 && firstPartialPackMode == order - 1){
            firstPartialPackMode = i - 1;
        }

        Unsigned testRemainder = remainder - packetShape[i] * readStride;
        if(testRemainder > remainder)
            remainder = 0;
        else
            remainder = testRemainder;
        readStride *= packetShape[i];
    }

    //Get the subset of processes involved in the communication
    ObjShape gvAShape = gvA.ParticipatingShape();
    Unsigned nCommProcs = prod(gvAShape);

    //Determine the tensor shape we will send to each process
    PrintVector(packetShape, "gblSendShape");
    PrintVector(gvAShape, "commGridViewShape");
    PrintVector(packetShape, "packetShape");
    ObjShape sendShape = MaxLengths(packetShape, gvAShape);
    PrintVector(sendShape, "sendShape");
    Unsigned nElemsPerProc = prod(sendShape);

    T* auxBuf = new T[prod(sendShape) * (nCommProcs + 1)];
//    T* auxBuf = A.auxMemory_.Require(prod(sendShape) * (nCommProcs + 1));
    T* sendBuf = &(auxBuf[prod(sendShape)]);
    T* recvBuf = &(auxBuf[0]);

    //Set up the communicator information, including
    //the permutation from TensorDist proc order -> comm proc order
    ModeArray commModes;
    for(i = 0; i < A.Order(); i++){
        ModeDistribution modeDist = A.ModeDist(i);
        commModes.insert(commModes.end(), modeDist.begin(), modeDist.end());
    }
    ModeArray sortedCommModes = commModes;
    std::sort(sortedCommModes.begin(), sortedCommModes.end());
    mpi::Comm comm = A.GetCommunicatorForModes(sortedCommModes, A.Grid());

    //Perform the read
    Location dataLoc(order, 0);
    Unsigned ptr = firstPartialPackMode;

    Unsigned sendBufPtr = 0;
    std::vector<Unsigned> sendBufStrides = Dimensions2Strides(dataShape);

    //For the case that we're dealing with ASCII, read into a stringstream
    std::stringstream dataStream;
    std::string line;

    if(format == ASCII || format == ASCII_MATLAB){
        std::getline(file, line);
        dataStream.str(line);
    }

    bool done = !ElemwiseLessThan(dataLoc, dataShape);

    Location myFirstGblElemLocUnpack;
    Location firstLocalElemLocUnpack;
    Location myPackGridViewLoc = gvA.ParticipatingLoc();
    PrintVector(myPackGridViewLoc, "myPackGridViewLoc");

    printf("ping2\n");
    while(!done){
        printf("ping2.1\n");
        PrintVector(dataLoc, "dataLoc");
        if(A.Grid().LinearRank() == 0){
            MemZero(&(sendBuf[0]), prod(sendShape) * nCommProcs);
            if(format == ASCII || format == ASCII_MATLAB){
                ReadAsciiSeqPack(A, packetShape, gvAShape, prod(sendShape), dataLoc, dataShape, dataStream, &(sendBuf[0]));
            }else{
                ReadBinarySeqPack(A, packetShape, gvAShape, prod(sendShape), dataLoc, dataShape, file, &(sendBuf[0]));
            }
            ObjShape commShape = sendShape;
            commShape.insert(commShape.end(), nCommProcs);
            PrintArray(sendBuf, commShape, "comm sendBuf");
        }
        printf("ping2.2\n");

        //Communicate the data
        mpi::Scatter(sendBuf, prod(sendShape), recvBuf, prod(sendShape), 0, comm);

        printf("UNPACKING\n");
        PrintArray(recvBuf, sendShape, "recvBuf");

        //Unpack it
        Location packLastLoc = dataLoc;
        for(i = 0; i < firstPartialPackMode + 1; i++)
            packLastLoc[i] += packetShape[i] - 1;

        //Determine the first element I own
        Location owner = A.DetermineOwner(dataLoc);
        myFirstGblElemLocUnpack = ElemwiseSum(dataLoc, ElemwiseSubtract(myPackGridViewLoc, owner));

        PrintVector(packLastLoc, "packLastLoc");
        PrintVector(myFirstGblElemLocUnpack, "firstGblElemLocUnpack");

        if(ElemwiseLessThan(myFirstGblElemLocUnpack, dataShape) && ElemwiseLessThanEqualTo(dataLoc, myFirstGblElemLocUnpack) && !AnyElemwiseGreaterThan(myFirstGblElemLocUnpack, packLastLoc)){
            ReadSeqUnpack(A, myFirstGblElemLocUnpack, sendShape, recvBuf);
            PrintArray(A.Buffer(), A.LocalShape(), "dataBuf");
        }

        //Update
        dataLoc[ptr] += packetShape[ptr];

        while (ptr < order && dataLoc[ptr] >= dataShape[ptr]) {
            dataLoc[ptr] = 0;

            ptr++;
            if (ptr >= order) {
                done = true;
                break;
            } else {
                dataLoc[ptr]++;
            }
        }
        if (done)
            break;
        ptr = firstPartialPackMode;
    }
    delete [] auxBuf;
//    A.auxMemory_.Release();

}

template<typename T>
void
ReadNonSeq(DistTensor<T>& A, const std::string filename, FileFormat format){
    std::ifstream file;
    switch(format){
        case ASCII_MATLAB:
        case ASCII: file.open(filename); break;
        case BINARY_FLAT:
        case BINARY: file.open(filename, std::ios::binary); break;
        default: LogicError("Unsupported distributed read format");
    }
    if( !file.is_open() ){
        std::string msg = "Could not open " + filename;
        RuntimeError(msg);
    }
    //Get the shape of the object we are trying to fill
    ObjShape dataShape;
    if(format == ASCII_MATLAB || format == ASCII){
        std::string line;
        std::getline(file, line);
        std::stringstream dataShapeStream(line);
        Unsigned value;
        while( dataShapeStream >> value ) dataShape.push_back(value);
        A.ResizeTo(dataShape);
    }else if(format == BINARY){
        //Ignore tensor order?
        Unsigned i;
        Unsigned order;
        file.read( (char*)&order, sizeof(Unsigned) );
        dataShape.resize(order);
        Unsigned value;
        for(i = 0; i < order; i++)
            file.read( (char*)&(dataShape[i]), sizeof(Unsigned));
        A.ResizeTo(dataShape);
    }

    Unsigned i;
    Unsigned order = A.Order();
    const tmen::GridView& gvA = A.GetGridView();

    ObjShape shapeA = A.Shape();
    Location myLoc = gvA.ParticipatingLoc();
    ObjShape gvAShape = gvA.ParticipatingShape();
    std::vector<Unsigned> readInc = gvAShape;
    std::vector<Unsigned> loopSpace = ElemwiseSubtract(shapeA, myLoc);
    std::vector<Unsigned> loopIters = MaxLengths(loopSpace, gvAShape);

    Unsigned startLinLoc = Loc2LinearLoc(myLoc, shapeA);
    std::vector<Unsigned> srcBufStrides = Dimensions2Strides(A.Shape());
    std::vector<Unsigned> dstBufStrides = A.LocalStrides();
    T* dstBuf = A.Buffer();

    Unsigned startPtr;
    std::string line;
    std::stringstream dataStream;
    if(format == ASCII_MATLAB || format == ASCII){
        std::getline(file, line);
        dataStream.str(line);
        T value;
        for(i = 0; i < startLinLoc; i++)
            dataStream >> value;
    }else if(format == BINARY){
        file.seekg( 1 + order + (startLinLoc * sizeof(T)));
    }else if(format == BINARY_FLAT){
        file.seekg( startLinLoc * sizeof(T));
    }

    Unsigned ptr = 0;
    Unsigned dstBufPtr = 0;

    Location curLoc = myLoc;
    Unsigned srcBufPtr = startLinLoc;
    Unsigned newSrcBufPtr = 0;

    bool done = !ElemwiseLessThan(curLoc, A.Shape());

//    printf("order: %d\n", order);
//    PrintVector(myLoc, "myLoc");
//    PrintVector(readInc, "readInc");
//    PrintVector(loopIters, "loopIters");
//    PrintVector(srcBufStrides, "srcBufStrides"); 
//    PrintVector(dstBufStrides, "dstBufStrides");
    while(!done){
        T value;
        if(format == ASCII_MATLAB || format == ASCII){
            dataStream >> value;
        }else{
            file.read((char*)&value, sizeof(T));
        }
//        PrintVector(curLoc, "curLoc");
//        printf("read val: %.3f\n", value);
        dstBuf[dstBufPtr] = value;

        //Update
        curLoc[ptr] += readInc[ptr];
        dstBufPtr += dstBufStrides[ptr];
        newSrcBufPtr = srcBufPtr;
        newSrcBufPtr += srcBufStrides[ptr] * readInc[ptr];

        while (ptr < order && curLoc[ptr] >= shapeA[ptr]) {
//            PrintVector(curLoc, "curLoc before reset");
            curLoc[ptr] = myLoc[ptr];
//            PrintVector(curLoc, "curLoc after reset");
//            printf("newSrcBufPtr before reset: %d\n", newSrcBufPtr);
            newSrcBufPtr -= srcBufStrides[ptr] * readInc[ptr] * loopIters[ptr];
//            printf("newSrcBufPtr after reset: %d\n", newSrcBufPtr);
            ptr++;

            if (ptr >= order) {
                done = true;
                break;
            } else {
                curLoc[ptr] += readInc[ptr];
//                PrintVector(curLoc, "curLoc after inc");
                newSrcBufPtr += srcBufStrides[ptr] * readInc[ptr];
//                printf("newSrcBufPtr after inc: %d\n", newSrcBufPtr);
            }
        }
        if (done)
            break;
        ptr = 0;
        //Adjust streams (we read in 1 value, so adjust by -1 )
        Unsigned adjustAmount = newSrcBufPtr - srcBufPtr - 1;
//        printf("newSrcBufPtr: %d, oldSrcBufPtr: %d\n", newSrcBufPtr, srcBufPtr);
        if(format == ASCII_MATLAB || format == ASCII){
            T val;
            for(i = 0; i < adjustAmount; i++)
                dataStream >> val;
        }else if(format == BINARY){
            file.seekg(1 + order + (newSrcBufPtr * sizeof(T)));
        }else if(format == BINARY_FLAT){
            file.seekg((newSrcBufPtr * sizeof(T)));
        }
        srcBufPtr = newSrcBufPtr;
    }

}

template<typename T>
void Read
( DistTensor<T>& A, const std::string filename, FileFormat format,
  bool sequential )
{
    if( format == AUTO )
        format = DetectFormat( filename ); 

    //Everyone accesses data
    if(!sequential)
    {
        ReadNonSeq(A, filename, format);
//        switch( format )
//        {
//        case ASCII:
//            read::Ascii( A, filename );
//            break;
////        case ASCII_MATLAB:
////            read::AsciiMatlab( A, filename );
////            break;
////        case BINARY:
////            read::Binary( A, filename );
////            break;
////        case BINARY_FLAT:
////            read::BinaryFlat( A, A.Height(), A.Width(), filename );
//            break;
//        default:
//            LogicError("Unsupported distributed read format");
//        }
    }
    //Only root process accesses data
    else
    {
        ReadSeq( A, filename, format );
//        switch( format )
//        {
//        case ASCII:
//            ReadSeq( A, filename, format );
//            break;
////        case ASCII_MATLAB:
////            read::AsciiMatlabSeq( A, filename );
////            break;
////        case BINARY:
////            read::BinarySeq( A, filename );
////            break;
////        case BINARY_FLAT:
////            read::BinaryFlatSeq( A, A.Height(), A.Width(), filename );
////            break;
//        default:
//            LogicError("Unsupported distributed read format");
//        }
    }

}

#define PROTO(T) \
  template void Read \
  ( Tensor<T>& A, const std::string filename, FileFormat format ); \
  template void Read \
  ( DistTensor<T>& A, const std::string filename, \
    FileFormat format, bool sequential );

#define FULL(T) \
  PROTO(T);

FULL(Int);
#ifndef DISABLE_FLOAT
FULL(float);
#endif
FULL(double);


} // namespace tmen
