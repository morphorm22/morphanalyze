#pragma once

#include "ToMap.hpp"
#include "MetaData.hpp"
#include "ScalarGrad.hpp"
#include "ThermalFlux.hpp"
#include "GradientMatrix.hpp"
#include "InterpolateFromNodal.hpp"
#include "GeneralFluxDivergence.hpp"

namespace Plato
{

namespace Elliptic
{

template<typename EvaluationType, typename IndicatorFunctionType>
ResidualThermostatic<EvaluationType, IndicatorFunctionType>::
ResidualThermostatic(
  const Plato::SpatialDomain   & aSpatialDomain,
        Plato::DataMap         & aDataMap,
        Teuchos::ParameterList & aProblemParams,
        Teuchos::ParameterList & penaltyParams
) :
  FunctionBaseType   (aSpatialDomain, aDataMap),
  mIndicatorFunction (penaltyParams),
  mApplyWeighting    (mIndicatorFunction),
  mBodyLoads         (nullptr),
  mBoundaryLoads     (nullptr)
{
  // obligatory: define dof names in order
  mDofNames.push_back("temperature");
  Plato::ThermalConductionModelFactory<EvaluationType> tMaterialFactory(aProblemParams);
  mMaterialModel = tMaterialFactory.create(aSpatialDomain.getMaterialName());
  // parse body loads
  // 
  if(aProblemParams.isSublist("Body Loads"))
  {
    mBodyLoads = std::make_shared<Plato::BodyLoads<EvaluationType, ElementType>>(
      aProblemParams.sublist("Body Loads")
    );
  }
  // parse boundary Conditions
  // 
  if(aProblemParams.isSublist("Natural Boundary Conditions"))
  {
    mBoundaryLoads = std::make_shared<Plato::NaturalBCs<ElementType, mNumDofsPerNode>>(
      aProblemParams.sublist("Natural Boundary Conditions")
    );
  }
  auto tResidualParams = aProblemParams.sublist("Elliptic");
  if( tResidualParams.isType<Teuchos::Array<std::string>>("Plottable") )
  {
    mPlottable = tResidualParams.get<Teuchos::Array<std::string>>("Plottable").toVector();
  }
}

template<typename EvaluationType, typename IndicatorFunctionType>
Plato::Solutions 
ResidualThermostatic<EvaluationType, IndicatorFunctionType>::
getSolutionStateOutputData(
  const Plato::Solutions &aSolutions) 
const 
{
  return aSolutions;
}

template<typename EvaluationType, typename IndicatorFunctionType>
void
ResidualThermostatic<EvaluationType, IndicatorFunctionType>::
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
  // create local functors
  Plato::ComputeGradientMatrix<ElementType>  tComputeGradient;
  Plato::ScalarGrad<ElementType>             tScalarGrad;
  Plato::GeneralFluxDivergence<ElementType>  tFluxDivergence;
  Plato::ThermalFlux<EvaluationType>         tThermalFlux(mMaterialModel);
  Plato::InterpolateFromNodal<ElementType, mNumDofsPerNode> tInterpolateFromNodal;
  // create temporary containers
  auto tNumCells = mSpatialDomain.numCells();
  Plato::ScalarVectorT<ConfigScalarType>      tCellVolume("cell weight",tNumCells);
  Plato::ScalarMultiVectorT<GradScalarType>   tCellGrad("temperature gradient", tNumCells, mNumSpatialDims);
  Plato::ScalarMultiVectorT<ResultScalarType> tCellFlux("thermal flux", tNumCells, mNumSpatialDims);
  // get interpolation rule
  auto tNumPoints  = mNumGaussPoints;
  auto tCubPoints  = ElementType::getCubPoints();
  auto tCubWeights = ElementType::getCubWeights();

  auto& tApplyWeighting = mApplyWeighting;
  Kokkos::parallel_for("compute stress", 
    Kokkos::MDRangePolicy<Kokkos::Rank<2>>({0, 0}, {tNumCells, tNumPoints}),
    KOKKOS_LAMBDA(const Plato::OrdinalType iCellOrdinal, const Plato::OrdinalType iGpOrdinal)
  {
    // compute gradient of interpolation functions
    ConfigScalarType tVolume(0.0);
    Plato::Matrix<mNumNodesPerCell, mNumSpatialDims, ConfigScalarType> tGradient;
    Plato::Array<mNumSpatialDims, GradScalarType> tGrad(0.0);
    Plato::Array<mNumSpatialDims, ResultScalarType> tFlux(0.0);
    auto tCubPoint = tCubPoints(iGpOrdinal);
    auto tBasisValues = ElementType::basisValues(tCubPoint);
    tComputeGradient(iCellOrdinal, tCubPoint, tConfigWS, tGradient, tVolume);
    // compute temperature gradient and interpolate temperature to integration points
    tScalarGrad(iCellOrdinal, tGrad, tStateWS, tGradient);
    StateScalarType tTemperature = tInterpolateFromNodal(iCellOrdinal, tBasisValues, tStateWS);
    // compute penalized thermal flux
    tThermalFlux(tFlux, tGrad, tTemperature);
    tVolume *= tCubWeights(iGpOrdinal);
    tApplyWeighting(iCellOrdinal, tControlWS, tBasisValues, tFlux);
    // applied divergence operator to thermal flux
    tFluxDivergence(iCellOrdinal, tResultWS, tFlux, tGradient, tVolume, -1.0);
    // compute element quantity of interests
    for(int i=0; i<mNumSpatialDims; i++)
    {
      Kokkos::atomic_add(&tCellGrad(iCellOrdinal,i), tVolume*tGrad(i));
      Kokkos::atomic_add(&tCellFlux(iCellOrdinal,i), tVolume*tFlux(i));
    }
    Kokkos::atomic_add(&tCellVolume(iCellOrdinal), tVolume);
  });
  // compute output element quantities of interests
  Kokkos::parallel_for("compute cell quantities", 
    Kokkos::RangePolicy<>(0, tNumCells),
    KOKKOS_LAMBDA(const Plato::OrdinalType iCellOrdinal)
  {
    for(int i=0; i<mNumSpatialDims; i++)
    {
      tCellGrad(iCellOrdinal,i) /= tCellVolume(iCellOrdinal);
      tCellFlux(iCellOrdinal,i) /= tCellVolume(iCellOrdinal);
    }
  });
  // evaluate body forces
  if( mBodyLoads != nullptr )
  {
    mBodyLoads->get( mSpatialDomain, tStateWS, tControlWS, tConfigWS, tResultWS, -1.0 );
  }
  // save output quantities of interests
  if( std::count(mPlottable.begin(),mPlottable.end(),"tgrad") ) 
    { toMap(mDataMap, tCellGrad, "tgrad", mSpatialDomain); }
  if( std::count(mPlottable.begin(),mPlottable.end(),"flux" ) ) 
    { toMap(mDataMap, tCellFlux, "flux" , mSpatialDomain); }
}

template<typename EvaluationType, typename IndicatorFunctionType>
void
ResidualThermostatic<EvaluationType, IndicatorFunctionType>::
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
  if( mBoundaryLoads != nullptr )
  {
    mBoundaryLoads->get(aSpatialModel,tStateWS,tControlWS,tConfigWS,tResultWS,1.0);
  }
}

} // namespace Elliptic

} // namespace Plato
