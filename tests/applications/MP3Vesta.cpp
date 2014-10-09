/*
  This file is part of DxTer.
  DxTer is a prototype using the Design by Transformation (DxT)
  approach to program generation.

  Copyright (C) 2014, The University of Texas and Bryan Marker

  DxTer is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  DxTer is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with DxTer.  If not, see <http://www.gnu.org/licenses/>.
*/
// NOTE: It is possible to simply include "tensormental.hpp" instead
#include "tensormental.hpp"
using namespace tmen;
using namespace std;

#define GRIDORDER 4

template <typename T>
void PrintLocalSizes(const DistTensor<T>& A)
{
  const Int commRank = mpi::CommRank( mpi::COMM_WORLD );
  if (commRank == 0) {
    for (Unsigned i = 0; i < A.Order(); ++i) {
      cout << i << " is " << A.LocalDimension(i) << endl;
    }
  }
}


template <typename T>
void GatherAllModes(const DistTensor<T>& A, DistTensor<T>& B)
{
  DistTensor<T> *tmp = NULL;
  //  DistTensor<T> *tmp = new DistTensor<T>(A.TensorDist(), A.Grid());
  //  *tmp = A;

  const TensorDistribution dist = A.TensorDist();

  for (Unsigned mode = 0; mode < A.Order(); ++mode) {
    ModeDistribution modeDist = dist[mode];
    if (!(modeDist.empty())) {
      TensorDistribution newDist = (tmp ? tmp->TensorDist() : A.TensorDist());
      ModeDistribution newIgnoreDist = newDist[newDist.size() - 1];
      newIgnoreDist.insert(newIgnoreDist.end(), modeDist.begin(), modeDist.end());
      newDist[newDist.size() - 1] = newIgnoreDist;
      modeDist.clear();
      newDist[mode] = modeDist;
      DistTensor<T> *tmp2 = new DistTensor<T>(newDist, A.Grid());
      if (!tmp) {
    tmp2->GatherToOneRedistFrom(A, mode);
      }
      else {
    tmp2->GatherToOneRedistFrom(*tmp, mode);
    delete tmp;
      }
      tmp = tmp2;
    }
  }

  if (tmp) {

    if (TensorDistToString(B.TensorDist()) != TensorDistToString(tmp->TensorDist())) {
      cout << TensorDistToString(B.TensorDist()) << endl;
      cout << TensorDistToString(tmp->TensorDist()) << endl;
      throw;
    }

    B = *tmp;
    delete tmp;
  }
  else {
    B = A;
  }
}


void Usage(){
  std::cout << "./DistTensor <gridDim0> <gridDim1> ... \n";
  std::cout << "<gridDimK>   : dimension of mode-K of grid\n";
}

typedef struct Arguments{
  ObjShape gridShape;
  Unsigned nProcs;
  Unsigned smallStart;
  Unsigned smallEnd;
  Unsigned smallInc;
  Unsigned bigStart;
  Unsigned bigEnd;
  Unsigned bigInc;
  Unsigned procStart;
  Unsigned procMax;
} Params;

void ProcessInput(int argc,  char** const argv, Params& args){
  Unsigned i;
  Unsigned argCount = 0;
/*
  if(argCount + 1 >= argc){
    std::cerr << "Missing required gridOrder argument\n";
    Usage();
    throw ArgException();
  }

  if(argCount + GRIDORDER >= argc){
    std::cerr << "Missing required grid dimensions\n";
    Usage();
    throw ArgException();
  }

  args.gridShape.resize(GRIDORDER);
  args.nProcs = 1;
  for(int i = 0; i < GRIDORDER; i++){
    int gridDim = atoi(argv[++argCount]);
    if(gridDim <= 0){
      std::cerr << "Grid dim must be greater than 0\n";
      Usage();
      throw ArgException();
    }
    args.nProcs *= gridDim;
    args.gridShape[i] = gridDim;
  }
*/
  args.smallStart = atoi(argv[++argCount]);
  args.smallEnd = atoi(argv[++argCount]);
  args.smallInc = atoi(argv[++argCount]);
  args.bigStart = atoi(argv[++argCount]);
  args.bigEnd = atoi(argv[++argCount]);
  args.bigInc = atoi(argv[++argCount]);
  args.procStart = atoi(argv[++argCount]);
  args.procMax = atoi(argv[++argCount]);
  args.nProcs = 1 << args.procMax;
}

template<typename T>
void
Set(DistTensor<T>& A)
{
  Unsigned order = A.Order();
  Location loc(order);
  std::fill(loc.begin(), loc.end(), 0);
  Unsigned ptr = 0;
  bool stop = false;

  while(!stop){
    A.Set(loc, rand());
    if (loc.size() == 0)
      break;

    //Update
    loc[ptr]++;
    while(loc[ptr] == A.Dimension(ptr)){
      loc[ptr] = 0;
      ptr++;
      if(ptr == order){
    stop = true;
    break;
      }else{
    loc[ptr]++;
      }
    }
    ptr = 0;
  }
}

template<typename T>
void Load_Tensor_Helper(ifstream& fid, Mode mode, const Location& curLoc, DistTensor<T>& A){
    Unsigned i;
    Unsigned dim = A.Dimension(mode);
    Location newCurLoc = curLoc;
    for(i = 0; i < dim; i++){
        newCurLoc[mode] = i;
        if(mode == 0){
            char* valS = new char[8];
            fid.read(valS, 8);
            double val = *reinterpret_cast<double*>(valS);
//          std::cout << "val: " << val << std::endl;
//          std::memcpy(&val, &(valS[0]), sizeof(double));
//          printf("newVal %.03f\n", val);
            A.Set(newCurLoc, val);
        }else{
            Load_Tensor_Helper(fid, mode - 1, newCurLoc, A);
        }
    }
}

template<typename T>
void Load_Tensor(DistTensor<T>& A, const std::string& filename){
//  printf("Loading tensor\n");
//  PrintVector(A.Shape(), "of size");
    Unsigned order = A.Order();
    ifstream fid;
    fid.open(filename, std::ios::in | std::ios::binary);
    //Skip 4 bytes of Fortran
    fid.seekg(4);
    Location zeros(order, 0);
    Load_Tensor_Helper(fid, order - 1, zeros, A);
    fid.close();
}

template<typename T>
void Load_Tensor_efgh_Helper(ifstream& fid, Mode mode, const Location& curLoc, DistTensor<T>& A){
    Unsigned i;
    Unsigned dim = A.Dimension(mode);
    Location newCurLoc = curLoc;
    for(i = 0; i < dim; i++){
        if(mode == 3)
            newCurLoc[2] = i;
        else if(mode == 2)
            newCurLoc[3] = i;
        else
            newCurLoc[mode] = i;
        if(mode == 0){
            char* valS = new char[8];
            fid.read(valS, 8);
            double val = *reinterpret_cast<double*>(valS);
//          PrintVector(newCurLoc, "Setting loc");
//          std::cout << "to val: " << val << std::endl;
//          std::cout << "val: " << val << std::endl;
//          std::memcpy(&val, &(valS[0]), sizeof(double));
//          printf("newVal %.03f\n", val);
            A.Set(newCurLoc, -val);
        }else{
            Load_Tensor_efgh_Helper(fid, mode - 1, newCurLoc, A);
        }
    }
}

template<typename T>
void Load_Tensor_efgh(DistTensor<T>& A, const std::string& filename){
//  printf("Loading tensor\n");
//  PrintVector(A.Shape(), "of size");
    Unsigned order = A.Order();
    ifstream fid;
    fid.open(filename, std::ios::in | std::ios::binary);
    //Skip 4 bytes of Fortran
    fid.seekg(4);
    Location zeros(order, 0);
    Load_Tensor_efgh_Helper(fid, order - 1, zeros, A);
    fid.close();
}

template<typename T>
void Load_Tensor_aijb_Helper(ifstream& fid, Mode mode, const Location& curLoc, DistTensor<T>& A){
    Unsigned i;
    Unsigned dim = A.Dimension(mode);
    Location newCurLoc = curLoc;
    for(i = 0; i < dim; i++){
        if(mode == 2)
            newCurLoc[1] = i;
        else if(mode == 1)
            newCurLoc[2] = i;
        else
            newCurLoc[mode] = i;
        if(mode == 0){
            char* valS = new char[8];
            fid.read(valS, 8);
            double val = *reinterpret_cast<double*>(valS);
//          PrintVector(newCurLoc, "Setting loc");
//          std::cout << "to val: " << val << std::endl;
//          std::cout << "val: " << val << std::endl;
//          std::memcpy(&val, &(valS[0]), sizeof(double));
//          printf("newVal %.03f\n", val);
            A.Set(newCurLoc, val);
        }else{
            Load_Tensor_aijb_Helper(fid, mode - 1, newCurLoc, A);
        }
    }
}

template<typename T>
void Load_Tensor_aijb(DistTensor<T>& A, const std::string& filename){
//  printf("Loading tensor\n");
//  PrintVector(A.Shape(), "of size");
    Unsigned order = A.Order();
    ifstream fid;
    fid.open(filename, std::ios::in | std::ios::binary);
    //Skip 4 bytes of Fortran
    fid.seekg(4);
    Location zeros(order, 0);
    Load_Tensor_aijb_Helper(fid, order - 1, zeros, A);
    fid.close();
}

template<typename T>
void Form_D_abij_Helper(const DistTensor<T>& epsilonA, const DistTensor<T>& epsilonB, Mode mode, const Location& loc, DistTensor<T>& D_abij){
    Unsigned i;
    Unsigned dim = D_abij.Dimension(mode);
    Location newCurLoc = loc;
    for(i = 0; i < dim; i++){
        newCurLoc[mode] = i;
        if(mode == 0){
            Location epsLoc(1);
            epsLoc[0] = newCurLoc[0];
            double e_a = epsilonA.Get(epsLoc);

            epsLoc[0] = newCurLoc[1];
            double e_b = epsilonA.Get(epsLoc);

            epsLoc[0] = newCurLoc[2];
            double e_i = epsilonB.Get(epsLoc);

            epsLoc[0] = newCurLoc[3];
            double e_j = epsilonB.Get(epsLoc);
            double val = -1.0 / (e_a + e_b - e_i - e_j);
            D_abij.Set(newCurLoc, val);
        }else{
            Form_D_abij_Helper(epsilonA, epsilonB, mode - 1, newCurLoc, D_abij);
        }
    }
}

template<typename T>
void Form_D_abij(const DistTensor<T>& epsilonA, const DistTensor<T>& epsilonB, DistTensor<T>& D_abij){
    Unsigned order = D_abij.Order();

    Location zeros(order, 0);
    Form_D_abij_Helper(epsilonA, epsilonB, order - 1, zeros, D_abij);
}


template<typename T>
void
DistTensorTest( const Grid& g, Unsigned tenDimFive, Unsigned tenDimFiftyThree )
{
#ifndef RELEASE
  CallStackEntry entry("DistTensorTest");
#endif
  Unsigned i;
  const Int commRank = mpi::CommRank( mpi::COMM_WORLD );
  const Unsigned gridOrder = 4;

  ObjShape tempShape;
TensorDistribution dist____N_D_0_1_2_3 = tmen::StringToTensorDist("[]|(0,1,2,3)");
TensorDistribution dist__S__S__D_2__D_3 = tmen::StringToTensorDist("[(),(),(2),(3)]");
TensorDistribution dist__S__D_1__D_2__D_3 = tmen::StringToTensorDist("[(),(1),(2),(3)]");
TensorDistribution dist__D_0__D_1__S__S__D_2__D_3 = tmen::StringToTensorDist("[(0),(1),(),(),(2),(3)]");
TensorDistribution dist__D_0__D_1__S__S = tmen::StringToTensorDist("[(0),(1),(),()]");
TensorDistribution dist__D_0__D_1__S__D_3 = tmen::StringToTensorDist("[(0),(1),(),(3)]");
TensorDistribution dist__D_0__D_1__D_2__D_3 = tmen::StringToTensorDist("[(0),(1),(2),(3)]");
TensorDistribution dist__D_0__D_1__D_3__D_2 = tmen::StringToTensorDist("[(0),(1),(3),(2)]");
TensorDistribution dist__D_2_0__D_1__S__D_3 = tmen::StringToTensorDist("[(2,0),(1),(),(3)]");
TensorDistribution dist__D_1__S__D_3__S__D_2__D_0 = tmen::StringToTensorDist("[(1),(),(3),(),(2),(0)]");
TensorDistribution dist__D_1__D_2__D_3__D_0 = tmen::StringToTensorDist("[(1),(2),(3),(0)]");
TensorDistribution dist__D_2__S__S__D_0 = tmen::StringToTensorDist("[(2),(),(),(0)]");
TensorDistribution dist__D_2__S__D_0__S = tmen::StringToTensorDist("[(2),(),(0),()]");
TensorDistribution dist__D_2__D_1__S__D_0 = tmen::StringToTensorDist("[(2),(1),(),(0)]");
TensorDistribution dist__D_2__D_1__S__D_3_0 = tmen::StringToTensorDist("[(2),(1),(),(3,0)]");
TensorDistribution dist__D_2__D_1__S__D_3 = tmen::StringToTensorDist("[(2),(1),(),(3)]");
TensorDistribution dist__D_2__D_1__S__D_0_3 = tmen::StringToTensorDist("[(2),(1),(),(0,3)]");
TensorDistribution dist__D_2__D_1__D_0__S = tmen::StringToTensorDist("[(2),(1),(0),()]");
TensorDistribution dist__D_2__D_1__D_0__D_3 = tmen::StringToTensorDist("[(2),(1),(0),(3)]");
TensorDistribution dist__D_2__D_3__S__S = tmen::StringToTensorDist("[(2),(3),(),()]");
TensorDistribution dist__D_2__D_3__S__D_1 = tmen::StringToTensorDist("[(2),(3),(),(1)]");
TensorDistribution dist__D_0_2__D_1__S__D_3 = tmen::StringToTensorDist("[(0,2),(1),(),(3)]");
    //E_MP3[D0,D1,D2,D3]
DistTensor<double> E_MP3__D_0__D_1__D_2__D_3( dist__D_0__D_1__D_2__D_3, g );
    //E_MP3[] | {0,1,2,3}
DistTensor<double> E_MP3____N_D_0_1_2_3( dist____N_D_0_1_2_3, g );
    //accum_temp[D0,D1,D2,D3]
DistTensor<double> accum_temp__D_0__D_1__D_2__D_3( dist__D_0__D_1__D_2__D_3, g );
    //accum_temp[D0,D1,*,*,D2,D3]
DistTensor<double> accum_temp__D_0__D_1__S__S__D_2__D_3( dist__D_0__D_1__S__S__D_2__D_3, g );
    //accum_temp[D1,*,D3,*,D2,D0]
DistTensor<double> accum_temp__D_1__S__D_3__S__D_2__D_0( dist__D_1__S__D_3__S__D_2__D_0, g );
    //accum_temp_temp[D0,D1,D2,D3]
DistTensor<double> accum_temp_temp__D_0__D_1__D_2__D_3( dist__D_0__D_1__D_2__D_3, g );
    //accum_temp_temp[D1,D2,D3,D0]
DistTensor<double> accum_temp_temp__D_1__D_2__D_3__D_0( dist__D_1__D_2__D_3__D_0, g );
    //axppx2_temp[D0,D1,D2,D3]
DistTensor<double> axppx2_temp__D_0__D_1__D_2__D_3( dist__D_0__D_1__D_2__D_3, g );
    //axppx2_temp[D2,D1,D0,D3]
DistTensor<double> axppx2_temp__D_2__D_1__D_0__D_3( dist__D_2__D_1__D_0__D_3, g );
    //axppx2_temp[D2,D1,D0,*]
DistTensor<double> axppx2_temp__D_2__D_1__D_0__S( dist__D_2__D_1__D_0__S, g );
    //axppx2_temp[D2,*,D0,*]
DistTensor<double> axppx2_temp__D_2__S__D_0__S( dist__D_2__S__D_0__S, g );
    //axppx3_temp[D0,D1,D2,D3]
DistTensor<double> axppx3_temp__D_0__D_1__D_2__D_3( dist__D_0__D_1__D_2__D_3, g );
    //cont1_temp[D0,D1,D2,D3]
DistTensor<double> cont1_temp__D_0__D_1__D_2__D_3( dist__D_0__D_1__D_2__D_3, g );
    //cont1_temp[D0,D1,D3,D2]
DistTensor<double> cont1_temp__D_0__D_1__D_3__D_2( dist__D_0__D_1__D_3__D_2, g );
    //cont1_temp[D1,D2,D3,D0]
DistTensor<double> cont1_temp__D_1__D_2__D_3__D_0( dist__D_1__D_2__D_3__D_0, g );
    //cont1_temp[D1,*,D3,*,D2,D0]
DistTensor<double> cont1_temp__D_1__S__D_3__S__D_2__D_0( dist__D_1__S__D_3__S__D_2__D_0, g );
    //t_efmn[D02,D1,*,D3]
DistTensor<double> t_efmn__D_0_2__D_1__S__D_3( dist__D_0_2__D_1__S__D_3, g );
    //t_efmn[D0,D1,D2,D3]
DistTensor<double> t_efmn__D_0__D_1__D_2__D_3( dist__D_0__D_1__D_2__D_3, g );
    //t_efmn[D0,D1,D3,D2]
DistTensor<double> t_efmn__D_0__D_1__D_3__D_2( dist__D_0__D_1__D_3__D_2, g );
    //t_efmn[D0,D1,*,D3]
DistTensor<double> t_efmn__D_0__D_1__S__D_3( dist__D_0__D_1__S__D_3, g );
    //t_efmn[D0,D1,*,*]
DistTensor<double> t_efmn__D_0__D_1__S__S( dist__D_0__D_1__S__S, g );
    //t_efmn[D20,D1,*,D3]
DistTensor<double> t_efmn__D_2_0__D_1__S__D_3( dist__D_2_0__D_1__S__D_3, g );
    //t_efmn[D2,D1,*,D0]
DistTensor<double> t_efmn__D_2__D_1__S__D_0( dist__D_2__D_1__S__D_0, g );
    //t_efmn[D2,D1,*,D03]
DistTensor<double> t_efmn__D_2__D_1__S__D_0_3( dist__D_2__D_1__S__D_0_3, g );
    //t_efmn[D2,D1,*,D3]
DistTensor<double> t_efmn__D_2__D_1__S__D_3( dist__D_2__D_1__S__D_3, g );
    //t_efmn[D2,D1,*,D30]
DistTensor<double> t_efmn__D_2__D_1__S__D_3_0( dist__D_2__D_1__S__D_3_0, g );
    //t_efmn[D2,D3,*,D1]
DistTensor<double> t_efmn__D_2__D_3__S__D_1( dist__D_2__D_3__S__D_1, g );
    //t_efmn[D2,D3,*,*]
DistTensor<double> t_efmn__D_2__D_3__S__S( dist__D_2__D_3__S__S, g );
    //t_efmn[D2,*,*,D0]
DistTensor<double> t_efmn__D_2__S__S__D_0( dist__D_2__S__S__D_0, g );
    //v2_oegm[D0,D1,D2,D3]
DistTensor<double> v2_oegm__D_0__D_1__D_2__D_3( dist__D_0__D_1__D_2__D_3, g );
    //v_efgh[D0,D1,D2,D3]
DistTensor<double> v_efgh__D_0__D_1__D_2__D_3( dist__D_0__D_1__D_2__D_3, g );
    //v_oegm[D0,D1,D2,D3]
DistTensor<double> v_oegm__D_0__D_1__D_2__D_3( dist__D_0__D_1__D_2__D_3, g );
    //v_opmn[D0,D1,D2,D3]
DistTensor<double> v_opmn__D_0__D_1__D_2__D_3( dist__D_0__D_1__D_2__D_3, g );
    //v_opmn[*,D1,D2,D3]
DistTensor<double> v_opmn__S__D_1__D_2__D_3( dist__S__D_1__D_2__D_3, g );
    //v_opmn[*,*,D2,D3]
DistTensor<double> v_opmn__S__S__D_2__D_3( dist__S__S__D_2__D_3, g );
ModeArray modes_0;
modes_0.push_back(0);
ModeArray modes_0_1_2_3;
modes_0_1_2_3.push_back(0);
modes_0_1_2_3.push_back(1);
modes_0_1_2_3.push_back(2);
modes_0_1_2_3.push_back(3);
ModeArray modes_0_2;
modes_0_2.push_back(0);
modes_0_2.push_back(2);
ModeArray modes_1;
modes_1.push_back(1);
ModeArray modes_1_2_3_0;
modes_1_2_3_0.push_back(1);
modes_1_2_3_0.push_back(2);
modes_1_2_3_0.push_back(3);
modes_1_2_3_0.push_back(0);
ModeArray modes_1_3;
modes_1_3.push_back(1);
modes_1_3.push_back(3);
ModeArray modes_2;
modes_2.push_back(2);
ModeArray modes_2_0;
modes_2_0.push_back(2);
modes_2_0.push_back(0);
ModeArray modes_2_3;
modes_2_3.push_back(2);
modes_2_3.push_back(3);
ModeArray modes_3;
modes_3.push_back(3);
ModeArray modes_3_0;
modes_3_0.push_back(3);
modes_3_0.push_back(0);
ModeArray modes_3_1;
modes_3_1.push_back(3);
modes_3_1.push_back(1);
ModeArray modes_3_2;
modes_3_2.push_back(3);
modes_3_2.push_back(2);
ModeArray modes_5_4;
modes_5_4.push_back(5);
modes_5_4.push_back(4);
IndexArray indices_efgh( 4 );
indices_efgh[0] = 'e';
indices_efgh[1] = 'f';
indices_efgh[2] = 'g';
indices_efgh[3] = 'h';
IndexArray indices_efmn( 4 );
indices_efmn[0] = 'e';
indices_efmn[1] = 'f';
indices_efmn[2] = 'm';
indices_efmn[3] = 'n';
IndexArray indices_efmngh( 6 );
indices_efmngh[0] = 'e';
indices_efmngh[1] = 'f';
indices_efmngh[2] = 'm';
indices_efmngh[3] = 'n';
indices_efmngh[4] = 'g';
indices_efmngh[5] = 'h';
IndexArray indices_efmngo( 6 );
indices_efmngo[0] = 'e';
indices_efmngo[1] = 'f';
indices_efmngo[2] = 'm';
indices_efmngo[3] = 'n';
indices_efmngo[4] = 'g';
indices_efmngo[5] = 'o';
IndexArray indices_efop( 4 );
indices_efop[0] = 'e';
indices_efop[1] = 'f';
indices_efop[2] = 'o';
indices_efop[3] = 'p';
IndexArray indices_gfno( 4 );
indices_gfno[0] = 'g';
indices_gfno[1] = 'f';
indices_gfno[2] = 'n';
indices_gfno[3] = 'o';
IndexArray indices_gfon( 4 );
indices_gfon[0] = 'g';
indices_gfon[1] = 'f';
indices_gfon[2] = 'o';
indices_gfon[3] = 'n';
IndexArray indices_ghmn( 4 );
indices_ghmn[0] = 'g';
indices_ghmn[1] = 'h';
indices_ghmn[2] = 'm';
indices_ghmn[3] = 'n';
IndexArray indices_oegm( 4 );
indices_oegm[0] = 'o';
indices_oegm[1] = 'e';
indices_oegm[2] = 'g';
indices_oegm[3] = 'm';
IndexArray indices_opmn( 4 );
indices_opmn[0] = 'o';
indices_opmn[1] = 'p';
indices_opmn[2] = 'm';
indices_opmn[3] = 'n';
std::vector<ModeArray> modeArrayArray___0___2;
modeArrayArray___0___2.push_back(modes_0);
modeArrayArray___0___2.push_back(modes_2);
std::vector<ModeArray> modeArrayArray___1___2___3___0;
modeArrayArray___1___2___3___0.push_back(modes_1);
modeArrayArray___1___2___3___0.push_back(modes_2);
modeArrayArray___1___2___3___0.push_back(modes_3);
modeArrayArray___1___2___3___0.push_back(modes_0);
std::vector<ModeArray> modeArrayArray___1___3;
modeArrayArray___1___3.push_back(modes_1);
modeArrayArray___1___3.push_back(modes_3);
std::vector<ModeArray> modeArrayArray___2___3;
modeArrayArray___2___3.push_back(modes_2);
modeArrayArray___2___3.push_back(modes_3);
Permutation perm_0_1_3_2;
perm_0_1_3_2.push_back(0);
perm_0_1_3_2.push_back(1);
perm_0_1_3_2.push_back(3);
perm_0_1_3_2.push_back(2);

const unsigned fiftythree = tenDimFiftyThree;
const unsigned five = tenDimFive;

// t_efmn has 4 dims
//  Starting distribution: [D0,D1,D2,D3] or _D_0__D_1__D_2__D_3
ObjShape t_efmn__D_0__D_1__D_2__D_3_tempShape;
t_efmn__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
t_efmn__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
t_efmn__D_0__D_1__D_2__D_3_tempShape.push_back( five);
t_efmn__D_0__D_1__D_2__D_3_tempShape.push_back( five);
t_efmn__D_0__D_1__D_2__D_3.ResizeTo( t_efmn__D_0__D_1__D_2__D_3_tempShape );
//MakeUniform( t_efmn__D_0__D_1__D_2__D_3 );
DistTensor<T> t_efmn_local( tmen::StringToTensorDist("[(),(),(),()]|(0,1,2,3)"), g );
GatherAllModes( t_efmn__D_0__D_1__D_2__D_3, t_efmn_local );
//Print(t_efmn__D_0__D_1__D_2__D_3, "t_efmn");
// axppx2_temp has 4 dims
//  Starting distribution: [D0,D1,D2,D3] or _D_0__D_1__D_2__D_3
ObjShape axppx2_temp__D_0__D_1__D_2__D_3_tempShape;
axppx2_temp__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
axppx2_temp__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
axppx2_temp__D_0__D_1__D_2__D_3_tempShape.push_back( five);
axppx2_temp__D_0__D_1__D_2__D_3_tempShape.push_back( five);
axppx2_temp__D_0__D_1__D_2__D_3.ResizeTo( axppx2_temp__D_0__D_1__D_2__D_3_tempShape );
//Zero(axppx2_temp__D_0__D_1__D_2__D_3);
//MakeUniform( axppx2_temp__D_0__D_1__D_2__D_3 );
DistTensor<T> axppx2_temp_local( tmen::StringToTensorDist("[(),(),(),()]|(0,1,2,3)"), g );
GatherAllModes( axppx2_temp__D_0__D_1__D_2__D_3, axppx2_temp_local );
// v_opmn has 4 dims
//  Starting distribution: [D0,D1,D2,D3] or _D_0__D_1__D_2__D_3
ObjShape v_opmn__D_0__D_1__D_2__D_3_tempShape;
v_opmn__D_0__D_1__D_2__D_3_tempShape.push_back( five);
v_opmn__D_0__D_1__D_2__D_3_tempShape.push_back( five);
v_opmn__D_0__D_1__D_2__D_3_tempShape.push_back( five);
v_opmn__D_0__D_1__D_2__D_3_tempShape.push_back( five);
v_opmn__D_0__D_1__D_2__D_3.ResizeTo( v_opmn__D_0__D_1__D_2__D_3_tempShape );
//MakeUniform( v_opmn__D_0__D_1__D_2__D_3 );
DistTensor<T> v_opmn_local( tmen::StringToTensorDist("[(),(),(),()]|(0,1,2,3)"), g );
//GatherAllModes( v_opmn__D_0__D_1__D_2__D_3, v_opmn_local );
//Print(v_opmn__D_0__D_1__D_2__D_3, "v_opmn");
// v_efgh has 4 dims
//  Starting distribution: [D0,D1,D2,D3] or _D_0__D_1__D_2__D_3
ObjShape v_efgh__D_0__D_1__D_2__D_3_tempShape;
v_efgh__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
v_efgh__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
v_efgh__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
v_efgh__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
v_efgh__D_0__D_1__D_2__D_3.ResizeTo( v_efgh__D_0__D_1__D_2__D_3_tempShape );
//MakeUniform( v_efgh__D_0__D_1__D_2__D_3 );
DistTensor<T> v_efgh_local( tmen::StringToTensorDist("[(),(),(),()]|(0,1,2,3)"), g );
//GatherAllModes( v_efgh__D_0__D_1__D_2__D_3, v_efgh_local );
//Print(v_efgh__D_0__D_1__D_2__D_3, "v_efgh");
// v_oegm has 4 dims
//  Starting distribution: [D0,D1,D2,D3] or _D_0__D_1__D_2__D_3
ObjShape v_oegm__D_0__D_1__D_2__D_3_tempShape;
v_oegm__D_0__D_1__D_2__D_3_tempShape.push_back( five);
v_oegm__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
v_oegm__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
v_oegm__D_0__D_1__D_2__D_3_tempShape.push_back( five);
v_oegm__D_0__D_1__D_2__D_3.ResizeTo( v_oegm__D_0__D_1__D_2__D_3_tempShape );
//MakeUniform( v_oegm__D_0__D_1__D_2__D_3 );
DistTensor<T> v_oegm_local( tmen::StringToTensorDist("[(),(),(),()]|(0,1,2,3)"), g );
//GatherAllModes( v_oegm__D_0__D_1__D_2__D_3, v_oegm_local );
//Print(v_oegm__D_0__D_1__D_2__D_3, "v_oegm");
// v2_oegm has 4 dims
//  Starting distribution: [D0,D1,D2,D3] or _D_0__D_1__D_2__D_3
ObjShape v2_oegm__D_0__D_1__D_2__D_3_tempShape;
v2_oegm__D_0__D_1__D_2__D_3_tempShape.push_back( five);
v2_oegm__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
v2_oegm__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
v2_oegm__D_0__D_1__D_2__D_3_tempShape.push_back( five);
v2_oegm__D_0__D_1__D_2__D_3.ResizeTo( v2_oegm__D_0__D_1__D_2__D_3_tempShape );
//MakeUniform( v2_oegm__D_0__D_1__D_2__D_3 );
DistTensor<T> v2_oegm_local( tmen::StringToTensorDist("[(),(),(),()]|(0,1,2,3)"), g );
//GatherAllModes( v2_oegm__D_0__D_1__D_2__D_3, v2_oegm_local );
//Print(v2_oegm__D_0__D_1__D_2__D_3, "v_oegm");
// axppx3_temp has 4 dims
//  Starting distribution: [D0,D1,D2,D3] or _D_0__D_1__D_2__D_3
ObjShape axppx3_temp__D_0__D_1__D_2__D_3_tempShape;
axppx3_temp__D_0__D_1__D_2__D_3_tempShape.push_back( five);
axppx3_temp__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
axppx3_temp__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
axppx3_temp__D_0__D_1__D_2__D_3_tempShape.push_back( five);
axppx3_temp__D_0__D_1__D_2__D_3.ResizeTo( axppx3_temp__D_0__D_1__D_2__D_3_tempShape );
//Zero(axppx3_temp__D_0__D_1__D_2__D_3);
//MakeUniform( axppx3_temp__D_0__D_1__D_2__D_3 );
DistTensor<T> axppx3_temp_local( tmen::StringToTensorDist("[(),(),(),()]|(0,1,2,3)"), g );
//GatherAllModes( axppx3_temp__D_0__D_1__D_2__D_3, axppx3_temp_local );
// cont1_temp has 4 dims
//  Starting distribution: [D0,D1,D2,D3] or _D_0__D_1__D_2__D_3
ObjShape cont1_temp__D_0__D_1__D_2__D_3_tempShape;
cont1_temp__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
cont1_temp__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
cont1_temp__D_0__D_1__D_2__D_3_tempShape.push_back( five);
cont1_temp__D_0__D_1__D_2__D_3_tempShape.push_back( five);
cont1_temp__D_0__D_1__D_2__D_3.ResizeTo( cont1_temp__D_0__D_1__D_2__D_3_tempShape );
//Zero(cont1_temp__D_0__D_1__D_2__D_3);
//MakeUniform( cont1_temp__D_0__D_1__D_2__D_3 );
DistTensor<T> cont1_temp_local( tmen::StringToTensorDist("[(),(),(),()]|(0,1,2,3)"), g );
//GatherAllModes( cont1_temp__D_0__D_1__D_2__D_3, cont1_temp_local );
// accum_temp has 4 dims
//  Starting distribution: [D0,D1,D2,D3] or _D_0__D_1__D_2__D_3
ObjShape accum_temp__D_0__D_1__D_2__D_3_tempShape;
accum_temp__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
accum_temp__D_0__D_1__D_2__D_3_tempShape.push_back( fiftythree );
accum_temp__D_0__D_1__D_2__D_3_tempShape.push_back( five);
accum_temp__D_0__D_1__D_2__D_3_tempShape.push_back( five);
accum_temp__D_0__D_1__D_2__D_3.ResizeTo( accum_temp__D_0__D_1__D_2__D_3_tempShape );
//Zero(accum_temp__D_0__D_1__D_2__D_3);
//MakeUniform( accum_temp__D_0__D_1__D_2__D_3 );
DistTensor<T> accum_temp_local( tmen::StringToTensorDist("[(),(),(),()]|(0,1,2,3)"), g );
//GatherAllModes( accum_temp__D_0__D_1__D_2__D_3, accum_temp_local );
// scalar input has 0 dims
//  Starting distribution: [] | {0,1,2,3} or ___N_D_0_1_2_3
ObjShape E_MP3____N_D_0_1_2_3_tempShape;
E_MP3____N_D_0_1_2_3.ResizeTo( E_MP3____N_D_0_1_2_3_tempShape );
//Zero(E_MP3____N_D_0_1_2_3);
//MakeUniform( E_MP3____N_D_0_1_2_3 );
DistTensor<T> E_MP3_local( tmen::StringToTensorDist("[]|(0,1,2,3)"), g );
GatherAllModes( E_MP3____N_D_0_1_2_3, E_MP3_local );

//******************************
//* Load tensors
//******************************

  double startLoadTime = mpi::Time();
  DistTensor<T> epsilonA( tmen::StringToTensorDist("[(0)]|()"), g);
  ObjShape epsilonAShape;
  epsilonAShape.push_back(fiftythree);
  epsilonA.ResizeTo(epsilonAShape);
  std::string epsilonAFilename = "data/ea";
  //printf("loading epsilonA\n");
//  Load_Tensor(epsilonA, epsilonAFilename);
  MakeUniform(epsilonA);
  //Print(epsilonA, "eps_a");

  DistTensor<T> epsilonB( tmen::StringToTensorDist("[(0)]|()"), g);
  ObjShape epsilonBShape;
  epsilonBShape.push_back(five);
  epsilonB.ResizeTo(epsilonBShape);
  std::string epsilonBFilename = "data/ei";
  //printf("loading epsilonB\n");
//  Load_Tensor(epsilonB, epsilonBFilename);
  MakeUniform(epsilonB);
  //Print(epsilonB, "eps_b");

  DistTensor<T> D_abij( tmen::StringToTensorDist("[(0),(1),(2),(3)]|()"), g);
  ObjShape D_abijShape;
  D_abijShape.push_back(fiftythree);
  D_abijShape.push_back(fiftythree);
  D_abijShape.push_back(five);
  D_abijShape.push_back(five);
  D_abij.ResizeTo(D_abijShape);

  DistTensor<T> V_abij( tmen::StringToTensorDist("[(0),(1),(2),(3)]|()"), g);
  V_abij.ResizeTo(D_abijShape);
  std::string v_abijFilename = "data/abij";
  //printf("loading V_abij\n");
  //Load_Tensor(V_abij, v_abijFilename);
  MakeUniform(V_abij);
  //Print(V_abij, "v_abij");

  std::string v_opmnFilename = "data/ijkl";
  //printf("loading v_opmn\n");
  //Load_Tensor(v_opmn__D_0__D_1__D_2__D_3, v_opmnFilename);
  MakeUniform(v_opmn__D_0__D_1__D_2__D_3);
  //Print(v_opmn__D_0__D_1__D_2__D_3, "v_opmn");

  //printf("loading 4\n");
  std::string v_oegmFilename = "data/aijb";
  //printf("loading v_oegm\n");
  //Load_Tensor_aijb(v_oegm__D_0__D_1__D_2__D_3, v_oegmFilename);
  MakeUniform(v_oegm__D_0__D_1__D_2__D_3);
  //Print(v_oegm__D_0__D_1__D_2__D_3, "v_oegm");

  //printf("loading 5\n");
  std::string v2_oegmFilename = "data/aibj";
  //printf("loading v2_oegm\n");
  //Load_Tensor(v2_oegm__D_0__D_1__D_2__D_3, v2_oegmFilename);
  MakeUniform(v2_oegm__D_0__D_1__D_2__D_3);
  //Print(v2_oegm__D_0__D_1__D_2__D_3, "v2_oegm");

  //printf("loading 3\n");
  std::string v_efghFilename = "data/abcd";
  //printf("loading v_efgh\n");
  //Load_Tensor_efgh(v_efgh__D_0__D_1__D_2__D_3, v_efghFilename);
  MakeUniform(v_efgh__D_0__D_1__D_2__D_3);
  //Print(v_efgh__D_0__D_1__D_2__D_3, "v_efgh");
  double runLoadTime = mpi::Time() - startLoadTime;

//  printf("load time: %d\n", runLoadTime);
//  printf("elemScaling\n");
  Form_D_abij(epsilonA, epsilonB, D_abij);
  tmen::ElemScal(V_abij, D_abij, t_efmn__D_0__D_1__D_2__D_3);
  //Print(t_efmn__D_0__D_1__D_2__D_3, "t_efmn");

//  printf("zeroing\n");
  //Zero out the temporaries
  Zero(axppx2_temp__D_0__D_1__D_2__D_3);
  Zero(axppx3_temp__D_0__D_1__D_2__D_3);
  Zero(cont1_temp__D_0__D_1__D_2__D_3);
  Zero(accum_temp__D_0__D_1__D_2__D_3);
  Zero(accum_temp_temp__D_0__D_1__D_2__D_3);
  Zero(E_MP3____N_D_0_1_2_3);

//  printf("gathering\n");
  //Form local versions of tensors
  GatherAllModes( t_efmn__D_0__D_1__D_2__D_3, t_efmn_local );
  GatherAllModes( v_opmn__D_0__D_1__D_2__D_3, v_opmn_local );
  GatherAllModes( v_efgh__D_0__D_1__D_2__D_3, v_efgh_local );
  GatherAllModes( v_oegm__D_0__D_1__D_2__D_3, v_oegm_local );
  GatherAllModes( v2_oegm__D_0__D_1__D_2__D_3, v2_oegm_local );
  GatherAllModes( axppx2_temp__D_0__D_1__D_2__D_3, axppx2_temp_local );
  GatherAllModes( axppx3_temp__D_0__D_1__D_2__D_3, axppx3_temp_local );
  GatherAllModes( cont1_temp__D_0__D_1__D_2__D_3, cont1_temp_local );
  GatherAllModes( accum_temp__D_0__D_1__D_2__D_3, accum_temp_local );
  GatherAllModes( E_MP3____N_D_0_1_2_3, E_MP3_local );

//******************************
//* Load tensors
//******************************

  //**** (out of 1)
  //------------------------------------//

    double gflops;
    double startTime;
    double runTime;
//    if(commRank == 0)
//        std::cout << "starting\n";
//    printf("starting\n");
    mpi::Barrier(g.OwningComm());
    startTime = mpi::Time();
//  printf("started\n");
//**** (out of 1)
    //------------------------------------//

    ZAxpBy( 2.0, v_oegm__D_0__D_1__D_2__D_3, -1.0, v2_oegm__D_0__D_1__D_2__D_3, axppx3_temp__D_0__D_1__D_2__D_3 );
    //Print(axppx3_temp__D_0__D_1__D_2__D_3, "axppx3_temp__D_0__D_1__D_2__D_3");

    //------------------------------------//

//****
//**** (out of 16)
    //------------------------------------//

    tempShape = E_MP3____N_D_0_1_2_3.Shape();
    tempShape.push_back( g.Shape()[0] );
    tempShape.push_back( g.Shape()[1] );
    tempShape.push_back( g.Shape()[2] );
    tempShape.push_back( g.Shape()[3] );
    E_MP3__D_0__D_1__D_2__D_3.ResizeTo( tempShape );
    tempShape = cont1_temp__D_0__D_1__D_2__D_3.Shape();
    tempShape.push_back( g.Shape()[2] );
    tempShape.push_back( g.Shape()[0] );
    cont1_temp__D_1__S__D_3__S__D_2__D_0.ResizeTo( tempShape );
    tempShape = cont1_temp__D_0__D_1__D_2__D_3.Shape();
    cont1_temp__D_1__D_2__D_3__D_0.ResizeTo( tempShape );
    //**** (out of 2)
    //**** Is a shadow
        //------------------------------------//

           // v_opmn[*,D1,D2,D3] <- v_opmn[D0,D1,D2,D3]
        v_opmn__S__D_1__D_2__D_3.AllGatherRedistFrom( v_opmn__D_0__D_1__D_2__D_3, 0, modes_0 );
           // v_opmn[*,*,D2,D3] <- v_opmn[*,D1,D2,D3]
        v_opmn__S__S__D_2__D_3.AllGatherRedistFrom( v_opmn__S__D_1__D_2__D_3, 1, modes_1 );

        //------------------------------------//

    //****
    //**** (out of 576)
    //**** Is real  1 shadows
        //------------------------------------//

           // t_efmn[D0,D1,D3,D2] <- t_efmn[D0,D1,D2,D3]
        t_efmn__D_0__D_1__D_3__D_2.AllToAllRedistFrom( t_efmn__D_0__D_1__D_2__D_3, modes_2_3, modes_3_2, modeArrayArray___2___3 );
           // t_efmn[D0,D1,*,D3] <- t_efmn[D0,D1,D2,D3]
        t_efmn__D_0__D_1__S__D_3.AllGatherRedistFrom( t_efmn__D_0__D_1__D_2__D_3, 2, modes_2 );
           // t_efmn[D0,D1,*,*] <- t_efmn[D0,D1,*,D3]
        t_efmn__D_0__D_1__S__S.AllGatherRedistFrom( t_efmn__D_0__D_1__S__D_3, 3, modes_3 );
           // t_efmn[D02,D1,*,D3] <- t_efmn[D0,D1,*,D3]
        t_efmn__D_0_2__D_1__S__D_3.LocalRedistFrom( t_efmn__D_0__D_1__S__D_3, 0, modes_2 );
           // t_efmn[D20,D1,*,D3] <- t_efmn[D02,D1,*,D3]
        t_efmn__D_2_0__D_1__S__D_3.PermutationRedistFrom( t_efmn__D_0_2__D_1__S__D_3, 0, modes_0_2 );
           // t_efmn[D2,D1,*,D3] <- t_efmn[D20,D1,*,D3]
        t_efmn__D_2__D_1__S__D_3.AllGatherRedistFrom( t_efmn__D_2_0__D_1__S__D_3, 0, modes_0 );
           // t_efmn[D2,D3,*,D1] <- t_efmn[D2,D1,*,D3]
        t_efmn__D_2__D_3__S__D_1.AllToAllRedistFrom( t_efmn__D_2__D_1__S__D_3, modes_1_3, modes_3_1, modeArrayArray___1___3 );
           // t_efmn[D2,D1,*,D30] <- t_efmn[D2,D1,*,D3]
        t_efmn__D_2__D_1__S__D_3_0.LocalRedistFrom( t_efmn__D_2__D_1__S__D_3, 3, modes_0 );
           // t_efmn[D2,D1,*,D03] <- t_efmn[D2,D1,*,D30]
        t_efmn__D_2__D_1__S__D_0_3.PermutationRedistFrom( t_efmn__D_2__D_1__S__D_3_0, 3, modes_3_0 );
           // t_efmn[D2,D1,*,D0] <- t_efmn[D2,D1,*,D03]
        t_efmn__D_2__D_1__S__D_0.AllGatherRedistFrom( t_efmn__D_2__D_1__S__D_0_3, 3, modes_3 );
           // t_efmn[D2,D3,*,*] <- t_efmn[D2,D3,*,D1]
        t_efmn__D_2__D_3__S__S.AllGatherRedistFrom( t_efmn__D_2__D_3__S__D_1, 3, modes_1 );
           // t_efmn[D2,*,*,D0] <- t_efmn[D2,D1,*,D0]
        t_efmn__D_2__S__S__D_0.AllGatherRedistFrom( t_efmn__D_2__D_1__S__D_0, 1, modes_1 );

        //------------------------------------//
    //****
    YAxpPx( 2.0, t_efmn__D_0__D_1__D_2__D_3, -1.0, t_efmn__D_0__D_1__D_3__D_2, perm_0_1_3_2, axppx2_temp__D_0__D_1__D_2__D_3 );
//  Print(axppx2_temp__D_0__D_1__D_2__D_3, "After YAxpPx: axppx2_temp__D_0__D_1__D_2__D_3");
       // 1.0 * v2_oegm[D0,D1,D2,D3]_oegm * t_efmn[D2,*,*,D0]_gfno + 0.0 * cont1_temp[D1,*,D3,*,D2,D0]_efmngo
    LocalContract(1.0, v2_oegm__D_0__D_1__D_2__D_3.LockedTensor(), indices_oegm,
        t_efmn__D_2__S__S__D_0.LockedTensor(), indices_gfno,
        0.0, cont1_temp__D_1__S__D_3__S__D_2__D_0.Tensor(), indices_efmngo);
    //**** (out of 6)
    //**** Is a shadow
        //------------------------------------//

           // axppx2_temp[D2,D1,D0,D3] <- axppx2_temp[D0,D1,D2,D3]
        axppx2_temp__D_2__D_1__D_0__D_3.AllToAllRedistFrom( axppx2_temp__D_0__D_1__D_2__D_3, modes_0_2, modes_2_0, modeArrayArray___0___2 );
           // axppx2_temp[D2,D1,D0,*] <- axppx2_temp[D2,D1,D0,D3]
        axppx2_temp__D_2__D_1__D_0__S.AllGatherRedistFrom( axppx2_temp__D_2__D_1__D_0__D_3, 3, modes_3 );
           // axppx2_temp[D2,*,D0,*] <- axppx2_temp[D2,D1,D0,*]
        axppx2_temp__D_2__S__D_0__S.AllGatherRedistFrom( axppx2_temp__D_2__D_1__D_0__S, 1, modes_1 );

        //------------------------------------//

    //****
    //**** (out of 1)
    //**** Is a shadow
        //------------------------------------//

           // cont1_temp[D1,D2,D3,D0] <- cont1_temp[D1,*,D3,*,D2,D0] (with SumScatter on (D2)(D0))
        cont1_temp__D_1__D_2__D_3__D_0.ReduceScatterRedistFrom( cont1_temp__D_1__S__D_3__S__D_2__D_0, modes_5_4, modes_3_1 );
           // cont1_temp[D0,D1,D2,D3] <- cont1_temp[D1,D2,D3,D0]
        cont1_temp__D_0__D_1__D_2__D_3.AllToAllRedistFrom( cont1_temp__D_1__D_2__D_3__D_0, modes_0_1_2_3, modes_1_2_3_0, modeArrayArray___1___2___3___0 );
           // cont1_temp[D0,D1,D3,D2] <- cont1_temp[D0,D1,D2,D3]
        cont1_temp__D_0__D_1__D_3__D_2.AllToAllRedistFrom( cont1_temp__D_0__D_1__D_2__D_3, modes_2_3, modes_3_2, modeArrayArray___2___3 );

//  Print(cont1_temp__D_0__D_1__D_2__D_3, "After Contract: cont1_temp__D_0__D_1__D_2__D_3");
        //------------------------------------//

    //****
    YAxpPx( 0.5, cont1_temp__D_0__D_1__D_2__D_3, 1.0, cont1_temp__D_0__D_1__D_3__D_2, perm_0_1_3_2, accum_temp__D_0__D_1__D_2__D_3 );
    //Print(accum_temp__D_0__D_1__D_2__D_3, "After YAxpPx: accum_temp__D_0__D_1__D_2__D_3");
    tempShape = accum_temp__D_0__D_1__D_2__D_3.Shape();
    tempShape.push_back( g.Shape()[2] );
    tempShape.push_back( g.Shape()[0] );
    accum_temp__D_1__S__D_3__S__D_2__D_0.ResizeTo( tempShape );
       // 0.5 * axppx3_temp[D0,D1,D2,D3]_oegm * axppx2_temp[D2,*,D0,*]_gfon + 0.0 * accum_temp[D1,*,D3,*,D2,D0]_efmngo
    LocalContract(0.5, axppx3_temp__D_0__D_1__D_2__D_3.LockedTensor(), indices_oegm,
        axppx2_temp__D_2__S__D_0__S.LockedTensor(), indices_gfon,
        0.0, accum_temp__D_1__S__D_3__S__D_2__D_0.Tensor(), indices_efmngo);
    //Print(accum_temp__D_1__S__D_3__S__D_2__D_0, "After Contract: accum_temp");
    tempShape = accum_temp__D_0__D_1__D_2__D_3.Shape();
    accum_temp_temp__D_1__D_2__D_3__D_0.ResizeTo( tempShape );
    //**** (out of 1)
    //**** Is a shadow
        //------------------------------------//

           // accum_temp_temp[D1,D2,D3,D0] <- accum_temp[D1,*,D3,*,D2,D0] (with SumScatter on (D2)(D0))
        accum_temp_temp__D_1__D_2__D_3__D_0.ReduceScatterRedistFrom( accum_temp__D_1__S__D_3__S__D_2__D_0, modes_5_4, modes_3_1 );
           // accum_temp_temp[D0,D1,D2,D3] <- accum_temp_temp[D1,D2,D3,D0]
        accum_temp_temp__D_0__D_1__D_2__D_3.AllToAllRedistFrom( accum_temp_temp__D_1__D_2__D_3__D_0, modes_0_1_2_3, modes_1_2_3_0, modeArrayArray___1___2___3___0 );
    //Print(accum_temp_temp__D_0__D_1__D_2__D_3, "After Contract: accum_temp_temp__D_0__D_1__D_2__D_3");

        //------------------------------------//

    //****
    YxpBy( accum_temp_temp__D_0__D_1__D_2__D_3, -1.0, accum_temp__D_0__D_1__D_2__D_3 );
    //Print(accum_temp__D_0__D_1__D_2__D_3, "After YxpBy: accum_temp__D_0__D_1__D_2__D_3");
    tempShape = accum_temp__D_0__D_1__D_2__D_3.Shape();
    tempShape.push_back( g.Shape()[2] );
    tempShape.push_back( g.Shape()[3] );
    accum_temp__D_0__D_1__S__S__D_2__D_3.ResizeTo( tempShape );
       // 0.5 * v_efgh[D0,D1,D2,D3]_efgh * t_efmn[D2,D3,*,*]_ghmn + 0.0 * accum_temp[D0,D1,*,*,D2,D3]_efmngh
    LocalContract(0.5, v_efgh__D_0__D_1__D_2__D_3.LockedTensor(), indices_efgh,
        t_efmn__D_2__D_3__S__S.LockedTensor(), indices_ghmn,
        0.0, accum_temp__D_0__D_1__S__S__D_2__D_3.Tensor(), indices_efmngh);
    //Print(accum_temp__D_0__D_1__S__S__D_2__D_3, "After Contract: accum_temp__D_0__D_1__S__S__D_2__D_3");
    //**** (out of 1)
    //**** Is a shadow
        //------------------------------------//

           // accum_temp[D0,D1,D2,D3] <- accum_temp[D0,D1,*,*,D2,D3] (with SumScatter on (D2)(D3))
        accum_temp__D_0__D_1__D_2__D_3.ReduceScatterUpdateRedistFrom( accum_temp__D_0__D_1__S__S__D_2__D_3, 1.0, modes_5_4, modes_3_2 );
    //Print(accum_temp__D_0__D_1__D_2__D_3, "After Contract: accum_temp__D_0__D_1__D_2__D_3");

        //------------------------------------//

    //****
       // 0.5 * v_opmn[*,*,D2,D3]_opmn * t_efmn[D0,D1,*,*]_efop + 1.0 * accum_temp[D0,D1,D2,D3]_efmn
    LocalContractAndLocalEliminate(0.5, v_opmn__S__S__D_2__D_3.LockedTensor(), indices_opmn,
        t_efmn__D_0__D_1__S__S.LockedTensor(), indices_efop,
        1.0, accum_temp__D_0__D_1__D_2__D_3.Tensor(), indices_efmn);
    //Print(accum_temp__D_0__D_1__D_2__D_3, "After Contract: accum_temp__D_0__D_1__D_2__D_3");
       // 2.0 * axppx2_temp[D0,D1,D2,D3]_efmn * accum_temp[D0,D1,D2,D3]_efmn + 0.0 * E_MP3[D0,D1,D2,D3]_efmn
    LocalContract(2.0, axppx2_temp__D_0__D_1__D_2__D_3.LockedTensor(), indices_efmn,
        accum_temp__D_0__D_1__D_2__D_3.LockedTensor(), indices_efmn,
        0.0, E_MP3__D_0__D_1__D_2__D_3.Tensor(), indices_efmn);
    //**** (out of 1)
    //**** Is a shadow
        //------------------------------------//

           // E_MP3[] | {0,1,2,3} <- E_MP3[D0,D1,D2,D3] (with SumScatter on (D0)(D1)(D2)(D3))
        E_MP3____N_D_0_1_2_3.ReduceToOneRedistFrom( E_MP3__D_0__D_1__D_2__D_3, modes_0_1_2_3 );
    //Print(E_MP3____N_D_0_1_2_3, "E_MP3____N_D_0_1_2_3");

        //------------------------------------//
    mpi::Barrier(g.OwningComm());
    runTime = mpi::Time() - startTime;

//    Unsigned flops = ;
//    Unsigned gflops = flops / (1.e9*runTime);
    //****

    //------------------------------------//

//****

//Print(E_MP3____N_D_0_1_2_3, "E_MP3");

  if(commRank == 0){
    std::cout << "SIZE " << five << " " << fiftythree << std::endl;
//        std::cout << "FLOPS " << flops << std::endl;
        std::cout << "TIME " << runTime << std::endl;
//  std::cout << "GFLOPS " << gflops << std::endl;
    std::cout << "GRIDSHAPE";
        Unsigned gridOrder = g.Order();
        for(Unsigned i = 0; i < gridOrder; i++)
        std::cout << " " << g.Dimension(i);
    std::cout << std::endl;
  }

  //if(commRank == 0)
//  std::cout << "commMap size: " << E_MP3____N_D_0_1_2_3.CommMapSize() << std::endl;
  E_MP3____N_D_0_1_2_3.ClearCommMap();
  //if(commRank == 0)
//  std::cout << "commMap size: " << E_MP3____N_D_0_1_2_3.CommMapSize() << std::endl;
  //****

    /*
    DistTensor<T> diffTensor( tmen::StringToTensorDist("[(),(),()]|(0,1,2,3)"), g );
    diffTensor.ResizeTo(C_local);
    Diff( C_local.LockedTensor(), C_local_comparison.LockedTensor(), diffTensor.Tensor() );

    if (commRank == 0) {
      cout << "Norm of distributed is " << Norm(C_local_comparison.LockedTensor()) << endl;
      cout << "Norm of local is " << Norm(C_local.LockedTensor()) << endl;
      cout << "Norm is " << Norm(diffTensor.LockedTensor()) << endl;
    }

    */
}

int
main( int argc, char* argv[] )
{
    Initialize( argc, argv );
    Unsigned i;
    mpi::Comm comm = mpi::COMM_WORLD;
    const Int commRank = mpi::CommRank( comm );
    const Int commSize = mpi::CommSize( comm );
    //printf("My Rank: %d\n", commRank);
    try
    {
        Params args;

        ProcessInput(argc, argv, args);

        if(commRank == 0 && commSize != args.nProcs){
            std::cerr << "program not started with correct number of processes\n";
        std::cerr << commSize << " vs " << args.nProcs << std::endl;
            Usage();
            throw ArgException();
        }
/*
        if(commRank == 0){
            printf("Creating %d", args.gridShape[0]);
            for(i = 1; i < GRIDORDER; i++)
                printf(" x %d", args.gridShape[i]);
            printf(" grid\n");
        }
*/
/*
        if( commRank == 0 )
        {
            std::cout << "------------------" << std::endl
                      << "Testing with doubles:" << std::endl
                      << "------------------" << std::endl;
        }
*/

    Unsigned i, j, k, l, m, n;
    for(i = args.procStart; i < args.procMax; i++){
        Unsigned maxJ = args.procMax - i;
        for(j = args.procStart; j < maxJ; j++){
            Unsigned maxK = maxJ - j;
            for(k = args.procStart; k < maxK; k++){
                l = maxK - k;
                if(i + j + k + l != args.procMax)
                    continue;
                ObjShape gridShape(4);
                gridShape[0] = 1 << i;
                gridShape[1] = 1 << j;
                gridShape[2] = 1 << k;
                gridShape[3] = 1 << l;
                const Grid g( comm, gridShape );
                for(m = args.smallStart; m < args.smallEnd; m += args.smallInc){
                    for(n = args.bigStart; n < args.bigEnd; n += args.bigInc){
                        DistTensorTest<double>(g, m, n);
                    }
                }
            }
        }
    }
    }
    catch( std::exception& e ) { ReportException(e); }

    Finalize();
    //printf("Completed\n");
    return 0;
}
