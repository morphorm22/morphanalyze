#ifndef PLATO_LINEAR_TMKINETICS_HPP
#define PLATO_LINEAR_TMKINETICS_HPP

#include "AbstractTMKinetics.hpp"

namespace Plato
{

/******************************************************************************/
/*! Linear Thermomechanics Kinetics functor.

    given a strain, temperature gradient, and temperature, compute the stress and flux
*/
/******************************************************************************/
template<typename EvaluationType, typename SimplexPhysics>
class LinearTMKinetics :
    public Plato::AbstractTMKinetics<EvaluationType, SimplexPhysics>
{
protected:
    using StateT  = typename EvaluationType::StateScalarType;  /*!< state variables automatic differentiation type */
    using ConfigT = typename EvaluationType::ConfigScalarType; /*!< configuration variables automatic differentiation type */
    using KineticsScalarType = typename EvaluationType::ResultScalarType; /*!< result variables automatic differentiation type */
    using KinematicsScalarType = typename Plato::fad_type_t<SimplexPhysics, StateT, ConfigT>; /*!<   strain variables automatic differentiation type */
    using Plato::SimplexThermomechanics<EvaluationType::SpatialDim>::mNumVoigtTerms;
    
    Plato::Rank4VoigtConstant<EvaluationType::SpatialDim> mElasticStiffnessConstant;
    Plato::TensorConstant<EvaluationType::SpatialDim> mThermalExpansivityConstant;
    Plato::TensorConstant<EvaluationType::SpatialDim> mThermalConductivityConstant;
    Plato::Scalar mRefTemperature;
    const Plato::Scalar mScaling;
    const Plato::Scalar mScaling2;
    Plato::VoigtMap<EvaluationType::SpatialDim> cVoigtMap;

public:
    /******************************************************************************//**
     * \brief Constructor
     * \param [in] aMaterialModel material model
    **********************************************************************************/
    LinearTMKinetics(const Teuchos::RCP<Plato::MaterialModel<EvaluationType::SpatialDim>> aMaterialModel) :
            AbstractTMKinetics<EvaluationType, SimplexPhysics>(aMaterialModel),
            mRefTemperature(aMaterialModel->getScalarConstant("Reference Temperature")),
            mScaling(aMaterialModel->getScalarConstant("Temperature Scaling")),
            mScaling2(mScaling*mScaling)
    {
        mElasticStiffnessConstant = aMaterialModel->getRank4VoigtConstant("Elastic Stiffness");
        mThermalExpansivityConstant = aMaterialModel->getTensorConstant("Thermal Expansivity");
        mThermalConductivityConstant = aMaterialModel->getTensorConstant("Thermal Conductivity");
    }

    /***********************************************************************************
     * \brief Compute stress and thermal flux from strain, temperature, and temperature gradient
     * \param [in] aStrain infinitesimal strain tensor
     * \param [in] aTGrad temperature gradient
     * \param [in] aTemperature temperature
     * \param [out] aStress Cauchy stress tensor
     * \param [out] aFlux thermal flux vector
     **********************************************************************************/
    //template<typename KineticsScalarType, typename KinematicsScalarType, typename StateScalarType>
    void
    operator()( Kokkos::View<KineticsScalarType**,   Plato::Layout, Plato::MemSpace> const& aStress,
                Kokkos::View<KineticsScalarType**,   Plato::Layout, Plato::MemSpace> const& aFlux,
                Kokkos::View<KinematicsScalarType**, Plato::Layout, Plato::MemSpace> const& aStrain,
                Kokkos::View<KinematicsScalarType**, Plato::Layout, Plato::MemSpace> const& aTGrad,
                Kokkos::View<StateT*,                Plato::MemSpace> const& aTemperature) const override
    {
        const Plato::OrdinalType tNumCells = aStrain.extent(0);
        Kokkos::parallel_for(Kokkos::RangePolicy<int>(0, tNumCells), LAMBDA_EXPRESSION(const int & aCellOrdinal)
        {
            StateT tTemperature = aTemperature(aCellOrdinal);
            // compute thermal strain
            //
            StateT tstrain[mNumVoigtTerms] = {0};
            for( int iDim=0; iDim<EvaluationType::SpatialDim; iDim++ ){
                tstrain[iDim] = mScaling * mThermalExpansivityConstant(cVoigtMap.I[iDim], cVoigtMap.J[iDim])
                            * (tTemperature - mRefTemperature);
            }

            // compute stress
            //
            for( int iVoigt=0; iVoigt<mNumVoigtTerms; iVoigt++){
                aStress(aCellOrdinal,iVoigt) = 0.0;
                for( int jVoigt=0; jVoigt<mNumVoigtTerms; jVoigt++){
                    aStress(aCellOrdinal,iVoigt) += (aStrain(aCellOrdinal,jVoigt)-tstrain[jVoigt])*mElasticStiffnessConstant(iVoigt, jVoigt);
                }
            }

            // compute flux
            //
            for( int iDim=0; iDim<EvaluationType::SpatialDim; iDim++){
                aFlux(aCellOrdinal,iDim) = 0.0;
                for( int jDim=0; jDim<EvaluationType::SpatialDim; jDim++){
                    aFlux(aCellOrdinal,iDim) += mScaling2 * aTGrad(aCellOrdinal,jDim)*mThermalConductivityConstant(iDim, jDim);
                }
            }
        }, "Cauchy stress");
    }
};
// class LinearTMKinetics

}// namespace Plato
#endif

/*
#ifdef PLATOANALYZE_1D
PLATO_EXPL_DEC2(Plato::LinearTMKinetics  , Plato::SimplexThermomechanics, 1)
#endif

#ifdef PLATOANALYZE_2D
PLATO_EXPL_DEC2(Plato::LinearTMKinetics  , Plato::SimplexThermomechanics, 2)
#endif

#ifdef PLATOANALYZE_3D
PLATO_EXPL_DEC2(Plato::LinearTMKinetics  , Plato::SimplexThermomechanics, 3)
#endif
*/
