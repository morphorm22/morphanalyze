#pragma once

#include "ToMap.hpp"
#include "TMKinetics.hpp"
#include "TMKinematics.hpp"
#include "ProjectToNode.hpp"
#include "ThermalContent.hpp"
#include "GradientMatrix.hpp"
#include "PlatoMathHelpers.hpp"
#include "ThermalMassMaterial.hpp"
#include "InterpolateFromNodal.hpp"
#include "GeneralFluxDivergence.hpp"
#include "ThermoelasticMaterial.hpp"
#include "GeneralStressDivergence.hpp"

namespace Plato
{

namespace Parabolic
{

    /**************************************************************************/
    template<typename EvaluationType, typename IndicatorFunctionType>
    TransientThermomechResidual<EvaluationType, IndicatorFunctionType>::
    TransientThermomechResidual(
        const Plato::SpatialDomain   & aSpatialDomain,
              Plato::DataMap         & aDataMap,
              Teuchos::ParameterList & aProblemParams,
              Teuchos::ParameterList & aPenaltyParams
     ) :
         FunctionBaseType      (aSpatialDomain, aDataMap),
         mIndicatorFunction    (aPenaltyParams),
         mApplyStressWeighting (mIndicatorFunction),
         mApplyFluxWeighting   (mIndicatorFunction),
         mApplyMassWeighting   (mIndicatorFunction),
         mBoundaryLoads        (nullptr),
         mBoundaryFluxes       (nullptr)
    /**************************************************************************/
    {
        // obligatory: define dof names in order
        mDofNames.push_back("displacement X");
        mDofDotNames.push_back("velocity X");
        if(mNumSpatialDims > 1)
        {
            mDofNames.push_back("displacement Y");
            mDofDotNames.push_back("velocity Y");
        }
        if(mNumSpatialDims > 2)
        {
            mDofNames.push_back("displacement Z");
            mDofDotNames.push_back("velocity Z");
        }
        mDofNames.push_back("temperature");
        mDofDotNames.push_back("temperature rate");

        {
            Plato::ThermoelasticModelFactory<mNumSpatialDims> mmfactory(aProblemParams);
            mMaterialModel = mmfactory.create(aSpatialDomain.getMaterialName());
        }

        {
            Plato::ThermalMassModelFactory<mNumSpatialDims> mmfactory(aProblemParams);
            mThermalMassMaterialModel = mmfactory.create(aSpatialDomain.getMaterialName());
        }

        // parse boundary Conditions
        // 
        if(aProblemParams.isSublist("Mechanical Natural Boundary Conditions"))
        {
            mBoundaryLoads = std::make_shared<Plato::NaturalBCs<ElementType, NMechDims, mNumDofsPerNode, MDofOffset>>
                (aProblemParams.sublist("Mechanical Natural Boundary Conditions"));
        }
        // parse thermal boundary Conditions
        // 
        if(aProblemParams.isSublist("Thermal Natural Boundary Conditions"))
        {
            mBoundaryFluxes = std::make_shared<Plato::NaturalBCs<ElementType, NThrmDims, mNumDofsPerNode, TDofOffset>>
                (aProblemParams.sublist("Thermal Natural Boundary Conditions"));
        }

        auto tResidualParams = aProblemParams.sublist("Parabolic");
        if( tResidualParams.isType<Teuchos::Array<std::string>>("Plottable") )
        {
            mPlottable = tResidualParams.get<Teuchos::Array<std::string>>("Plottable").toVector();
        }
    }


    template<typename EvaluationType, typename IndicatorFunctionType>
    Plato::Solutions
    TransientThermomechResidual<EvaluationType, IndicatorFunctionType>::
    getSolutionStateOutputData(const Plato::Solutions &aSolutions) const
    {
      return aSolutions;
    }


    /**************************************************************************/
    template<typename EvaluationType, typename IndicatorFunctionType>
    void
    TransientThermomechResidual<EvaluationType, IndicatorFunctionType>::
    evaluate(
        const Plato::ScalarMultiVectorT< StateScalarType    > & aState,
        const Plato::ScalarMultiVectorT< StateDotScalarType > & aStateDot,
        const Plato::ScalarMultiVectorT< ControlScalarType  > & aControl,
        const Plato::ScalarArray3DT    < ConfigScalarType   > & aConfig,
              Plato::ScalarMultiVectorT< ResultScalarType   > & aResult,
              Plato::Scalar aTimeStep) const
    /**************************************************************************/
    {
      auto tNumCells = mSpatialDomain.numCells();

      using GradScalarType = typename Plato::fad_type_t<ElementType, StateScalarType, ConfigScalarType>;

      Plato::ScalarVectorT<ConfigScalarType> tCellVolume("cell weight",tNumCells);

      Plato::ScalarMultiVectorT<GradScalarType> tCellStrain("strain", tNumCells, mNumVoigtTerms);
      Plato::ScalarMultiVectorT<GradScalarType> tCellTgrad("tgrad", tNumCells, mNumSpatialDims);

      Plato::ScalarMultiVectorT<ResultScalarType> tCellStress("stress", tNumCells, mNumVoigtTerms);
      Plato::ScalarMultiVectorT<ResultScalarType> tCellFlux("flux" , tNumCells, mNumSpatialDims);

      Plato::ComputeGradientMatrix<ElementType> tComputeGradient;

      Plato::TMKinematics<ElementType> tKinematics;
      Plato::TMKinetics<ElementType>   tKinetics(mMaterialModel);

      Plato::InterpolateFromNodal<ElementType, mNumDofsPerNode, TDofOffset> tInterpolateFromNodal;

      Plato::GeneralFluxDivergence  <ElementType, mNumDofsPerNode, TDofOffset> tFluxDivergence;
      Plato::GeneralStressDivergence<ElementType, mNumDofsPerNode, MDofOffset> tStressDivergence;

      Plato::ThermalContent<mNumSpatialDims> tComputeHeatRate(mThermalMassMaterialModel);

      Plato::ProjectToNode<ElementType, mNumDofsPerNode, TDofOffset> tProjectHeatRate;

      auto tCubPoints = ElementType::getCubPoints();
      auto tCubWeights = ElementType::getCubWeights();
      auto tNumPoints = tCubWeights.size();
      
      auto& tApplyStressWeighting = mApplyStressWeighting;
      auto& tApplyFluxWeighting   = mApplyFluxWeighting;
      auto& tApplyMassWeighting   = mApplyMassWeighting;
      Kokkos::parallel_for("stress and flux divergence", Kokkos::MDRangePolicy<Kokkos::Rank<2>>({0, 0}, {tNumCells, tNumPoints}),
      KOKKOS_LAMBDA(const Plato::OrdinalType iCellOrdinal, const Plato::OrdinalType iGpOrdinal)
      {
    
          ConfigScalarType tVolume(0.0);

          Plato::Matrix<ElementType::mNumNodesPerCell, ElementType::mNumSpatialDims, ConfigScalarType> tGradient;

          Plato::Array<ElementType::mNumVoigtTerms,  GradScalarType>   tStrain(0.0);
          Plato::Array<ElementType::mNumSpatialDims, GradScalarType>   tTGrad (0.0);
          Plato::Array<ElementType::mNumVoigtTerms,  ResultScalarType> tStress(0.0);
          Plato::Array<ElementType::mNumSpatialDims, ResultScalarType> tFlux  (0.0);

          auto tCubPoint = tCubPoints(iGpOrdinal);
          auto tBasisValues = ElementType::basisValues(tCubPoint);

          tComputeGradient(iCellOrdinal, tCubPoint, aConfig, tGradient, tVolume);
          tVolume *= tCubWeights(iGpOrdinal);
    
          // compute strain and temperature gradient
          //
          tKinematics(iCellOrdinal, tStrain, tTGrad, aState, tGradient);

          // compute stress and thermal flux
          //
          StateScalarType tTemperature = tInterpolateFromNodal(iCellOrdinal, tBasisValues, aState);
          tKinetics(tStress, tFlux, tStrain, tTGrad, tTemperature);

          // apply weighting
          //
          tApplyStressWeighting(iCellOrdinal, aControl, tBasisValues, tStress);
          tApplyFluxWeighting  (iCellOrdinal, aControl, tBasisValues, tFlux);

          // compute stress and flux divergence
          //
          tStressDivergence(iCellOrdinal, aResult, tStress, tGradient, tVolume);
          tFluxDivergence  (iCellOrdinal, aResult, tFlux,   tGradient, tVolume);

          // compute temperature rate at gausspoints
          //
          StateDotScalarType tTemperatureRate = tInterpolateFromNodal(iCellOrdinal, tBasisValues, aStateDot);

          // compute the time rate of internal thermal energy
          //
          ResultScalarType tHeatRate(0.0);
          tComputeHeatRate(tHeatRate, tTemperatureRate, tTemperature);

          // apply weighting
          //
          tApplyMassWeighting(iCellOrdinal, aControl, tBasisValues, tHeatRate);

          // project to nodes
          //
          tProjectHeatRate(iCellOrdinal, tVolume, tBasisValues, tHeatRate, aResult);

          for(int i=0; i<ElementType::mNumVoigtTerms; i++)
          {
              Kokkos::atomic_add(&tCellStrain(iCellOrdinal,i), tVolume*tStrain(i));
              Kokkos::atomic_add(&tCellStress(iCellOrdinal,i), tVolume*tStress(i));
          }
          for(int i=0; i<ElementType::mNumSpatialDims; i++)
          {
              Kokkos::atomic_add(&tCellTgrad(iCellOrdinal,i), tVolume*tTGrad(i));
              Kokkos::atomic_add(&tCellFlux(iCellOrdinal,i), tVolume*tFlux(i));
          }
          Kokkos::atomic_add(&tCellVolume(iCellOrdinal), tVolume);

      });

      Kokkos::parallel_for("compute cell quantities", Kokkos::RangePolicy<>(0, tNumCells),
      KOKKOS_LAMBDA(const Plato::OrdinalType iCellOrdinal)
      {
          for(int i=0; i<ElementType::mNumVoigtTerms; i++)
          {
              tCellStrain(iCellOrdinal,i) /= tCellVolume(iCellOrdinal);
              tCellStress(iCellOrdinal,i) /= tCellVolume(iCellOrdinal);
          }
          for(int i=0; i<ElementType::mNumSpatialDims; i++)
          {
              tCellTgrad(iCellOrdinal,i) /= tCellVolume(iCellOrdinal);
              tCellFlux(iCellOrdinal,i) /= tCellVolume(iCellOrdinal);
          }
      });

      if( std::count(mPlottable.begin(),mPlottable.end(),"strain") ) toMap(mDataMap, tCellStrain, "strain", mSpatialDomain);
      if( std::count(mPlottable.begin(),mPlottable.end(),"tgrad" ) ) toMap(mDataMap, tCellTgrad,  "tgrad",  mSpatialDomain);
      if( std::count(mPlottable.begin(),mPlottable.end(),"stress") ) toMap(mDataMap, tCellStress, "stress", mSpatialDomain);
      if( std::count(mPlottable.begin(),mPlottable.end(),"flux"  ) ) toMap(mDataMap, tCellFlux,   "flux" ,  mSpatialDomain);
    }

    /**************************************************************************/
    template<typename EvaluationType, typename IndicatorFunctionType>
    void
    TransientThermomechResidual<EvaluationType, IndicatorFunctionType>::
    evaluate_boundary(
        const Plato::SpatialModel                             & aSpatialModel,
        const Plato::ScalarMultiVectorT< StateScalarType    > & aState,
        const Plato::ScalarMultiVectorT< StateDotScalarType > & aStateDot,
        const Plato::ScalarMultiVectorT< ControlScalarType  > & aControl,
        const Plato::ScalarArray3DT    < ConfigScalarType   > & aConfig,
              Plato::ScalarMultiVectorT< ResultScalarType   > & aResult,
              Plato::Scalar aTimeStep) const
    /**************************************************************************/
    {
        if( mBoundaryLoads != nullptr )
        {
            mBoundaryLoads->get(aSpatialModel, aState, aControl, aConfig, aResult, -1.0);
        }
        if( mBoundaryFluxes != nullptr )
        {
            mBoundaryFluxes->get(aSpatialModel, aState, aControl, aConfig, aResult, -1.0);
        }
    }
} // namespace Parabolic

} // namespace Plato
