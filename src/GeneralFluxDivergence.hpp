#pragma once

#include "PlatoStaticsTypes.hpp"

namespace Plato
{

/******************************************************************************/
/*! Flux divergence functor.

 Given a thermal flux, compute the flux divergence.
 */
/******************************************************************************/
template<typename ElementType, Plato::OrdinalType NumDofsPerNode = 1, Plato::OrdinalType DofOffset = 0>
class GeneralFluxDivergence : public ElementType
{
private:
    using ElementType::mNumNodesPerCell;
    using ElementType::mNumSpatialDims;

public:
    /******************************************************************************//**
     * \brief Compute flux divergence
     * \param [in] aCellOrdinal cell (i.e. element) ordinal
     * \param [in/out] aOutput output, i.e. flux divergence
     * \param [in] aFlux input flux workset
     * \param [in] aGradient configuration gradients
     * \param [in] aStateValues 2D state values workset
     * \param [in] aScale scale parameter (default = 1.0)
    **********************************************************************************/
    template<typename ForcingScalarType, typename FluxScalarType, typename GradientScalarType, typename VolumeScalarType>
    DEVICE_TYPE inline void
    operator()(
        Plato::OrdinalType aCellOrdinal,
        Plato::ScalarMultiVectorT<ForcingScalarType> aOutput,
        Plato::Array<mNumSpatialDims, FluxScalarType> aFlux,
        Plato::Matrix<mNumNodesPerCell, mNumSpatialDims, GradientScalarType> aGradient,
        VolumeScalarType  aCellVolume,
        Plato::Scalar aScale = 1.0
    ) const
    {
        for(Plato::OrdinalType tNodeIndex = 0; tNodeIndex < mNumNodesPerCell; tNodeIndex++)
        {
            Plato::OrdinalType tLocalOrdinal = tNodeIndex * NumDofsPerNode + DofOffset;
            for(Plato::OrdinalType tDimIndex = 0; tDimIndex < mNumSpatialDims; tDimIndex++)
            {
                Kokkos::atomic_add(&aOutput(aCellOrdinal, tLocalOrdinal),
                    aScale * aFlux(tDimIndex) * aGradient(tNodeIndex, tDimIndex) * aCellVolume);
            }
        }
    }
};
// class FluxDivergence

}
// namespace Plato
