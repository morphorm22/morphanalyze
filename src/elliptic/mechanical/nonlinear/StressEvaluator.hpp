/*
 * StressEvaluator.hpp
 *
 *  Created on: May 31, 2023
 */

#pragma once

#include "SpatialModel.hpp"
#include "PlatoStaticsTypes.hpp"

namespace Plato
{

/// @class StressEvaluator
/// @brief base class for stress evaluators
/// @tparam EvaluationType automatic differentiation evaluation type, which sets scalar types
template<typename EvaluationType>
class StressEvaluator
{
private:
  /// @brief topological element typename
  using ElementType = typename EvaluationType::ElementType;
  /// @brief scalar types for an evaluation type
  using StateScalarType   = typename EvaluationType::StateScalarType;
  using ControlScalarType = typename EvaluationType::ControlScalarType;
  using ConfigScalarType  = typename EvaluationType::ConfigScalarType;
  using ResultScalarType  = typename EvaluationType::ResultScalarType;

protected:
  /// @brief contains mesh and model information
  const Plato::SpatialDomain & mSpatialDomain;
  /// @brief output database 
  Plato::DataMap & mDataMap;

public:
  /// @brief base class construtor
  /// @param [in] aSpatialDomain contains mesh and model information
  /// @param [in] aDataMap       output database
  explicit
  StressEvaluator(
      const Plato::SpatialDomain & aSpatialDomain,
            Plato::DataMap       & aDataMap
  ) :
    mSpatialDomain(aSpatialDomain),
    mDataMap(aDataMap)
  {}
  /// @brief base class destructor
  virtual ~StressEvaluator()
  {}

  /// @fn evaluate
  /// @brief pure virtual method: evaluates current density 
  /// @param [in]     aSpatialDomain contains meshed model information
  /// @param [in]     aState         state workset
  /// @param [in]     aControl       control workset
  /// @param [in]     aConfig        configuration workset
  /// @param [in,out] aResult        result workset
  /// @param [in]     aCycle         scalar 
  virtual 
  void 
  evaluate(
      const Plato::ScalarMultiVectorT<StateScalarType>   & aState,
      const Plato::ScalarMultiVectorT<ControlScalarType> & aControl,
      const Plato::ScalarArray3DT<ConfigScalarType>      & aConfig,
      const Plato::ScalarArray4DT<ResultScalarType>      & aResult,
            Plato::Scalar                                  aCycle = 0.0
  ) const = 0;

};

}