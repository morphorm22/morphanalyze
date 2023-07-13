/*
 * BoundaryEvaluatorTestIsotropicElasticStress_def.hpp
 *
 *  Created on: July 13, 2023
 */

#pragma once

#include "MetaData.hpp"
#include "GradientMatrix.hpp"
#include "PlatoMathTypes.hpp"

#include "materials/mechanical/FactoryMechanicalMaterials.hpp"
#include "elliptic/mechanical/linear/ComputeStrainTensor.hpp"
#include "elliptic/mechanical/linear/ComputeIsotropicElasticStressTensor.hpp"

namespace Plato
{

namespace Elliptic
{

template<typename EvaluationType>
BoundaryEvaluatorTestIsotropicElasticStress<EvaluationType>::
BoundaryEvaluatorTestIsotropicElasticStress(
  Teuchos::ParameterList & aParamList,
  Teuchos::ParameterList & aNitscheParams
) : 
  BaseClassType(aNitscheParams)
{
  Plato::FactoryMechanicalMaterials<EvaluationType> tFactory;
  mMaterialModel = tFactory.create(mMaterialName,aParamList);
}

template<typename EvaluationType>
void 
BoundaryEvaluatorTestIsotropicElasticStress<EvaluationType>::
evaluate(
  const Plato::SpatialModel                     & aSpatialModel,
  const Plato::WorkSets                         & aWorkSets,
        Plato::ScalarArray4DT<ResultScalarType> & aResult,
        Plato::Scalar                             aCycle
) const
{
  // unpack worksets
  //
  Plato::ScalarArray3DT<ConfigScalarType> tConfigWS  = 
    Plato::unpack<Plato::ScalarArray3DT<ConfigScalarType>>(aWorkSets.get("configuration"));
  // get side set connectivity information
  //
  auto tSideCellOrdinals  = aSpatialModel.Mesh->GetSideSetElements(mSideSetName);
  auto tSideLocalFaceOrds = aSpatialModel.Mesh->GetSideSetFaces(mSideSetName);
  // create local functors
  //
  Plato::ComputeGradientMatrix<BodyElementBase> tComputeGradient;
  Plato::ComputeStrainTensor<EvaluationType> tComputeStrainTensor;
  Plato::ComputeIsotropicElasticStressTensor<EvaluationType> tComputeStressTensor(mMaterialModel.operator*());
  // get integration points and weights
  //
  auto tCubPointsOnParentFaceElem = FaceElementBase::getCubPoints();
  auto tCubPointsOnParentBodyElemSurfaces = BodyElementBase::getFaceCubPoints();
  auto tCubWeightsOnParentBodyElemSurface = BodyElementBase::getFaceCubWeights();
  // evaluate integral
  //
  Plato::OrdinalType tNumCellsOnSideSet = tSideCellOrdinals.size();
  Kokkos::parallel_for("boundary test stress evaluator", 
    Kokkos::MDRangePolicy<Kokkos::Rank<2>>({0,0},{tNumCellsOnSideSet, mNumGaussPointsPerFace}),
    KOKKOS_LAMBDA(const Plato::OrdinalType & aSideOrdinal, const Plato::OrdinalType & aPointOrdinal)
  {
    // quadrature data to evaluate integral on the body surface of interest
    Plato::Array<mNumSpatialDims> tCubPointOnParentBodyElemSurface;
    Plato::OrdinalType tLocalFaceOrdinal = tSideLocalFaceOrds(aSideOrdinal);
    auto tCubPointsOnBodyParentElemSurface = tCubPointsOnParentBodyElemSurfaces(tLocalFaceOrdinal);
    for( Plato::OrdinalType tDim=0; tDim < mNumSpatialDims; tDim++ ){
      Plato::OrdinalType tIndex = BodyElementBase::mNumGaussPointsPerFace * aPointOrdinal + tDim;
      tCubPointOnParentBodyElemSurface(tDim) = tCubPointsOnBodyParentElemSurface(tIndex);
    }
    // compute trial stress tensor
    //
    ConfigScalarType tVolume(0.0);
    auto tCellOrdinal = tSideCellOrdinals(aSideOrdinal);
    Plato::Matrix<mNumNodesPerCell,mNumSpatialDims, ConfigScalarType> tGradient;
    tComputeGradient(tCellOrdinal,tCubPointOnParentBodyElemSurface,tConfigWS,tGradient,tVolume);
    Plato::Matrix<mNumSpatialDims,mNumSpatialDims, ConfigScalarType>  tVirtualStrainTensor(0.0);
    tComputeStrainTensor(tCellOrdinal,tGradient,tVirtualStrainTensor);
    Plato::Matrix<mNumSpatialDims,mNumSpatialDims, ConfigScalarType>  tVirtualStressTensor(0.0);
    tComputeStressTensor(tVirtualStrainTensor,tVirtualStressTensor);
    // copy stress tensor to output workset
    //
    for(Plato::OrdinalType tDimI = 0; tDimI < mNumSpatialDims; tDimI++){
      for(Plato::OrdinalType tDimJ = 0; tDimJ < mNumSpatialDims; tDimJ++){
        aResult(tCellOrdinal,aPointOrdinal,tDimI,tDimJ) = tVirtualStressTensor(tDimI,tDimJ);
      }
    }
  });
}

} // namespace Elliptic

} // namespace Plato
