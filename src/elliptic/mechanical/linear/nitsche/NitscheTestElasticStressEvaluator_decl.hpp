/*
 * NitscheTestElasticStressEvaluator_decl.hpp
 *
 *  Created on: July 13, 2023
 */

#pragma once

#include "WorkSets.hpp"
#include "SpatialModel.hpp"

#include "materials/MaterialModel.hpp"
#include "elliptic/EvaluationTypes.hpp"
#include "bcs/dirichlet/nitsche/NitscheEvaluator.hpp"
#include "elliptic/mechanical/linear/nitsche/BoundaryEvaluatorTestIsotropicElasticStress.hpp"

namespace Plato
{

namespace Elliptic
{

/// @class NitscheTestElasticStressEvaluator
/// @brief evaluate nitsche's trial stress integral 
///
/// \f[
///   -\int_{\Gamma_D}\delta\left(\sigma_{ij}n_j\right)\left(u_i-u_i^D\right) d\Gamma
/// \f]
///
///  where \f$\delta\sigma_{ij}\f$ is the test stress tensor, \f$n_j\f$ is the normal vector, 
/// \f$u_i\f$ is the trial displacement, \f$u_i^D\f$ is the enforced dirichlet displacement, 
/// and \f$\Gamma_D\f$ is the surface where dirichlet boundary conditions are enforced. 
///
/// @tparam EvaluationType automatic differentiation evaluation type, which sets scalar types
template<typename EvaluationType>
class NitscheTestElasticStressEvaluator : public Plato::NitscheEvaluator
{
private:
  /// @brief local topological parent body and face element typenames
  using BodyElementBase = typename EvaluationType::ElementType;
  using FaceElementBase = typename BodyElementBase::Face;
  /// @brief number of spatial dimensions
  static constexpr auto mNumSpatialDims = BodyElementBase::mNumSpatialDims;
  /// @brief number of degrees of freedom per parent body element vertex/node
  static constexpr auto mNumDofsPerNode = BodyElementBase::mNumDofsPerNode;
  /// @brief number of nodes per parent body element
  static constexpr auto mNumNodesPerCell = BodyElementBase::mNumNodesPerCell;
  /// @brief number of nodes per parent body element surface
  static constexpr auto mNumNodesPerFace = BodyElementBase::mNumNodesPerFace;
  /// @brief number of integration points per parent body element surface
  static constexpr auto mNumGaussPointsPerFace = BodyElementBase::mNumGaussPointsPerFace;
  /// @brief local typename for base class
  using BaseClassType = Plato::NitscheEvaluator;
  /// @brief side set name where dirichlet boundary conditions are 
  using BaseClassType::mSideSetName;
  /// @brief name assigned to the material constitutive model
  using BaseClassType::mMaterialName;
  /// @brief scalar types associated with the automatic differentation evaluation type
  using StateScalarType  = typename EvaluationType::StateScalarType;
  using ResultScalarType = typename EvaluationType::ResultScalarType;
  using ConfigScalarType = typename EvaluationType::ConfigScalarType;
  using StrainScalarType = typename Plato::fad_type_t<BodyElementBase,StateScalarType,ConfigScalarType>;
  /// @brief local typename for surface stress evaluator class
  using EvaluatorClassType = Plato::Elliptic::BoundaryEvaluatorTestIsotropicElasticStress<EvaluationType>;
  /// @brief evaluates boundary test stress tensors
  std::shared_ptr<EvaluatorClassType> mBoundaryStressEvaluator;

public:
  /// @brief class constructor
  /// @param [in] aParamList     input problem parameters
  /// @param [in] aNitscheParams input parameters for nitsche's method
  NitscheTestElasticStressEvaluator(
    Teuchos::ParameterList & aParamList,
    Teuchos::ParameterList & aNitscheParams
  );

  /// @brief class destructor
  ~NitscheTestElasticStressEvaluator();

  /// @fn evaluate
  /// @brief evaluate test stresses for all side set cells
  /// @param [in]     aSpatialModel contains mesh and model information
  /// @param [in,out] aWorkSets     domain and range workset database
  /// @param [in]     aCycle        scalar
  /// @param [in]     aScale        scalar
  void 
  evaluate(
    const Plato::SpatialModel & aSpatialModel,
    const Plato::WorkSets     & aWorkSets,
        Plato::Scalar           aCycle = 0.0,
        Plato::Scalar           aScale = 1.0
  );
};

} // namespace Elliptic

} // namespace Plato