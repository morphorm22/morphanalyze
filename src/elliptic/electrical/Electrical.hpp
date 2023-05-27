/*
 *  Electrical.hpp
 *
 *  Created on: May 25, 2023
 */

#pragma once

/// @include standard cpp includes
#include <memory>

/// @include analyze includes
#include "AnalyzeMacros.hpp"
#include "elliptic/electrical/ElectricalElement.hpp"
#include "elliptic/electrical/SupportedOptionEnums.hpp"
#include "elliptic/electrical/ResidualSteadyStateCurrent.hpp"
#include "elliptic/electrical/CriterionVolumeTwoPhase.hpp"
#include "elliptic/electrical/CriterionPowerSurfaceDensityTwoPhase.hpp"

namespace Plato
{

/// @struct FunctionFactory
/// @brief factory of electrical residual and criteria
struct FunctionFactory
{
  /// @fn createVectorFunction
  /// @brief create electrical residual evaluator for an elliptic problem
  /// @tparam EvaluationType automatic differentiation evaluation type, which sets scalar types
  /// @param [in] aSpatialDomain contains mesh and model information
  /// @param [in] aDataMap       output data map
  /// @param [in] aParamList     input problem parameters
  /// @param [in] aTypePDE       partial differential equation type
  /// @return shared pointer to electrical residual evaluator 
  template <typename EvaluationType>
  std::shared_ptr<Plato::Elliptic::AbstractVectorFunction<EvaluationType>>
  createVectorFunction(
      const Plato::SpatialDomain   & aSpatialDomain,
            Plato::DataMap         & aDataMap,
            Teuchos::ParameterList & aParamList,
            std::string              aTypePDE
  )
  {
    Plato::electrical::ResidualEnum tSupportedResidual;
    auto tResidual = tSupportedResidual.get(aTypePDE);
    switch (tResidual)
    {
    default:
    case Plato::electrical::residual::STEADY_STATE_CURRENT:
      return 
        (std::make_shared<Plato::ResidualSteadyStateCurrent<EvaluationType>>(aSpatialDomain, aDataMap, aParamList));
      break;
    }
  }

  /// @fn createScalarFunction
  /// @brief create electrical criterion evaluator for an elliptic problem
  /// @tparam EvaluationType automatic differentiation evaluation type, which sets scalar types
  /// @param [in] aSpatialDomain contains mesh and model information
  /// @param [in] aDataMap       output data map
  /// @param [in] aParamList     input problem parameters
  /// @param [in] aCriterionType criterion type
  /// @param [in] aFuncName      name of the criterion parameter list
  /// @return shared pointer to electrical criterion evaluator 
  template <typename EvaluationType>
  std::shared_ptr<Plato::Elliptic::AbstractScalarFunction<EvaluationType>>
  createScalarFunction( 
      const Plato::SpatialDomain   & aSpatialDomain,
            Plato::DataMap         & aDataMap,
            Teuchos::ParameterList & aParamList,
            std::string              aCriterionType,
            std::string              aFuncName
  )
  {
    Plato::electrical::CriterionEnum tSupportedCriterion;
    auto tCriterion = tSupportedCriterion.get(aCriterionType);
    switch (tCriterion)
    {
    case Plato::electrical::criterion::TWO_PHASE_POWER_SURFACE_DENSITY:
      return ( std::make_shared<Plato::CriterionPowerSurfaceDensityTwoPhase<EvaluationType>>(
        aSpatialDomain, aDataMap, aParamList, aFuncName) );
      break;
    case Plato::electrical::criterion::TWO_PHASE_VOLUME:
      return ( std::make_shared<Plato::CriterionVolumeTwoPhase<EvaluationType>>(
        aSpatialDomain, aDataMap, aParamList, aFuncName) );
      break;  
    default:
      ANALYZE_THROWERR("Error while constructing criterion");
      return nullptr;
      break;
    }
  };
};

} // namespace Plato

namespace Plato
{
 
template<typename TopoElementType>
class Electrical
{
public:
  /// @brief factory for electrical physics scalar and vector functions
  typedef Plato::FunctionFactory FunctionFactory;
  /// @brief topological element type with additional physics related information 
  using ElementType = Plato::ElectricalElement<TopoElementType>;
};

}
// namespace Plato