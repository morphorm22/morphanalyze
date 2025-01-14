#pragma once

#include "PlatoMesh.hpp"
#include "SpatialModel.hpp"
#include "solver/ParallelComm.hpp"
#include "PlatoAbstractProblem.hpp"
#include "solver/PlatoSolverFactory.hpp"
#include "elliptic/base/VectorFunction.hpp"
#include "elliptic/criterioneval/CriterionEvaluatorBase.hpp"

namespace Plato
{

namespace Elliptic
{

/******************************************************************************//**
 * \brief Manage scalar and vector function evaluations
**********************************************************************************/
template<typename PhysicsType>
class Problem: public Plato::AbstractProblem
{
private:
  /// @brief local criterion typename
  using Criterion = std::shared_ptr<Plato::Elliptic::CriterionEvaluatorBase>;
  /// @brief local typename for map from criterion name to criterion evaluator
  using Criteria  = std::map<std::string, Criterion>;
  /// @brief local typename for physics-based element interface
  using ElementType     = typename PhysicsType::ElementType;
  /// @brief local typename for topological element
  using TopoElementType = typename ElementType::TopoElementType;
  /// @brief local typename for vector function evaluator
  using VectorFunctionType = Plato::Elliptic::VectorFunction<PhysicsType>;

  /// @brief contains mesh and model information
  Plato::SpatialModel mSpatialModel;
  /// @brief residual evaluator
  std::shared_ptr<VectorFunctionType> mResidualEvaluator; 
  /// @brief map from criterion name to criterion evaluator
  Criteria mCriterionEvaluator;
  /// @brief number of newton steps/cycles
  Plato::OrdinalType mNumNewtonSteps;
  /// @brief residual and increment tolerances for newton solver
  Plato::Scalar      mNewtonResTol, mNewtonIncTol;

  /// @brief save state if true
  bool mSaveState = false;
  /// @brief apply dirichlet boundary condition weakly
  bool mWeakEBCs = false;
  /// @brief vector of adjoint values
  Plato::ScalarMultiVector mAdjoints;
  /// @brief scalar residual vector
  Plato::ScalarVector mResidual;
  /// @brief vector of state values
  Plato::ScalarMultiVector mStates; 
  /// @brief jacobian matrix
  Teuchos::RCP<Plato::CrsMatrixType> mJacobianState;

  /// @brief dirichlet degrees of freedom
  Plato::OrdinalVector mDirichletDofs; 
  /// @brief dirichlet degrees of freedom state values
  Plato::ScalarVector mDirichletStateVals; 
  /// @brief dirichlet degrees of freedom adjoint values
  Plato::ScalarVector mDirichletAdjointVals;

  /// @brief multipoint constraint interface
  std::shared_ptr<Plato::MultipointConstraints> mMPCs;
  /// @brief linear solver interface
  std::shared_ptr<Plato::AbstractSolver> mSolver;

  /// @brief partial differential equation type
  std::string mTypePDE; 
  /// @brief simulated physics
  std::string mPhysics; 

public:
  /// @brief class constructor
  /// @param aMesh       mesh interface
  /// @param aParamList input problem parameters
  /// @param aMachine    mpi wrapper
  Problem(
    Plato::Mesh              aMesh,
    Teuchos::ParameterList & aParamList,
    Plato::Comm::Machine     aMachine
  );

  /// class destructor
  ~Problem();

  /// @fn numNodes
  /// @brief return total number of nodes/vertices
  /// @return integer
  Plato::OrdinalType numNodes() const;

  /// @fn numCells
  /// @brief return total number of cells/elements
  /// @return integer
  Plato::OrdinalType numCells() const;

  /// @fn numDofsPerCell
  /// @brief return number of degrees of freedom per cell
  /// @return integer
  Plato::OrdinalType numDofsPerCell() const;

  /// @fn numNodesPerCell
  /// @brief return number of nodes per cell
  /// @return integer
  Plato::OrdinalType numNodesPerCell() const;

  /// @fn numDofsPerNode
  /// @brief return number of state degrees of freedom per node
  /// @return integer
  Plato::OrdinalType numDofsPerNode() const;

  /// @fn numControlDofsPerNode
  /// @brief return number of control degrees of freedom per node
  /// @return integer
  Plato::OrdinalType numControlDofsPerNode() const;
  
  /// @fn criterionIsLinear
  /// @brief return true if criterion is linear
  /// @param [in] aName criterion name 
  /// @return boolean
  bool 
  criterionIsLinear( 
    const std::string & aName
  ) override;

  /// @brief output state solution and requested quantities of interests to visualization file
  /// @param [in] aFilepath output file name 
  void 
  output(
    const std::string & aFilepath
  );

  /// @fn updateProblem
  /// @brief update criterion parameters at runtime
  /// @param [in] aControl  control variables
  /// @param [in] aSolution current state solution
  void 
  updateProblem(
    const Plato::ScalarVector & aControl, 
    const Plato::Solutions & aSolution
  );

  /// @fn solution
  /// @brief solve for state solution
  /// @param [in] aControl control variables
  /// @return state solution database
  Plato::Solutions 
  solution(
    const Plato::ScalarVector & aControl
  );

  /// @fn criterionValue
  /// @brief evaluate criterion
  /// @param [in] aControl control variables
  /// @param [in] aName    criterion name
  /// @return scalar
  Plato::Scalar
  criterionValue(
      const Plato::ScalarVector & aControl,
      const std::string         & aName
  );

  /// @fn criterionValue
  /// @brief evaluate criterion
  /// @param [in] aControl  control variables
  /// @param [in] aSolution current state solution
  /// @param [in] aName     criterion name
  /// @return scalar
  Plato::Scalar
  criterionValue(
      const Plato::ScalarVector & aControl,
      const Plato::Solutions    & aSolution,
      const std::string         & aName
  );

    /******************************************************************************//**
     * \brief Evaluate criterion gradient wrt control variables
     * \param [in] aControl 1D view of control variables
     * \param [in] aSolution solution database
     * \param [in] aName Name of criterion.
     * \return 1D view - criterion gradient wrt control variables
    **********************************************************************************/
    Plato::ScalarVector
    criterionGradient(
        const Plato::ScalarVector & aControl,
        const Plato::Solutions    & aSolution,
        const std::string         & aName
    ) override;

    /******************************************************************************//**
     * \brief Evaluate criterion gradient wrt control variables
     * \param [in] aControl 1D view of control variables
     * \param [in] aSolution solution database
     * \param [in] aCriterion criterion to be evaluated
     * \return 1D view - criterion gradient wrt control variables
    **********************************************************************************/
    Plato::ScalarVector
    criterionGradient(
        const Plato::ScalarVector & aControl,
        const Plato::Solutions    & aSolution,
              Criterion             aCriterion);

    /******************************************************************************//**
     * \brief Evaluate criterion gradient wrt configuration variables
     * \param [in] aControl 1D view of control variables
     * \param [in] aSolution solution database
     * \param [in] aName Name of criterion.
     * \return 1D view - criterion gradient wrt control variables
    **********************************************************************************/
    Plato::ScalarVector
    criterionGradientX(
        const Plato::ScalarVector & aControl,
        const Plato::Solutions    & aSolution,
        const std::string         & aName
    ) override;

    /******************************************************************************//**
     * \brief Evaluate criterion gradient wrt configuration variables
     * \param [in] aControl 1D view of control variables
     * \param [in] aSolution solution database
     * \param [in] aCriterion criterion to be evaluated
     * \return 1D view - criterion gradient wrt configuration variables
    **********************************************************************************/
    Plato::ScalarVector
    criterionGradientX(
        const Plato::ScalarVector & aControl,
        const Plato::Solutions    & aSolution,
              Criterion             aCriterion);

    /******************************************************************************//**
     * \brief Evaluate criterion partial derivative wrt control variables
     * \param [in] aControl 1D view of control variables
     * \param [in] aName Name of criterion.
     * \return 1D view - criterion partial derivative wrt control variables
    **********************************************************************************/
    Plato::ScalarVector
    criterionGradient(
        const Plato::ScalarVector & aControl,
        const std::string         & aName
    ) override;

    /******************************************************************************//**
     * \brief Evaluate criterion partial derivative wrt configuration variables
     * \param [in] aControl 1D view of control variables
     * \param [in] aName Name of criterion.
     * \return 1D view - criterion partial derivative wrt configuration variables
    **********************************************************************************/
    Plato::ScalarVector
    criterionGradientX(
        const Plato::ScalarVector & aControl,
        const std::string         & aName
    ) override;

    /***************************************************************************//**
     * \brief Read essential (Dirichlet) boundary conditions from the Exodus file.
     * \param [in] aParamList input parameters database
    *******************************************************************************/
    void 
    readEssentialBoundaryConditions(
      Teuchos::ParameterList & aParamList
    );

    /***************************************************************************//**
     * \brief Set essential (Dirichlet) boundary conditions
     * \param [in] aDofs   degrees of freedom associated with Dirichlet boundary conditions
     * \param [in] aValues values associated with Dirichlet degrees of freedom
    *******************************************************************************/
    void 
    setEssentialBoundaryConditions(
      const Plato::OrdinalVector & aDofs, 
      const Plato::ScalarVector  & aValues
    );

private:
  /// @brief parse and set save output flag
  /// @param [in] aParamList input problem parameters
  void 
  parseSaveOutput(
    Teuchos::ParameterList & aParamList
  );

  /// @brief initialize linear system solver
  /// @param aMesh       mesh interface
  /// @param aParamList input problem parameters
  /// @param aMachine    mpi wrapper
  void 
  initializeSolver(
    Plato::Mesh            & aMesh,
    Teuchos::ParameterList & aParamList,
    Comm::Machine          & aMachine
  );

  /// @brief initialize multi-point constraint interface
  /// @param aParamList input problem parameters
  void 
  initializeMultiPointConstraints(
    Teuchos::ParameterList& aParamList
  );

  /// @brief initialize criteria and residual evaluators
  /// @param aParamList input problem parameters
  void 
  initializeEvaluators(
    Teuchos::ParameterList& aParamList
  );

  /// @brief return solution database
  /// @return solutions database
  Plato::Solutions 
  getSolution() 
  const override;
  
  std::string
  getErrorMsg(
    const std::string & aName
  ) const;

  void
  buildDatabase(
    const Plato::ScalarVector & aControl,
          Plato::Database     & aDatabase
  );

  Plato::ScalarVector
  computeCriterionGradientControl(
    Plato::Database & aDatabase,
    Criterion       & aCriterion
  );

  Plato::ScalarVector
  computeCriterionGradientConfig(
    Plato::Database & aDatabase,
    Criterion       & aCriterion
  );

  void
  enforceStrongEssentialBoundaryConditions(
    const Teuchos::RCP<Plato::CrsMatrixType> & aMatrix,
    const Plato::ScalarVector                & aVector,
    const Plato::Scalar                      & aMultiplier
  );

  void 
  enforceStrongEssentialAdjointBoundaryConditions(
    const Teuchos::RCP<Plato::CrsMatrixType> & aMatrix,
    const Plato::ScalarVector                & aVector
  );

  void 
  enforceWeakEssentialAdjointBoundaryConditions(
    Plato::Database & aDatabase
  );

};
// class Problem

} // namespace Elliptic

} // namespace Plato
