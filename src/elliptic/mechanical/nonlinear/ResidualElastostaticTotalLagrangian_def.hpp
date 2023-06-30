/*
 * ResidualElastostaticTotalLagrangian_def.hpp
 *
 *  Created on: May 31, 2023
 */

#pragma once

#include "MetaData.hpp"
#include "GradientMatrix.hpp"
#include "elliptic/mechanical/nonlinear/StateGradient.hpp"
#include "elliptic/mechanical/nonlinear/DeformationGradient.hpp"
#include "elliptic/mechanical/nonlinear/FactoryStressEvaluator.hpp"

namespace Plato
{

namespace Elliptic
{

template<typename EvaluationType>
ResidualElastostaticTotalLagrangian<EvaluationType>::
ResidualElastostaticTotalLagrangian(
  const Plato::SpatialDomain   & aSpatialDomain,
        Plato::DataMap         & aDataMap,
        Teuchos::ParameterList & aParamList
) :
  FunctionBaseType(aSpatialDomain, aDataMap),
  mStressEvaluator(nullptr),
  mNeumannBCs     (nullptr),
  mBodyLoads      (nullptr)
{
  // obligatory: define dof names in order
  //
  mDofNames.push_back("displacement X");
  if(mNumSpatialDims > 1) mDofNames.push_back("displacement Y");
  if(mNumSpatialDims > 2) mDofNames.push_back("displacement Z");
  // initialize member data
  //
  this->initialize(aParamList);
}

template<typename EvaluationType>
void
ResidualElastostaticTotalLagrangian<EvaluationType>::
postProcess(
  const Plato::Solutions & aSolutions
)
{ return; }

template<typename EvaluationType>
void
ResidualElastostaticTotalLagrangian<EvaluationType>::
evaluate(
  Plato::WorkSets & aWorkSets,
  Plato::Scalar     aCycle
) const
{
  // unpack worksets
  Plato::ScalarArray3DT<ConfigScalarType> tConfigWS  = 
    Plato::unpack<Plato::ScalarArray3DT<ConfigScalarType>>(aWorkSets.get("configuration"));
  Plato::ScalarMultiVectorT<ControlScalarType> tControlWS = 
    Plato::unpack<Plato::ScalarMultiVectorT<ControlScalarType>>(aWorkSets.get("controls"));
  Plato::ScalarMultiVectorT<StateScalarType> tStateWS = 
    Plato::unpack<Plato::ScalarMultiVectorT<StateScalarType>>(aWorkSets.get("states"));
  Plato::ScalarMultiVectorT<ResultScalarType> tResultWS = 
    Plato::unpack<Plato::ScalarMultiVectorT<ResultScalarType>>(aWorkSets.get("result"));
  // evaluate second piola-kirchhoff stress tensor
  Plato::OrdinalType tNumCells = mSpatialDomain.numCells();
  Plato::OrdinalType tNumGaussPoints = ElementType::mNumGaussPoints;
  Plato::ScalarArray4DT<ResultScalarType> tSecondPiolaKirchhoffStress(
    "2nd Piola-Kirchhoff Stress",tNumCells,tNumGaussPoints,mNumSpatialDims,mNumSpatialDims
  );
  mStressEvaluator->evaluate(aWorkSets,tSecondPiolaKirchhoffStress,aCycle);
  // get integration rule data
  auto tCubPoints  = ElementType::getCubPoints();
  auto tCubWeights = ElementType::getCubWeights();
  // evaluate internal forces
  Plato::StateGradient<EvaluationType> tComputeStateGradient;
  Plato::ComputeGradientMatrix<ElementType> tComputeGradient;
  Plato::DeformationGradient<EvaluationType> tComputeDeformationGradient;
  Kokkos::parallel_for("compute internal forces", 
    Kokkos::MDRangePolicy<Kokkos::Rank<2>>({0,0}, {tNumCells,mNumGaussPoints}),
    KOKKOS_LAMBDA(const Plato::OrdinalType iCellOrdinal, const Plato::OrdinalType iGpOrdinal)
  {
    // compute gradient of interpolation functions
    ConfigScalarType tVolume(0.0);
    auto tCubPoint = tCubPoints(iGpOrdinal);
    Plato::Matrix<mNumNodesPerCell,mNumSpatialDims,ConfigScalarType> tGradient;
    tComputeGradient(iCellOrdinal,tCubPoint,tConfigWS,tGradient,tVolume);
    // compute state gradient
    Plato::Matrix<ElementType::mNumSpatialDims,ElementType::mNumSpatialDims,StrainScalarType> 
      tStateGradient(StrainScalarType(0.));
    tComputeStateGradient(iCellOrdinal,tStateWS,tGradient,tStateGradient);
    // compute deformation gradient 
    Plato::Matrix<ElementType::mNumSpatialDims,ElementType::mNumSpatialDims,StrainScalarType> 
      tDefGradient(StrainScalarType(0.));
    tComputeDeformationGradient(tStateGradient,tDefGradient);
    // compute nominal stress
    Plato::Matrix<mNumNodesPerCell,mNumSpatialDims,ResultScalarType> 
      tNominalStress(ResultScalarType(0.));
    for(Plato::OrdinalType tDimI = 0; tDimI < ElementType::mNumSpatialDims; tDimI++){
      for(Plato::OrdinalType tDimJ = 0; tDimJ < ElementType::mNumSpatialDims; tDimJ++){
        for(Plato::OrdinalType tDimK = 0; tDimK < ElementType::mNumSpatialDims; tDimK++){
          tNominalStress(tDimI,tDimJ) += tSecondPiolaKirchhoffStress(iCellOrdinal,iGpOrdinal,tDimI,tDimK) 
            * tDefGradient(tDimJ,tDimK);
        }
      }
    }
    // apply integration point weight to element volume
    tVolume *= tCubWeights(iGpOrdinal);
    // apply divergence operator to stress tensor
    for(Plato::OrdinalType tNodeIndex = 0; tNodeIndex < mNumNodesPerCell; tNodeIndex++){
      for(Plato::OrdinalType tDimI = 0; tDimI < mNumSpatialDims; tDimI++){
        Plato::OrdinalType tLocalOrdinal = tNodeIndex * mNumSpatialDims + tDimI;
        for(Plato::OrdinalType tDimJ = 0; tDimJ < mNumSpatialDims; tDimJ++){
          ResultScalarType tVal = tNominalStress(tDimI,tDimJ) * tGradient(tNodeIndex,tDimJ) * tVolume;
          Kokkos::atomic_add( &tResultWS(iCellOrdinal,tLocalOrdinal),tVal );
        }
      }
    }
  });
  // evaluate body forces
  if( mBodyLoads != nullptr )
  {
    mBodyLoads->get( mSpatialDomain,tStateWS,tControlWS,tConfigWS,tResultWS,-1.0 );
  }
}

template<typename EvaluationType>
void
ResidualElastostaticTotalLagrangian<EvaluationType>::
evaluateBoundary(
  const Plato::SpatialModel & aSpatialModel,
        Plato::WorkSets     & aWorkSets,
        Plato::Scalar         aCycle
) const
{
  // unpack worksets
  Plato::ScalarArray3DT<ConfigScalarType> tConfigWS  = 
    Plato::unpack<Plato::ScalarArray3DT<ConfigScalarType>>(aWorkSets.get("configuration"));
  Plato::ScalarMultiVectorT<ControlScalarType> tControlWS = 
    Plato::unpack<Plato::ScalarMultiVectorT<ControlScalarType>>(aWorkSets.get("controls"));
  Plato::ScalarMultiVectorT<StateScalarType> tStateWS = 
    Plato::unpack<Plato::ScalarMultiVectorT<StateScalarType>>(aWorkSets.get("states"));
  Plato::ScalarMultiVectorT<ResultScalarType> tResultWS = 
    Plato::unpack<Plato::ScalarMultiVectorT<ResultScalarType>>(aWorkSets.get("result"));
  // evaluate boundary forces
  if( mNeumannBCs != nullptr )
  {
    mNeumannBCs->get(aSpatialModel, tStateWS, tControlWS, tConfigWS, tResultWS, -1.0 );
  }
}

template<typename EvaluationType>
void 
ResidualElastostaticTotalLagrangian<EvaluationType>::
initialize(
  Teuchos::ParameterList & aParamList
)
{
  // create material model and get stiffness
  //
  Plato::FactoryStressEvaluator<EvaluationType> tStressEvaluatorFactory(mSpatialDomain.getMaterialName());
  mStressEvaluator = tStressEvaluatorFactory.create(aParamList,mSpatialDomain,mDataMap);
  // parse body loads
  // 
  if(aParamList.isSublist("Body Loads"))
  {
    mBodyLoads = 
      std::make_shared<Plato::BodyLoads<EvaluationType, ElementType>>(aParamList.sublist("Body Loads"));
  }
  // parse boundary Conditions
  // 
  if(aParamList.isSublist("Natural Boundary Conditions"))
  {
    mNeumannBCs = 
      std::make_shared<Plato::NeumannBCs<ElementType>>(aParamList.sublist("Natural Boundary Conditions"));
  }
  // parse plot table
  //
  auto tResidualParams = aParamList.sublist("Output");
  if( tResidualParams.isType<Teuchos::Array<std::string>>("Plottable") )
  {
    mPlotTable = tResidualParams.get<Teuchos::Array<std::string>>("Plottable").toVector();
  }
}

} // namespace Elliptic

} // namespace Plato
