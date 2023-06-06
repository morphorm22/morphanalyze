#pragma once

#include "BLAS1.hpp"
#include "Solutions.hpp"
#include "ParseTools.hpp"
#include "Geometrical.hpp"
#include "EssentialBCs.hpp"
#include "AnalyzeMacros.hpp"
#include "AnalyzeOutput.hpp"
#include "base/Database.hpp"
#include "ImplicitFunctors.hpp"
#include "ApplyConstraints.hpp"
#include "MultipointConstraints.hpp"
#include "geometric/ScalarFunctionBaseFactory.hpp"
#include "elliptic/criterioneval/FactoryCriterionEvaluator.hpp"


namespace Plato
{

namespace Elliptic
{

template<typename PhysicsType>
Problem<PhysicsType>::
Problem(
  Plato::Mesh              aMesh,
  Teuchos::ParameterList & aParamList,
  Plato::Comm::Machine     aMachine
) :
  AbstractProblem(aMesh,aParamList),
  mSpatialModel  (aMesh,aParamList,mDataMap),
  mPDE(std::make_shared<VectorFunctionType>(mSpatialModel,mDataMap,aParamList,aParamList.get<std::string>("PDE Constraint"))),
  mNumNewtonSteps(Plato::ParseTools::getSubParam<int>(aParamList,"Newton Iteration","Maximum Iterations",1.)),
  mNewtonIncTol(Plato::ParseTools::getSubParam<double>(aParamList,"Newton Iteration","Increment Tolerance",0.)),
  mNewtonResTol(Plato::ParseTools::getSubParam<double>(aParamList,"Newton Iteration","Residual Tolerance",0.)),
  mSaveState    (aParamList.sublist("Elliptic").isType<Teuchos::Array<std::string>>("Plottable")),
  mResidual     ("MyResidual", mPDE->size()),
  mStates       ("States", static_cast<Plato::OrdinalType>(1), mPDE->size()),
  mJacobian     (Teuchos::null),
  mIsSelfAdjoint(aParamList.get<bool>("Self-Adjoint", false)),
  mPDEType      (aParamList.get<std::string>("PDE Constraint")),
  mPhysics      (aParamList.get<std::string>("Physics")),
  mMPCs         (nullptr)
{
  this->initialize(aParamList);
  LinearSystemType systemType = LinearSystemType::SYMMETRIC_POSITIVE_DEFINITE;
  if (mPhysics == "Electromechanical" || mPhysics == "Thermomechanical") {
    systemType = LinearSystemType::SYMMETRIC_INDEFINITE;
  }
  Plato::SolverFactory tSolverFactory(aParamList.sublist("Linear Solver"), systemType);
  mSolver = tSolverFactory.create(aMesh->NumNodes(), aMachine, ElementType::mNumDofsPerNode, mMPCs);
}

template<typename PhysicsType>
Problem<PhysicsType>::
~Problem(){}

template<typename PhysicsType>
Plato::OrdinalType
Problem<PhysicsType>::
numNodes() const
{
  const auto tNumNodes = mPDE->numNodes();
  return (tNumNodes);
}

template<typename PhysicsType>
Plato::OrdinalType
Problem<PhysicsType>::
numCells() const
{
  const auto tNumCells = mPDE->numCells();
  return (tNumCells);
}
    
template<typename PhysicsType>
Plato::OrdinalType
Problem<PhysicsType>::
numDofsPerCell() const
{
  const auto tNumDofsPerCell = mPDE->numDofsPerCell();
  return (tNumDofsPerCell);
}

template<typename PhysicsType>
Plato::OrdinalType
Problem<PhysicsType>::
numNodesPerCell() const
{
  const auto tNumNodesPerCell = mPDE->numNodesPerCell();
  return (tNumNodesPerCell);
}

template<typename PhysicsType>
Plato::OrdinalType
Problem<PhysicsType>::
numDofsPerNode() const
{
    const auto tNumDofsPerNode = mPDE->numDofsPerNode();
    return (tNumDofsPerNode);
}

template<typename PhysicsType>
Plato::OrdinalType
Problem<PhysicsType>::
numControlsPerNode() const
{
  const auto tNumControlsPerNode = mPDE->numControlsPerNode();
  return (tNumControlsPerNode);
}

template<typename PhysicsType>
bool
Problem<PhysicsType>::
criterionIsLinear(
    const std::string & aName
)
{
  return mLinearCriteria.count(aName) > 0 ? true : false;
}

template<typename PhysicsType>
void Problem<PhysicsType>::
output(
    const std::string & aFilepath
)
{
  auto tDataMap = this->getDataMap();
  auto tSolution = this->getSolution();
  auto tSolutionOutput = mPDE->getSolutionStateOutputData(tSolution);
  Plato::universal_solution_output(aFilepath, tSolutionOutput, tDataMap, mSpatialModel.Mesh);
}

template<typename PhysicsType>
void Problem<PhysicsType>::
applyStateConstraints(
  const Teuchos::RCP<Plato::CrsMatrixType> & aMatrix,
  const Plato::ScalarVector & aVector,
        Plato::Scalar aScale
)
//**********************************************************************************/
{
  if(mJacobian->isBlockMatrix())
  {
    Plato::applyBlockConstraints<ElementType::mNumDofsPerNode>(aMatrix, aVector, mBcDofs, mBcValues, aScale);
  }
  else
  {
    Plato::applyConstraints<ElementType::mNumDofsPerNode>(aMatrix, aVector, mBcDofs, mBcValues, aScale);
  }
}

template<typename PhysicsType>
void Problem<PhysicsType>::
updateProblem(
  const Plato::ScalarVector & aControls, 
  const Plato::Solutions & aSolution
)
{
  auto tState = aSolution.get("State");
  constexpr Plato::OrdinalType tCYCLE = 0;
  auto tMyStates = Kokkos::subview(tState, tCYCLE, Kokkos::ALL());
  for( auto tCriterion : mCriteria )
  {
    tCriterion.second->updateProblem(tMyStates, aControls);
  }
  for( auto tCriterion : mLinearCriteria )
  {
    tCriterion.second->updateProblem(aControls);
  }
}

template<typename PhysicsType>
Plato::Solutions
Problem<PhysicsType>::
solution(
  const Plato::ScalarVector & aControls
)
{
  // clear output database
  mDataMap.clearStates();
  // set database
  Plato::Database tDatabase;
  constexpr Plato::OrdinalType tCYCLE = 0;
  Plato::ScalarVector tMyStates = Kokkos::subview(mStates, tCYCLE, Kokkos::ALL());
  Plato::blas1::fill(static_cast<Plato::Scalar>(0.0), tMyStates);
  tDatabase.vector("states",tMyStates);
  tDatabase.vector("controls",aControls);
  // save controls to output database
  mDataMap.scalarNodeFields["Topology"] = aControls;
  // inner loop for non-linear models
  for(Plato::OrdinalType tNewtonIndex = 0; tNewtonIndex < mNumNewtonSteps; tNewtonIndex++)
  {
      mResidual = mPDE->value(tMyStates, aControls);
      Plato::blas1::scale(-1.0, mResidual);
      if (mNumNewtonSteps > 1) {
          auto tResidualNorm = Plato::blas1::norm(mResidual);
          std::cout << " Residual norm: " << tResidualNorm << std::endl;
          if (tResidualNorm < mNewtonResTol) {
              std::cout << " Residual norm tolerance satisfied." << std::endl;
              break;
          }
      }
      mJacobian = mPDE->gradient_u(tMyStates, aControls);
      Plato::Scalar tScale = (tNewtonIndex == 0) ? 1.0 : 0.0;
      this->applyStateConstraints(mJacobian, mResidual, tScale);
      Plato::ScalarVector tDeltaD("increment", tMyStates.extent(0));
      Plato::blas1::fill(static_cast<Plato::Scalar>(0.0), tDeltaD);
      mSolver->solve(*mJacobian, tDeltaD, mResidual);
      Plato::blas1::axpy(1.0, tDeltaD, tMyStates);
      if (mNumNewtonSteps > 1) {
          auto tIncrementNorm = Plato::blas1::norm(tDeltaD);
          std::cout << " Delta norm: " << tIncrementNorm << std::endl;
          if (tIncrementNorm < mNewtonIncTol) {
              std::cout << " Solution increment norm tolerance satisfied." << std::endl;
              break;
          }
      }
  }
  if ( mSaveState )
  {
      // evaluate at new state
      mResidual  = mPDE->value(tMyStates, aControls);
      mDataMap.saveState();
  }
  auto tSolution = this->getSolution();
  return tSolution;
}

template<typename PhysicsType>
Plato::Scalar
Problem<PhysicsType>::
criterionValue(
    const Plato::ScalarVector & aControls,
    const std::string         & aName
)
{
  if( mCriteria.count(aName) )
  {
      Plato::Solutions tSolution(mPhysics);
      tSolution.set("State", mStates);
      Criterion tCriterion = mCriteria[aName];
      return tCriterion->value(tSolution, aControls);
  }
  else
  if( mLinearCriteria.count(aName) )
  {
      LinearCriterion tCriterion = mLinearCriteria[aName];
      return tCriterion->value(aControls);
  }
  else
  {
      ANALYZE_THROWERR(std::string("CRITERION WITH NAME '") + aName + "' IS NOT DEFINED IN THE CRITERION MAP.")
  }
}

template<typename PhysicsType>
Plato::Scalar
Problem<PhysicsType>::
criterionValue(
    const Plato::ScalarVector & aControls,
    const Plato::Solutions    & aSolution,
    const std::string         & aName
)
{
  if( mCriteria.count(aName) )
  {
      Criterion tCriterion = mCriteria[aName];
      return tCriterion->value(aSolution, aControls);
  }
  else
  if( mLinearCriteria.count(aName) )
  {
      LinearCriterion tCriterion = mLinearCriteria[aName];
      return tCriterion->value(aControls);
  }
  else
  {
      ANALYZE_THROWERR(std::string("CRITERION WITH NAME '") + aName + "' IS NOT DEFINED IN THE CRITERION MAP.")
  }
}

template<typename PhysicsType>
Plato::ScalarVector
Problem<PhysicsType>::
criterionGradient(
    const Plato::ScalarVector & aControls,
    const Plato::Solutions    & aSolution,
    const std::string         & aName
)
{
  if( mCriteria.count(aName) )
  {
      Criterion tCriterion = mCriteria[aName];
      return criterionGradient(aControls, aSolution, tCriterion);
  }
  else
  if( mLinearCriteria.count(aName) )
  {
      LinearCriterion tCriterion = mLinearCriteria[aName];
      return tCriterion->gradient_z(aControls);
  }
  else
  {
      ANALYZE_THROWERR(std::string("CRITERION WITH NAME '") + aName + "' IS NOT DEFINED IN THE CRITERION MAP.")
  }
}

template<typename PhysicsType>
Plato::ScalarVector
Problem<PhysicsType>::
criterionGradient(
    const Plato::ScalarVector & aControls,
    const Plato::Solutions    & aSolution,
          Criterion             aCriterion
)
{
  if(aCriterion == nullptr)
  {
      ANALYZE_THROWERR("REQUESTED CRITERION NOT DEFINED BY USER.");
  }
  if(aSolution.empty())
  {
      ANALYZE_THROWERR("SOLUTION DATABASE IS EMPTY")
  }
  if(static_cast<Plato::OrdinalType>(mAdjoint.size()) <= static_cast<Plato::OrdinalType>(0))
  {
      const auto tLength = mPDE->size();
      mAdjoint = Plato::ScalarMultiVector("Adjoint Variables", 1, tLength);
  }
  // compute dfdz: partial of criterion wrt z
  const Plato::OrdinalType tCYCLE = 0;
  auto tState = aSolution.get("State");
  auto tMyStates = Kokkos::subview(tState, tCYCLE, Kokkos::ALL());
  Plato::ScalarVector tAdjointSubView = Kokkos::subview(mAdjoint, tCYCLE, Kokkos::ALL());
  auto tPartialCriterionWRT_Control = aCriterion->gradient_z(aSolution, aControls);
  if(mIsSelfAdjoint)
  {
      Plato::blas1::copy(tMyStates, tAdjointSubView);
      Plato::blas1::scale(static_cast<Plato::Scalar>(-1), tAdjointSubView);
  }
  else
  {
      // compute dfdu: partial of criterion wrt u
      auto tPartialCriterionWRT_State = aCriterion->gradient_u(aSolution, aControls, /*stepIndex=*/0);
      Plato::blas1::scale(static_cast<Plato::Scalar>(-1), tPartialCriterionWRT_State);
      // compute dgdu: partial of PDE wrt state
      mJacobian = mPDE->gradient_u_T(tMyStates, aControls);
      this->applyAdjointConstraints(mJacobian, tPartialCriterionWRT_State);
      Plato::blas1::fill(static_cast<Plato::Scalar>(0.0), tAdjointSubView);
      mSolver->solve(*mJacobian, tAdjointSubView, tPartialCriterionWRT_State, /*isAdjointSolve=*/ true);
  }
  // compute dgdz: partial of PDE wrt state.
  // dgdz is returned transposed, nxm.  n=z.size() and m=u.size().
  auto tPartialPDE_WRT_Control = mPDE->gradient_z(tMyStates, aControls);
  // compute dgdz . adjoint + dfdz
  Plato::MatrixTimesVectorPlusVector(tPartialPDE_WRT_Control, tAdjointSubView, tPartialCriterionWRT_Control);
  return tPartialCriterionWRT_Control;
}

template<typename PhysicsType>
Plato::ScalarVector
Problem<PhysicsType>::
criterionGradientX(
    const Plato::ScalarVector & aControls,
    const Plato::Solutions    & aSolution,
    const std::string         & aName
)
{
  if( mCriteria.count(aName) )
  {
      Criterion tCriterion = mCriteria[aName];
      return criterionGradientX(aControls, aSolution, tCriterion);
  }
  else
  if( mLinearCriteria.count(aName) )
  {
      LinearCriterion tCriterion = mLinearCriteria[aName];
      return tCriterion->gradient_x(aControls);
  }
  else
  {
      ANALYZE_THROWERR(std::string("CRITERION WITH NAME '") + aName + "' IS NOT DEFINED IN THE CRITERION MAP.")
  }
}

template<typename PhysicsType>
Plato::ScalarVector
Problem<PhysicsType>::
criterionGradientX(
    const Plato::ScalarVector & aControls,
    const Plato::Solutions    & aSolution,
          Criterion             aCriterion)
{
  if(aCriterion == nullptr)
  {
      ANALYZE_THROWERR("REQUESTED CRITERION NOT DEFINED BY USER.");
  }
  if(aSolution.empty())
  {
      ANALYZE_THROWERR("SOLUTION DATABASE IS EMPTY")
  }
  // compute partial derivative wrt x
  auto tState = aSolution.get("State");
  const Plato::OrdinalType tCYCLE = 0;
  auto tMyStates = Kokkos::subview(tState, tCYCLE, Kokkos::ALL());
  auto tPartialCriterionWRT_Config  = aCriterion->gradient_x(aSolution, aControls);
  if(mIsSelfAdjoint)
  {
      Plato::blas1::scale(static_cast<Plato::Scalar>(-1), tPartialCriterionWRT_Config);
  }
  else
  {
      // compute dfdu: partial of criterion wrt u
      auto tPartialCriterionWRT_State = aCriterion->gradient_u(aSolution, aControls, /*stepIndex=*/0);
      Plato::blas1::scale(static_cast<Plato::Scalar>(-1), tPartialCriterionWRT_State);
      // compute dgdu: partial of PDE wrt state
      mJacobian = mPDE->gradient_u(tMyStates, aControls);
      this->applyStateConstraints(mJacobian, tPartialCriterionWRT_State, 1.0);
      // adjoint problem uses transpose of global stiffness, but we're assuming the constrained
      // system is symmetric.
      Plato::ScalarVector
        tAdjointSubView = Kokkos::subview(mAdjoint, tCYCLE, Kokkos::ALL());
      mSolver->solve(*mJacobian, tAdjointSubView, tPartialCriterionWRT_State, /*isAdjointSolve=*/ true);
      // compute dgdx: partial of PDE wrt config.
      // dgdx is returned transposed, nxm.  n=x.size() and m=u.size().
      auto tPartialPDE_WRT_Config = mPDE->gradient_x(tMyStates, aControls);
      // compute dgdx . adjoint + dfdx
      Plato::MatrixTimesVectorPlusVector(tPartialPDE_WRT_Config, tAdjointSubView, tPartialCriterionWRT_Config);
  }
  return tPartialCriterionWRT_Config;
}

template<typename PhysicsType>
Plato::ScalarVector
Problem<PhysicsType>::criterionGradient(
    const Plato::ScalarVector & aControls,
    const std::string         & aName
)
{
  if( mCriteria.count(aName) )
  {
      Plato::Solutions tSolution(mPhysics);
      tSolution.set("State", mStates);
      Criterion tCriterion = mCriteria[aName];
      return criterionGradient(aControls, tSolution, tCriterion);
  }
  else
  if( mLinearCriteria.count(aName) )
  {
      LinearCriterion tCriterion = mLinearCriteria[aName];
      return tCriterion->gradient_z(aControls);
  }
  else
  {
      ANALYZE_THROWERR(std::string("CRITERION WITH NAME '") + aName + "' IS NOT DEFINED IN THE CRITERION MAP.");
  }
}

template<typename PhysicsType>
Plato::ScalarVector
Problem<PhysicsType>::
criterionGradientX(
    const Plato::ScalarVector & aControls,
    const std::string         & aName
)
{
  if( mCriteria.count(aName) )
  {
      Plato::Solutions tSolution(mPhysics);
      tSolution.set("State", mStates);
      Criterion tCriterion = mCriteria[aName];
      return criterionGradientX(aControls, tSolution, tCriterion);
  }
  else
  if( mLinearCriteria.count(aName) )
  {
      LinearCriterion tCriterion = mLinearCriteria[aName];
      return tCriterion->gradient_x(aControls);
  }
  else
  {
      ANALYZE_THROWERR(std::string("CRITERION WITH NAME '") + aName + "' IS NOT DEFINED IN THE CRITERION MAP.");
  }
}

template<typename PhysicsType>
void Problem<PhysicsType>::
readEssentialBoundaryConditions(
    Teuchos::ParameterList& aProblemParams
)
{
  if(aProblemParams.isSublist("Essential Boundary Conditions") == false)
  {
      ANALYZE_THROWERR("ESSENTIAL BOUNDARY CONDITIONS SUBLIST IS NOT DEFINED IN THE INPUT FILE.")
  }
  Plato::EssentialBCs<ElementType>
  tEssentialBoundaryConditions(aProblemParams.sublist("Essential Boundary Conditions", false), mSpatialModel.Mesh);
  tEssentialBoundaryConditions.get(mBcDofs, mBcValues);
  if(mMPCs)
  {
      mMPCs->checkEssentialBcsConflicts(mBcDofs);
  }
}

template<typename PhysicsType>
void Problem<PhysicsType>::
setEssentialBoundaryConditions(
    const Plato::OrdinalVector & aDofs, 
    const Plato::ScalarVector & aValues
)
{
  if(aDofs.size() != aValues.size())
  {
      std::ostringstream tError;
      tError << "DIMENSION MISMATCH: THE NUMBER OF ELEMENTS IN INPUT DOFS AND VALUES ARRAY DO NOT MATCH."
          << "DOFS SIZE = " << aDofs.size() << " AND VALUES SIZE = " << aValues.size();
      ANALYZE_THROWERR(tError.str())
  }
  mBcDofs = aDofs;
  mBcValues = aValues;
}

template<typename PhysicsType>
void Problem<PhysicsType>::
initialize(
    Teuchos::ParameterList& aProblemParams
)
{
  auto tName = aProblemParams.get<std::string>("PDE Constraint");
  mPDE = std::make_shared<Plato::Elliptic::VectorFunction<PhysicsType>>(mSpatialModel,mDataMap,aProblemParams,tName);
  if(aProblemParams.isSublist("Criteria"))
  {
    Plato::Geometric::ScalarFunctionBaseFactory<Plato::Geometrical<TopoElementType>> tLinearFunctionBaseFactory;
    Plato::Elliptic::FactoryCriterionEvaluator<PhysicsType> tNonlinearFunctionBaseFactory;
    auto tCriteriaParams = aProblemParams.sublist("Criteria");
    for(Teuchos::ParameterList::ConstIterator tIndex = tCriteriaParams.begin(); 
      tIndex != tCriteriaParams.end(); 
      ++tIndex)
    {
      const Teuchos::ParameterEntry & tEntry = tCriteriaParams.entry(tIndex);
      std::string tName = tCriteriaParams.name(tIndex);
      TEUCHOS_TEST_FOR_EXCEPTION(!tEntry.isList(), std::logic_error,
        " Parameter in Criteria block not valid.  Expect lists only.");
      if( tCriteriaParams.sublist(tName).get<bool>("Linear", false) == true )
      {
          auto tCriterion = tLinearFunctionBaseFactory.create(mSpatialModel, mDataMap, aProblemParams, tName);
          if( tCriterion != nullptr )
          {
              mLinearCriteria[tName] = tCriterion;
          }
      }
      else
      {
          auto tCriterion = tNonlinearFunctionBaseFactory.create(mSpatialModel, mDataMap, aProblemParams, tName);
          if( tCriterion != nullptr )
          {
              mCriteria[tName] = tCriterion;
          }
      }
    }
    if( mCriteria.size() )
    {
      auto tLength = mPDE->size();
      mAdjoint = Plato::ScalarMultiVector("Adjoint Variables", 1, tLength);
    }
  }
  if(aProblemParams.isSublist("Multipoint Constraints") == true)
  {
    Plato::OrdinalType tNumDofsPerNode = mPDE->numDofsPerNode();
    auto & tMyParams = aProblemParams.sublist("Multipoint Constraints", false);
    mMPCs = std::make_shared<Plato::MultipointConstraints>(mSpatialModel, tNumDofsPerNode, tMyParams);
    mMPCs->setupTransform();
  }
  this->readEssentialBoundaryConditions(aProblemParams);
}

template<typename PhysicsType>
void Problem<PhysicsType>::
applyAdjointConstraints(
    const Teuchos::RCP<Plato::CrsMatrixType> & aMatrix, 
    const Plato::ScalarVector & aVector
)
{
  Plato::ScalarVector tDirichletValues("Dirichlet Values For Adjoint Problem", mBcValues.size());
  Plato::blas1::scale(static_cast<Plato::Scalar>(0.0), tDirichletValues);
  if(mJacobian->isBlockMatrix())
  {
      Plato::applyBlockConstraints<ElementType::mNumDofsPerNode>(aMatrix, aVector, mBcDofs, tDirichletValues);
  }
  else
  {
      Plato::applyConstraints<ElementType::mNumDofsPerNode>(aMatrix, aVector, mBcDofs, tDirichletValues);
  }
}

template<typename PhysicsType>
Plato::Solutions Problem<PhysicsType>::
getSolution() 
const
{
  Plato::Solutions tSolution(mPhysics, mPDEType);
  tSolution.set("State", mStates, mPDE->getDofNames());
  return tSolution;
}

} // namespace Elliptic

} // namespace Plato
