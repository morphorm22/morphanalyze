#pragma once

namespace Plato {

template<typename ElementType>
class ComputeCellVolume : public ElementType
{
  public:

    template<typename ScalarType>
    DEVICE_TYPE inline void
    operator()(
              Plato::OrdinalType aCellOrdinal,
        const Plato::Array<ElementType::mNumSpatialDims>& aCubPoint,
        const Plato::ScalarArray3DT<ScalarType> aConfig,
              ScalarType& aVolume
    ) const
    {
        auto tJacobian = ElementType::jacobian(aCubPoint, aConfig, aCellOrdinal);
        aVolume = Plato::determinant(tJacobian);
    }

};

}
