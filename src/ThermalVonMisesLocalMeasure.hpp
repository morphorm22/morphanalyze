#pragma once

#include "AbstractLocalMeasure.hpp"
#include "VonMisesYieldFunction.hpp"
#include "ImplicitFunctors.hpp"
#include <Teuchos_ParameterList.hpp>
#include "ExpInstMacros.hpp"
#include "TMKinematics.hpp"
#include "TMKinetics.hpp"
#include "InterpolateFromNodal.hpp"
#include "ThermoelasticMaterial.hpp"

namespace Plato
{

/******************************************************************************//**
 * \brief VonMises local measure class for use in Augmented Lagrange constraint formulation
 * \tparam EvaluationType evaluation type use to determine automatic differentiation
 *   type for scalar function (e.g. Residual, Jacobian, GradientZ, etc.)
**********************************************************************************/
template<typename EvaluationType>
class ThermalVonMisesLocalMeasure :
        public AbstractLocalMeasure<EvaluationType>
{
private:
    using ElementType = typename EvaluationType::ElementType;

    using AbstractLocalMeasure<EvaluationType>::mNumSpatialDims;
    using AbstractLocalMeasure<EvaluationType>::mNumVoigtTerms;
    using AbstractLocalMeasure<EvaluationType>::mNumNodesPerCell;
    using AbstractLocalMeasure<EvaluationType>::mSpatialDomain; 

    using StateT = typename EvaluationType::StateScalarType;
    using ConfigT = typename EvaluationType::ConfigScalarType;
    using ResultT = typename EvaluationType::ResultScalarType;

    Teuchos::RCP<Plato::MaterialModel<mNumSpatialDims>> mMaterialModel;

    static constexpr Plato::OrdinalType TDofOffset = mNumSpatialDims;

    static constexpr Plato::OrdinalType mNumDofsPerNode = ElementType::mNumDofsPerNode;

public:
    /******************************************************************************//**
     * \brief Primary constructor
     * \param [in] aInputParams input parameters database
     * \param [in] aName local measure name
     **********************************************************************************/
    ThermalVonMisesLocalMeasure(
        const Plato::SpatialDomain   & aSpatialDomain,
              Teuchos::ParameterList & aInputParams,
        const std::string            & aName
    ) : 
        AbstractLocalMeasure<EvaluationType>(aSpatialDomain, aInputParams, aName)
    {
        Plato::ThermoelasticModelFactory<mNumSpatialDims> tFactory(aInputParams);
        mMaterialModel = tFactory.create(mSpatialDomain.getMaterialName());
    }


    /******************************************************************************//**
     * \brief Destructor
     **********************************************************************************/
    virtual ~ThermalVonMisesLocalMeasure()
    {
    }

    /******************************************************************************//**
     * \brief Evaluate vonmises local measure
     * \param [in] aState 2D container of state variables
     * \param [in] aConfig 3D container of configuration/coordinates
     * \param [in] aDataMap map to stored data
     * \param [out] aResult 1D container of cell local measure values
    **********************************************************************************/
    virtual void
    operator()(
        const Plato::ScalarMultiVectorT <StateT>  & aStateWS,
        const Plato::ScalarArray3DT     <ConfigT> & aConfigWS,
              Plato::ScalarVectorT      <ResultT> & aResultWS
    ) override
    {
        using StrainT = typename Plato::fad_type_t<ElementType, StateT, ConfigT>;

        const Plato::OrdinalType tNumCells = aResultWS.size();

        Plato::VonMisesYieldFunction<mNumSpatialDims, mNumVoigtTerms> tComputeVonMises;

        Plato::ComputeGradientMatrix<ElementType> tComputeGradient;
        Plato::TMKinematics<ElementType>          tKinematics;
        Plato::TMKinetics<ElementType>            tKinetics(mMaterialModel);

        Plato::InterpolateFromNodal<ElementType, mNumDofsPerNode, TDofOffset> tInterpolateFromNodal;

        Plato::ScalarVectorT<ConfigT> tCellVolume("volume", tNumCells);

        auto tCubPoints = ElementType::getCubPoints();
        auto tCubWeights = ElementType::getCubWeights();
        auto tNumPoints = tCubWeights.size();

        Kokkos::parallel_for("compute element state", Kokkos::MDRangePolicy<Kokkos::Rank<2>>({0, 0}, {tNumCells, tNumPoints}),
        LAMBDA_EXPRESSION(const Plato::OrdinalType iCellOrdinal, const Plato::OrdinalType iGpOrdinal)
        {
            ConfigT tVolume(0.0);

            Plato::Matrix<ElementType::mNumNodesPerCell, ElementType::mNumSpatialDims, ConfigT> tGradient;

            Plato::Array<ElementType::mNumVoigtTerms,  StrainT> tStrain(0.0);
            Plato::Array<ElementType::mNumSpatialDims, StrainT> tTGrad (0.0);
            Plato::Array<ElementType::mNumVoigtTerms,  ResultT> tStress(0.0);
            Plato::Array<ElementType::mNumSpatialDims, ResultT> tFlux  (0.0);

            auto tCubPoint = tCubPoints(iGpOrdinal);

            tComputeGradient(iCellOrdinal, tCubPoint, aConfigWS, tGradient, tVolume);

            tVolume *= tCubWeights(iGpOrdinal);

            // compute strain and electric field
            //
            tKinematics(iCellOrdinal, tStrain, tTGrad, aStateWS, tGradient);

            // compute stress and electric displacement
            //
            StateT tTemperature(0.0);
            auto tBasisValues = ElementType::basisValues(tCubPoint);
            tInterpolateFromNodal(iCellOrdinal, tBasisValues, aStateWS, tTemperature);
            tKinetics(tStress, tFlux, tStrain, tTGrad, tTemperature);

            ResultT tResult(0);
            tComputeVonMises(iCellOrdinal, tStress, tResult);
            Kokkos::atomic_add(&aResultWS(iCellOrdinal), tResult*tVolume);
            Kokkos::atomic_add(&tCellVolume(iCellOrdinal), tVolume);
        });

        Kokkos::parallel_for("compute cell quantities", Kokkos::RangePolicy<>(0, tNumCells),
        LAMBDA_EXPRESSION(const Plato::OrdinalType iCellOrdinal)
        {
            aResultWS(iCellOrdinal) /= tCellVolume(iCellOrdinal);
        });

    }
};
// class ThermalVonMisesLocalMeasure

}
//namespace Plato

// TODO #include "SimplexThermomechanics.hpp"

#ifdef PLATOANALYZE_1D
// TODO PLATO_EXPL_DEC2(Plato::ThermalVonMisesLocalMeasure, Plato::SimplexThermomechanics, 1)
#endif

#ifdef PLATOANALYZE_2D
// TODO PLATO_EXPL_DEC2(Plato::ThermalVonMisesLocalMeasure, Plato::SimplexThermomechanics, 2)
#endif

#ifdef PLATOANALYZE_3D
// TODO PLATO_EXPL_DEC2(Plato::ThermalVonMisesLocalMeasure, Plato::SimplexThermomechanics, 3)
#endif
