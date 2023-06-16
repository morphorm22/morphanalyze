#pragma once


#include <Sacado.hpp>

#include "FadTypes.hpp"

namespace Plato
{

namespace Elliptic
{

template <typename ElementTypeT>
struct EvaluationTypes
{
    static constexpr int NumNodesPerCell = ElementTypeT::mNumNodesPerCell;
    static constexpr int NumControls     = ElementTypeT::mNumControl;
    static constexpr int SpatialDim      = ElementTypeT::mNumSpatialDims;

    using ElementType = ElementTypeT;
};

template <typename ElementType>
struct ResidualTypes : EvaluationTypes<ElementType>
{
  using StateScalarType     = Plato::Scalar;
  using ControlScalarType   = Plato::Scalar;
  using ConfigScalarType    = Plato::Scalar;
  using NodeStateScalarType = Plato::Scalar;
  using ResultScalarType    = Plato::Scalar;
};

template <typename ElementType>
struct JacobianTypes : EvaluationTypes<ElementType>
{
  using SFadType = typename Plato::FadTypes<ElementType>::StateFad;

  using StateScalarType     = SFadType;
  using ControlScalarType   = Plato::Scalar;
  using ConfigScalarType    = Plato::Scalar;
  using NodeStateScalarType = Plato::Scalar;
  using ResultScalarType    = SFadType;
};

template <typename ElementType>
struct GradientXTypes : EvaluationTypes<ElementType>
{
  using SFadType = typename FadTypes<ElementType>::ConfigFad;

  using StateScalarType     = Plato::Scalar;
  using ControlScalarType   = Plato::Scalar;
  using ConfigScalarType    = SFadType;
  using NodeStateScalarType = Plato::Scalar;
  using ResultScalarType    = SFadType;
};

template <typename ElementType>
struct GradientZTypes : EvaluationTypes<ElementType>
{
  using SFadType = typename FadTypes<ElementType>::ControlFad;

  using StateScalarType     = Plato::Scalar;
  using ControlScalarType   = SFadType;
  using ConfigScalarType    = Plato::Scalar;
  using NodeStateScalarType = Plato::Scalar;
  using ResultScalarType    = SFadType;
};

template <typename ElementType>
struct GradientNTypes : EvaluationTypes<ElementType>
{
  using SFadType = typename FadTypes<ElementType>::NodeStateFad;

  using StateScalarType     = Plato::Scalar;
  using ControlScalarType   = Plato::Scalar;
  using ConfigScalarType    = Plato::Scalar;
  using NodeStateScalarType = SFadType;
  using ResultScalarType    = SFadType;
};

template <typename ElementTypeT>
struct Evaluation {
  using Residual  = ResidualTypes<ElementTypeT>;
  using Jacobian  = JacobianTypes<ElementTypeT>;
  using GradientZ = GradientZTypes<ElementTypeT>;
  using GradientX = GradientXTypes<ElementTypeT>;
  using GradientN = GradientNTypes<ElementTypeT>;
};

} // namespace Elliptic

} // namespace Plato
