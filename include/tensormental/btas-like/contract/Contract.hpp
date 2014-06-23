/*
   Copyright (c) 2009-2013, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License,
   which can be found in the LICENSE file in the root directory, or at
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef TMEN_BTAS_CONTRACT_HPP
#define TMEN_BTAS_CONTRACT_HPP

#include "tensormental/util/vec_util.hpp"
#include "tensormental/util/btas_util.hpp"
#include "tensormental/core/view_decl.hpp"

namespace tmen{

////////////////////////////////////
// Local routines
////////////////////////////////////

template <typename T>
void LocalContract(T alpha, const Tensor<T>& A, const Tensor<T>& B, T beta, Tensor<T>& C, const std::vector<IndexArray>& indices){
#ifndef RELEASE
    CallStackEntry("LocalContract");
    if(indices.size() != 3)
        LogicError("LocalContract: indices vector must be of length 3 (A indices, B indices, C indices)");

    IndexArray indA = indices[0];
    IndexArray indB = indices[1];
    IndexArray indC = indices[2];

    if(indA.size() != A.Order() || indB.size() != B.Order() || indC.size() != C.Order())
        LogicError("LocalContract: number of indices assigned to each tensor must be of same order");

    //Check conformal modes
    for(Unsigned i = 0; i < indA.size(); i++){
        Index index = indA[i];
        //Check A,B
        for(Unsigned j = 0; j < indB.size(); j++){
            if(indB[j] == index){
                if(A.Dimension(i) != B.Dimension(j))
                    LogicError("LocalContract: Modes assigned same indices must be conformal");
            }
        }
        //Check A,C
        for(Unsigned j = 0; j < indC.size(); j++){
            if(indC[j] == index){
                if(A.Dimension(i) != C.Dimension(j))
                    LogicError("LocalContract: Modes assigned same indices must be conformal");
            }
        }
    }
    for(Unsigned i = 0; i < indB.size(); i++){
        Index index = indB[i];
        //Check B,C
        for(Unsigned j = 0; j < indC.size(); j++){
            if(indC[j] == index){
                if(B.Dimension(i) != C.Dimension(j))
                    LogicError("LocalContract: Modes assigned same indices must be conformal");
            }
        }
    }
#endif
    Unsigned i, j;
    const std::vector<ModeArray> contractModes(DetermineContractModes(A, B, C, indices));

    //TODO: Implement invPerm routine to get the invPermC variable (only reason I form CModes)
    ModeArray CModes(C.Order());
    for(i = 0; i < C.Order(); i++)
        CModes[i] = i;

    //Determine the permutations for each Tensor
    const std::vector<Unsigned> permA(ConcatenateVectors(contractModes[0], contractModes[1]));
    const std::vector<Unsigned> permB(ConcatenateVectors(contractModes[1], contractModes[2]));
    const std::vector<Unsigned> permC(ConcatenateVectors(contractModes[0], contractModes[2]));
    const std::vector<Unsigned> invPermC(DeterminePermutation(ConcatenateVectors(contractModes[0], contractModes[2]), CModes));

    Tensor<T> PA(FilterVector(A.Shape(), permA));
    Tensor<T> PB(FilterVector(B.Shape(), permB));
    Tensor<T> PC(FilterVector(C.Shape(), permC));
    Tensor<T> MPA, MPB, MPC;

    //Permute A, B, C
    printf("\n\nPermuting A: [%d", permA[0]);
    for(i = 1; i < permA.size(); i++)
        printf(" %d", permA[i]);
    printf("]\n");
    Permute(PA, A, permA);

    printf("\n\nPermuting B: [%d", permB[0]);
    for(i = 1; i < permB.size(); i++)
        printf(" %d", permB[i]);
    printf("]\n");
    Permute(PB, B, permB);

    printf("\n\nPermuting C: [%d", permC[0]);
    for(i = 1; i < permC.size(); i++)
        printf(" %d", permC[i]);
    printf("]\n");
    Permute(PC, C, permC);

    //View as matrices
    std::vector<ModeArray> MPAOldModes(2);
    MPAOldModes[0] = contractModes[0];
    MPAOldModes[1] = contractModes[1];

    std::vector<ModeArray> MPBOldModes(2);
    MPBOldModes[0] = contractModes[1];
    MPBOldModes[1] = contractModes[2];
    
    std::vector<ModeArray> MPCOldModes(2);
    MPCOldModes[0] = contractModes[0];
    MPCOldModes[1] = contractModes[2];

    ViewAsLowerOrder(MPA, PA, MPAOldModes );

    Print(PB, "PB");
    ViewAsLowerOrder(MPB, PB, MPBOldModes );
    ViewAsLowerOrder(MPC, PC, MPCOldModes );

    Print(MPA, "MPA");
    Print(MPB, "MPB");
    Print(MPC, "MPC");
    Gemm(alpha, MPA, MPB, beta, MPC);
    Print(MPC, "PostMult");
    //View as tensor

    std::vector<ObjShape> newShape(MPCOldModes.size());
    for(i = 0; i < newShape.size(); i++){
        ModeArray oldModes(MPCOldModes[i].size());
        for(j = 0; j < MPCOldModes[i].size(); j++){
            oldModes[j] = MPCOldModes[i][j];
        }
        newShape[i] = FilterVector(PC.Shape(), oldModes);
    }
    ModeArray MPCModes(2);
    MPCModes[0] = 0;
    MPCModes[1] = 1;
    ViewAsHigherOrder(PC, MPC, MPCModes, newShape);

    //Permute back the data
    printf("\n\nPermuting PC: [%d", invPermC[0]);
    for(i = 1; i < invPermC.size(); i++)
        printf(" %d", invPermC[i]);
    printf("]\n");
    Permute(C, PC, invPermC);
}

} // namespace tmen

#endif // ifndef TMEN_BTAS_CONTRACT_HPP
