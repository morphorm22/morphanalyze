/*
 * ElastoPlasticityTest.cpp
 *
 *  Created on: Sep 30, 2019
 */



#include "ApplyWeighting.hpp"
#include "EllipticProblem.hpp"
#include "PhysicsScalarFunction.hpp"
#include "AbstractScalarFunction.hpp"



#include "Teuchos_UnitTestHarness.hpp"

#include "PlatoTestHelpers.hpp"
#include "PlatoUtilities.hpp"

#include <memory>
#include <limits>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "MaximizePlasticWork.hpp"
#include "GlobalVectorFunctionInc.hpp"
#include "InfinitesimalStrainPlasticity.hpp"
#include "DoubleDotProduct2ndOrderTensor.hpp"
#include "PathDependentScalarFunctionFactory.hpp"
#include "InfinitesimalStrainPlasticityResidual.hpp"

#include "Simplex.hpp"
#include "Kinetics.hpp"
#include "ParseTools.hpp"

#include "Projection.hpp"
#include "WorksetBase.hpp"
#include "EssentialBCs.hpp"


#include "SimplexFadTypes.hpp"


#include "VectorFunctionVMS.hpp"
#include "PlatoStaticsTypes.hpp"
#include "Plato_Diagnostics.hpp"
#include "ScalarFunctionBase.hpp"

#include "StabilizedMechanics.hpp"
#include "PlatoAbstractProblem.hpp"
#include "Plato_TopOptFunctors.hpp"

#include "LinearElasticMaterial.hpp"
#include "ScalarFunctionIncBase.hpp"

#include "LocalVectorFunctionInc.hpp"

#include "LinearTetCubRuleDegreeOne.hpp"


#include "Plato_Solve.hpp"
#include "ApplyConstraints.hpp"
#include "ScalarFunctionBaseFactory.hpp"
#include "ScalarFunctionIncBaseFactory.hpp"

#include "BLAS2.hpp"
#include "BLAS3.hpp"

#include <Teuchos_XMLParameterListCoreHelpers.hpp>

namespace Plato
{




















struct NewtonRaphson
{
    enum stop_t
    {
        DID_NOT_CONVERGE = 0,
        MAX_NUMBER_ITERATIONS = 1,
        RELATIVE_NORM_TOLERANCE = 2,
        CURRENT_NORM_TOLERANCE = 3,
    };

    enum measure_t
    {
        RESIDUAL_NORM = 0,
        DISPLACEMENT_NORM = 1,
        RELATIVE_RESIDUAL_NORM = 2,
    };
};

struct PartialDerivative
{
    enum derivative_t
    {
        CONTROL = 0,
        CONFIGURATION = 1,
    };
};

/***************************************************************************//**
 * \brief Data structure used to solve forward problem in Plasticity Problem class.
 * The Plasticity Problem interface is responsible of evaluating the system of
 * forward and adjoint equations as well as assembling the total gradient with
 * respect to the variables of interest, e.g. design variables & configurations.
*******************************************************************************/
struct ForwardProblemStates
{
    Plato::OrdinalType mCurrentStepIndex;      /*!< current time step index */

    Plato::ScalarVector mDeltaGlobalState;     /*!< global state increment */
    Plato::ScalarVector mCurrentLocalState;    /*!< current local state */
    Plato::ScalarVector mPreviousLocalState;   /*!< previous local state */
    Plato::ScalarVector mCurrentGlobalState;   /*!< current global state */
    Plato::ScalarVector mPreviousGlobalState;  /*!< previous global state */
    Plato::ScalarVector mProjectedPressGrad;   /*!< current projected pressure gradient */
};
// struct StateData

/***************************************************************************//**
 * \brief Data structure used to solve adjoint problem in Plasticity Problem class.
 * The Plasticity Problem interface is responsible of evaluating the system of
 * forward and adjoint equations as well as assembling the total gradient with
 * respect to the variables of interest, e.g. design variables & configurations.
*******************************************************************************/
struct StateData
{
    Plato::OrdinalType mCurrentStepIndex;      /*!< current time step index */
    Plato::ScalarVector mCurrentLocalState;    /*!< current local state */
    Plato::ScalarVector mPreviousLocalState;   /*!< previous local state */
    Plato::ScalarVector mCurrentGlobalState;   /*!< current global state */
    Plato::ScalarVector mPreviousGlobalState;  /*!< previous global state */
    Plato::ScalarVector mProjectedPressGrad;   /*!< projected pressure gradient at time step k-1, where k is the step index */

    Plato::PartialDerivative::derivative_t mPartialDerivativeType;

    explicit StateData(const Plato::PartialDerivative::derivative_t &aType) :
        mCurrentStepIndex(0),
        mPartialDerivativeType(aType)
    {
    }

    ~StateData(){}
};
// struct StateData

struct AdjointProblemStates
{
    AdjointProblemStates(const Plato::OrdinalType & aNumGlobalAdjointVars,
                const Plato::OrdinalType & aNumLocalAdjointVars,
                const Plato::OrdinalType & aNumProjPressGradAdjointVars) :
            mCurrentLocalAdjoint(Plato::ScalarVector("Current Local Adjoint", aNumLocalAdjointVars)),
            mPreviousLocalAdjoint(Plato::ScalarVector("Previous Local Adjoint", aNumLocalAdjointVars)),
            mCurrentGlobalAdjoint(Plato::ScalarVector("Current Global Adjoint", aNumGlobalAdjointVars)),
            mPreviousGlobalAdjoint(Plato::ScalarVector("Previous Global Adjoint", aNumGlobalAdjointVars)),
            mProjPressGradAdjoint(Plato::ScalarVector("Current Projected Pressure Gradient Adjoint", aNumProjPressGradAdjointVars)),
            mPreviousProjPressGradAdjoint(Plato::ScalarVector("Previous Projected Pressure Gradient Adjoint", aNumProjPressGradAdjointVars))
    {
    }

    ~AdjointProblemStates(){}

    Plato::ScalarVector mCurrentLocalAdjoint;          /*!< current local adjoint */
    Plato::ScalarVector mPreviousLocalAdjoint;         /*!< previous local adjoint */
    Plato::ScalarVector mCurrentGlobalAdjoint;         /*!< current global adjoint */
    Plato::ScalarVector mPreviousGlobalAdjoint;        /*!< previous global adjoint */
    Plato::ScalarVector mProjPressGradAdjoint;  /*!< projected pressure adjoint */
    Plato::ScalarVector mPreviousProjPressGradAdjoint; /*!< projected pressure adjoint */
};
// struct AdjointProblemStates

struct NewtonRaphsonOutputData
{
    bool mWriteOutput;              /*!< flag: true = write output; false = do not write output */
    Plato::Scalar mCurrentNorm;     /*!< current norm */
    Plato::Scalar mRelativeNorm;    /*!< relative norm */
    Plato::Scalar mReferenceNorm;   /*!< reference norm */

    Plato::OrdinalType mCurrentIteration;             /*!< current Newton-Raphson solver iteration */
    Plato::NewtonRaphson::stop_t mStopingCriterion;   /*!< stopping criterion */
    Plato::NewtonRaphson::measure_t mStoppingMeasure; /*!< stopping criterion measure */

    NewtonRaphsonOutputData() :
        mWriteOutput(true),
        mCurrentNorm(1.0),
        mReferenceNorm(0.0),
        mRelativeNorm(1.0),
        mCurrentIteration(0),
        mStopingCriterion(Plato::NewtonRaphson::DID_NOT_CONVERGE),
        mStoppingMeasure(Plato::NewtonRaphson::RESIDUAL_NORM)
    {}
};
// struct NewtonRaphsonOutputData








/******************************************************************************//**
 * \brief Write a brief sentence explaining why Newton-Raphson algorithm stop.
 * \param [in] aStopCriterion stopping criterion flag
 * \param [in,out] aOutput string with brief description
**********************************************************************************/
inline void print_newton_raphson_stop_criterion(const Plato::NewtonRaphsonOutputData & aOutputData, std::ofstream &aOutputFile)
{
    if(aOutputData.mWriteOutput == false)
    {
        return;
    }

    if(aOutputFile.is_open() == false)
    {
        THROWERR("Newton-Raphson solver diagnostic file is closed.")
    }

    switch(aOutputData.mStopingCriterion)
    {
        case Plato::NewtonRaphson::MAX_NUMBER_ITERATIONS:
        {
            aOutputFile << "\n\n****** Newton-Raphson solver stopping due to exceeding maximum number of iterations. ******\n\n";
            break;
        }
        case Plato::NewtonRaphson::RELATIVE_NORM_TOLERANCE:
        {
            aOutputFile << "\n\n******  Newton-Raphson algorithm stopping due to relative norm tolerance being met. ******\n\n";
            break;
        }
        case Plato::NewtonRaphson::CURRENT_NORM_TOLERANCE:
        {
            aOutputFile << "\n\n******  Newton-Raphson algorithm stopping due to current norm tolerance being met. ******\n\n";
            break;
        }
        case Plato::NewtonRaphson::DID_NOT_CONVERGE:
        {
            aOutputFile << "\n\n****** Newton-Raphson algorithm did not converge. ******\n\n";
            break;
        }
        default:
        {
            aOutputFile << "\n\n****** ERROR: Optimization algorithm stopping due to undefined behavior. ******\n\n";
            break;
        }
    }
}
// function print_newton_raphson_stop_criterion

inline void print_newton_raphson_diagnostics(const Plato::NewtonRaphsonOutputData & aOutputData, std::ofstream &aOutputFile)
{
    if(aOutputData.mWriteOutput == false)
    {
        return;
    }

    if(aOutputFile.is_open() == false)
    {
        THROWERR("Newton-Raphson solver diagnostic file is closed.")
    }

    aOutputFile << std::scientific << std::setprecision(6) << aOutputData.mCurrentIteration << std::setw(20)
        << aOutputData.mCurrentNorm << std::setw(20) << aOutputData.mRelativeNorm << "\n" << std::flush;
}
// function print_newton_raphson_diagnostics

inline void print_newton_raphson_diagnostics_header(const Plato::NewtonRaphsonOutputData &aOutputData, std::ofstream &aOutputFile)
{
    if(aOutputData.mWriteOutput == false)
    {
        return;
    }

    if(aOutputFile.is_open() == false)
    {
        THROWERR("Newton-Raphson solver diagnostic file is closed.")
    }

    aOutputFile << std::scientific << std::setprecision(6) << std::right << "Iter" << std::setw(13)
        << "Norm" << std::setw(22) << "Relative" "\n" << std::flush;
}
// function print_newton_raphson_diagnostics_header

inline void compute_relative_residual_norm_criterion(const Plato::ScalarVector & aResidual, Plato::NewtonRaphsonOutputData & aOutputData)
{
    if(aOutputData.mCurrentIteration == static_cast<Plato::OrdinalType>(0))
    {
        aOutputData.mReferenceNorm = Plato::norm(aResidual);
        aOutputData.mCurrentNorm = aOutputData.mReferenceNorm;
    }
    else
    {
        aOutputData.mCurrentNorm = Plato::norm(aResidual);
        aOutputData.mRelativeNorm = std::abs(aOutputData.mCurrentNorm - aOutputData.mReferenceNorm);
        aOutputData.mReferenceNorm = aOutputData.mCurrentNorm;
    }
}








template<typename PhysicsT>
class NewtonRaphsonSolver
{
private:
    static constexpr auto mNumSpatialDims = PhysicsT::mNumSpatialDims;           /*!< spatial dimensions*/
    static constexpr auto mNumGlobalDofsPerCell = PhysicsT::mNumDofsPerCell;     /*!< number of global degrees of freedom per node*/
    static constexpr auto mNumGlobalDofsPerNode = PhysicsT::mNumDofsPerNode;     /*!< number of global degrees of freedom per node*/
    static constexpr auto mNumLocalDofsPerCell = PhysicsT::mNumLocalDofsPerCell; /*!< number of local degrees of freedom per cell (i.e. element)*/

    using LocalPhysicsT = typename Plato::Plasticity<mNumSpatialDims>;
    std::shared_ptr<Plato::GlobalVectorFunctionInc<PhysicsT>> mGlobalEquation;    /*!< global state residual interface */
    std::shared_ptr<Plato::LocalVectorFunctionInc<LocalPhysicsT>> mLocalEquation; /*!< local state residual interface*/

    Plato::WorksetBase<Plato::SimplexPlasticity<mNumSpatialDims>> mWorksetBase;     /*!< interface for assembly routines */

    Plato::Scalar mStoppingTolerance;            /*!< stopping tolerance*/
    Plato::Scalar mDirichletValuesMultiplier;
    Plato::Scalar mCurrentResidualNormTolerance; /*!< current residual norm stopping tolerance - avoids unnecessary solves */

    Plato::OrdinalType mMaxNumSolverIter;  /*!< maximum number of iterations */
    Plato::OrdinalType mCurrentSolverIter; /*!< current number of iterations */

    Plato::ScalarVector mDirichletValues;     /*!< Dirichlet boundary conditions values */
    Plato::LocalOrdinalVector mDirichletDofs; /*!< Dirichlet boundary conditions degrees of freedom */

    bool mUseAbsoluteTolerance;   /*!< use absolute stopping tolerance flag */
    bool mWriteSolverDiagnostics; /*!< write solver diagnostics flag */

    std::ofstream mSolverDiagnosticsFile; /*!< output solver diagnostics */

public:
    NewtonRaphsonSolver(Omega_h::Mesh& aMesh, Teuchos::ParameterList& aInputs) :
        mWorksetBase(aMesh),
        mStoppingTolerance(Plato::ParseTools::getSubParam<Plato::Scalar>(aInputs, "Newton-Raphson", "Stopping Tolerance", 1e-6)),
        mDirichletValuesMultiplier(1),
        mCurrentResidualNormTolerance(Plato::ParseTools::getSubParam<Plato::Scalar>(aInputs, "Newton-Raphson", "Current Residual Norm Stopping Tolerance", 1e-10)),
        mMaxNumSolverIter(Plato::ParseTools::getSubParam<Plato::OrdinalType>(aInputs, "Newton-Raphson", "Maximum Number Iterations", 10)),
        mCurrentSolverIter(0),
        mUseAbsoluteTolerance(false),
        mWriteSolverDiagnostics(true)
    {
        this->openDiagnosticsFile();
        auto tInitialNumTimeSteps = Plato::ParseTools::getSubParam<Plato::OrdinalType>(aInputs, "Time Stepping", "Initial Num. Pseudo Time Steps", 20);
        mDirichletValuesMultiplier = static_cast<Plato::Scalar>(1.0) / static_cast<Plato::Scalar>(tInitialNumTimeSteps);
    }

    explicit NewtonRaphsonSolver(Omega_h::Mesh& aMesh) :
        mWorksetBase(aMesh),
        mStoppingTolerance(1e-6),
        mDirichletValuesMultiplier(1),
        mCurrentResidualNormTolerance(1e-10),
        mMaxNumSolverIter(20),
        mCurrentSolverIter(0),
        mUseAbsoluteTolerance(false),
        mWriteSolverDiagnostics(true)
    {
        this->openDiagnosticsFile();
    }

    ~NewtonRaphsonSolver()
    {
        this->closeDiagnosticsFile();
    }

    void setDirichletValuesMultiplier(const Plato::Scalar & aInput)
    {
        mDirichletValuesMultiplier = aInput;
    }

    void appendLocalEquation(const std::shared_ptr<Plato::LocalVectorFunctionInc<LocalPhysicsT>> & aInput)
    {
        mLocalEquation = aInput;
    }

    void appendGlobalEquation(const std::shared_ptr<Plato::GlobalVectorFunctionInc<PhysicsT>> & aInput)
    {
        mGlobalEquation = aInput;
    }

    void appendDirichletValues(const Plato::ScalarVector & aInput)
    {
        mDirichletValues = aInput;
    }

    void appendDirichletDofs(const Plato::LocalOrdinalVector & aInput)
    {
        mDirichletDofs = aInput;
    }

    void appendOutputMessage(const std::stringstream & aInput)
    {
        mSolverDiagnosticsFile << aInput.str().c_str();
    }

    void openDiagnosticsFile()
    {
        if (mWriteSolverDiagnostics == false)
        {
            return;
        }

        mSolverDiagnosticsFile.open("plato_analyze_newton_raphson_diagnostics.txt");
    }

    void closeDiagnosticsFile()
    {
        if (mWriteSolverDiagnostics == false)
        {
            return;
        }

        mSolverDiagnosticsFile.close();
    }

    void updateInverseLocalJacobian(const Plato::ScalarVector & aControls,
                                    const Plato::ForwardProblemStates & aStates,
                                    Plato::ScalarArray3D& aInvLocalJacobianT)
    {
        auto tNumCells = mLocalEquation->numCells();
        auto tDhDc = mLocalEquation->gradient_c(aStates.mCurrentGlobalState, aStates.mPreviousGlobalState,
                                                  aStates.mCurrentLocalState , aStates.mPreviousLocalState,
                                                  aControls, aStates.mCurrentStepIndex);
        Plato::inverse_matrix_workset<mNumLocalDofsPerCell, mNumLocalDofsPerCell>(tNumCells, tDhDc, aInvLocalJacobianT);
    }

    void applyConstraints(const Teuchos::RCP<Plato::CrsMatrixType> & aMatrix, const Plato::ScalarVector & aResidual)
    {
        Plato::ScalarVector tDispControlledDirichletValues("Dirichlet Values", mDirichletValues.size());
        Plato::fill(0.0, tDispControlledDirichletValues);
        if(mCurrentSolverIter == static_cast<Plato::OrdinalType>(0))
        {
            Plato::update(mDirichletValuesMultiplier, mDirichletValues, static_cast<Plato::Scalar>(0.), tDispControlledDirichletValues);
        }

        if(aMatrix->isBlockMatrix())
        {
            Plato::applyBlockConstraints<mNumGlobalDofsPerNode>(aMatrix, aResidual, mDirichletDofs, tDispControlledDirichletValues);
        }
        else
        {
            Plato::applyConstraints<mNumGlobalDofsPerNode>(aMatrix, aResidual, mDirichletDofs, tDispControlledDirichletValues);
        }
    }

    void updateGlobalStates(Teuchos::RCP<Plato::CrsMatrixType> & aMatrix,
                            Plato::ScalarVector aResidual,
                            Plato::ForwardProblemStates &aStates)
    {
        const Plato::Scalar tAlpha = 1.0;
        Plato::fill(static_cast<Plato::Scalar>(0.0), aStates.mDeltaGlobalState);
        Plato::Solve::Consistent<mNumGlobalDofsPerNode>(aMatrix, aStates.mDeltaGlobalState, aResidual, mUseAbsoluteTolerance);
        Plato::update(tAlpha, aStates.mDeltaGlobalState, tAlpha, aStates.mCurrentGlobalState);
    }

    Plato::ScalarArray3D computeSchurComplement(const Plato::ScalarVector & aControls,
                                                const ForwardProblemStates & aStates,
                                                const Plato::ScalarArray3D & aInvLocalJacobianT)
    {
        // Compute cell Jacobian of the local residual with respect to the current global state WorkSet (WS)
        auto tDhDu = mLocalEquation->gradient_u(aStates.mCurrentGlobalState, aStates.mPreviousGlobalState,
                                                 aStates.mCurrentLocalState, aStates.mPreviousLocalState,
                                                 aControls, aStates.mCurrentStepIndex);

        // Compute cell C = (dH/dc)^{-1}*dH/du, where H is the local residual, c are the local states and u are the global states
        Plato::Scalar tBeta = 0.0;
        const Plato::Scalar tAlpha = 1.0;
        auto tNumCells = mLocalEquation->numCells();
        Plato::ScalarArray3D tInvDhDcTimesDhDu("InvDhDc times DhDu", tNumCells, mNumLocalDofsPerCell, mNumGlobalDofsPerCell);
        Plato::multiply_matrix_workset(tNumCells, tAlpha, aInvLocalJacobianT, tDhDu, tBeta, tInvDhDcTimesDhDu);

        // Compute cell Jacobian of the global residual with respect to the current local state WorkSet (WS)
        auto tDrDc = mGlobalEquation->gradient_c(aStates.mCurrentGlobalState, aStates.mPreviousGlobalState,
                                                  aStates.mCurrentLocalState, aStates.mPreviousLocalState,
                                                  aStates.mProjectedPressGrad, aControls, aStates.mCurrentStepIndex);

        // Compute cell Schur = dR/dc * (dH/dc)^{-1} * dH/du, where H is the local residual,
        // R is the global residual, c are the local states and u are the global states
        Plato::ScalarArray3D tSchurComplement("Schur Complement", tNumCells, mNumGlobalDofsPerCell, mNumGlobalDofsPerCell);
        Plato::multiply_matrix_workset(tNumCells, tAlpha, tDrDc, tInvDhDcTimesDhDu, tBeta, tSchurComplement);

        return tSchurComplement;
    }

    Teuchos::RCP<Plato::CrsMatrixType>
    assembleTangentMatrix(const Plato::ScalarVector & aControls,
                          const Plato::ForwardProblemStates & aStates,
                          const Plato::ScalarArray3D& aInvLocalJacobianT)
    {
        // Compute cell Schur Complement, i.e. dR/dc * (dH/dc)^{-1} * dH/du, where H is the local
        // residual, R is the global residual, c are the local states and u are the global states
        auto tSchurComplement = this->computeSchurComplement(aControls, aStates, aInvLocalJacobianT);

        // Compute cell Jacobian of the global residual with respect to the current global state WorkSet (WS)
        auto tDrDu = mGlobalEquation->gradient_u(aStates.mCurrentGlobalState, aStates.mPreviousGlobalState,
                                                   aStates.mCurrentLocalState, aStates.mPreviousLocalState,
                                                   aStates.mProjectedPressGrad, aControls, aStates.mCurrentStepIndex);

        // Add cell Schur complement to dR/du, where R is the global residual and u are the global states
        const Plato::Scalar tBeta = 1.0;
        const Plato::Scalar tAlpha = -1.0;
        auto tNumCells = mGlobalEquation->numCells();
        Plato::update_array_3D(tNumCells, tAlpha, tSchurComplement, tBeta, tDrDu);

        // Assemble full Jacobian
        auto tMesh = mGlobalEquation->getMesh();
        auto tGlobalJacobian = Plato::CreateBlockMatrix<Plato::CrsMatrixType, mNumGlobalDofsPerNode, mNumGlobalDofsPerNode>(&tMesh);
        Plato::BlockMatrixEntryOrdinal<mNumSpatialDims, mNumGlobalDofsPerNode> tGlobalJacEntryOrdinal(tGlobalJacobian, &tMesh);
        auto tJacEntries = tGlobalJacobian->entries();
        Plato::assemble_jacobian(tNumCells, mNumGlobalDofsPerCell, mNumGlobalDofsPerCell, tGlobalJacEntryOrdinal, tDrDu, tJacEntries);

        return tGlobalJacobian;
    }

    Plato::ScalarVector assembleResidual(const Plato::ScalarVector & aControls,
                                         const Plato::ForwardProblemStates & aStates,
                                         const Plato::ScalarArray3D& aInvLocalJacobianT)
    {
        auto tGlobalResidual =
            mGlobalEquation->value(aStates.mCurrentGlobalState, aStates.mPreviousGlobalState,
                                     aStates.mCurrentLocalState, aStates.mPreviousLocalState,
                                     aStates.mProjectedPressGrad, aControls, aStates.mCurrentStepIndex);

        // compute local residual workset (WS)
        auto tLocalResidualWS =
                mLocalEquation->valueWorkSet(aStates.mCurrentGlobalState, aStates.mPreviousGlobalState,
                                               aStates.mCurrentLocalState, aStates.mPreviousLocalState,
                                               aControls, aStates.mCurrentStepIndex);

        // compute inv(DhDc)*h, where h is the local residual and DhDc is the local jacobian
        auto tNumCells = mLocalEquation->numCells();
        const Plato::Scalar tAlpha = 1.0; const Plato::Scalar tBeta = 0.0;
        Plato::ScalarMultiVector tInvLocalJacTimesLocalRes("InvLocalJacTimesLocalRes", tNumCells, mNumLocalDofsPerCell);
        Plato::matrix_times_vector_workset("N", tAlpha, aInvLocalJacobianT, tLocalResidualWS, tBeta, tInvLocalJacTimesLocalRes);

        // compute DrDc*inv(DhDc)*h
        Plato::ScalarMultiVector tLocalResidualTerm("LocalResidualTerm", tNumCells, mNumGlobalDofsPerCell);
        auto tDrDc = mGlobalEquation->gradient_c(aStates.mCurrentGlobalState, aStates.mPreviousGlobalState,
                                                   aStates.mCurrentLocalState, aStates.mPreviousLocalState,
                                                   aStates.mProjectedPressGrad, aControls, aStates.mCurrentStepIndex);
        Plato::matrix_times_vector_workset("N", tAlpha, tDrDc, tInvLocalJacTimesLocalRes, tBeta, tLocalResidualTerm);

        // assemble local residual contribution
        const auto tNumNodes = mGlobalEquation->numNodes();
        const auto tTotalNumDofs = mNumGlobalDofsPerNode * tNumNodes;
        Plato::ScalarVector  tLocalResidualContribution("Assembled Local Residual", tTotalNumDofs);
        mWorksetBase.assembleResidual(tLocalResidualTerm, tLocalResidualContribution);

        // add local residual contribution to global residual, i.e. r - DrDc*inv(DhDc)*h
        Plato::axpy(static_cast<Plato::Scalar>(-1.0), tLocalResidualContribution, tGlobalResidual);

        return (tGlobalResidual);
    }

    void initializeSolver(Plato::ForwardProblemStates & aStates)
    {
        mCurrentSolverIter = 0;
        Plato::update(1.0, aStates.mPreviousLocalState, 0.0, aStates.mCurrentLocalState);
        Plato::update(1.0, aStates.mPreviousGlobalState, 0.0, aStates.mCurrentGlobalState);
    }

    bool checkStoppingCriterion(Plato::NewtonRaphsonOutputData & aOutputData)
    {
        bool tStop = false;

        if(aOutputData.mRelativeNorm < mStoppingTolerance)
        {
            tStop = true;
            aOutputData.mStopingCriterion = Plato::NewtonRaphson::RELATIVE_NORM_TOLERANCE;
        }
        else if(aOutputData.mCurrentNorm < mCurrentResidualNormTolerance)
        {
            tStop = true;
            aOutputData.mStopingCriterion = Plato::NewtonRaphson::CURRENT_NORM_TOLERANCE;
        }
        else if(aOutputData.mCurrentIteration >= mMaxNumSolverIter)
        {
            tStop = true;
            aOutputData.mStopingCriterion = Plato::NewtonRaphson::MAX_NUMBER_ITERATIONS;
        }

        return (tStop);
    }

    /***************************************************************************//**
     * \brief Solve Newton-Raphson problem
     * \param [in] aControls           1-D view of controls, e.g. design variables
     * \param [in] aStateData         data manager with current and previous state data
     * \param [in] aInvLocalJacobianT 3-D container for inverse Jacobian
     * \return flag used to indicate if the Newton-Raphson solver converged
    *******************************************************************************/
    bool solve(const Plato::ScalarVector & aControls, Plato::ForwardProblemStates & aStates)
    {
        bool tNewtonRaphsonConverged = false;
        Plato::NewtonRaphsonOutputData tOutputData;
        auto tNumCells = mLocalEquation->numCells();
        Plato::ScalarArray3D tInvLocalJacobianT("Inverse Transpose DhDc", tNumCells, mNumLocalDofsPerCell, mNumLocalDofsPerCell);

        tOutputData.mWriteOutput = mWriteSolverDiagnostics;
        Plato::print_newton_raphson_diagnostics_header(tOutputData, mSolverDiagnosticsFile);

        this->initializeSolver(aStates);
        while(true)
        {
            tOutputData.mCurrentIteration = mCurrentSolverIter;

            // update inverse of local Jacobian -> store in tInvLocalJacobianT
            this->updateInverseLocalJacobian(aControls, aStates, tInvLocalJacobianT);

            // assemble residual
            auto tGlobalResidual = this->assembleResidual(aControls, aStates, tInvLocalJacobianT);
            Plato::scale(static_cast<Plato::Scalar>(-1.0), tGlobalResidual);

            // assemble tangent stiffness matrix
            auto tGlobalJacobian = this->assembleTangentMatrix(aControls, aStates, tInvLocalJacobianT);

            // apply Dirichlet boundary conditions
            this->applyConstraints(tGlobalJacobian, tGlobalResidual);

            // check convergence
            Plato::compute_relative_residual_norm_criterion(tGlobalResidual, tOutputData);
            Plato::print_newton_raphson_diagnostics(tOutputData, mSolverDiagnosticsFile);

            const bool tStop = this->checkStoppingCriterion(tOutputData);
            if(tStop == true)
            {
                tNewtonRaphsonConverged = true;
                break;
            }

            // update global states
            this->updateGlobalStates(tGlobalJacobian, tGlobalResidual, aStates);

            // update local states
            mLocalEquation->updateLocalState(aStates.mCurrentGlobalState, aStates.mPreviousGlobalState,
                                               aStates.mCurrentLocalState, aStates.mPreviousLocalState,
                                               aControls, aStates.mCurrentStepIndex);
            mCurrentSolverIter++;
        }

        Plato::print_newton_raphson_stop_criterion(tOutputData, mSolverDiagnosticsFile);

        return (tNewtonRaphsonConverged);
    }
};



















/***************************************************************************//**
 * \brief Plasticity problem manager.  This interface is responsible for the 
 * evaluation of the criteriai, sensitivities and residual evaluations.
 *
 * \tparam PhysicsT physics type, e.g. Plato::InfinitesimalStrainPlasticity
*******************************************************************************/
template<typename PhysicsT>
class PlasticityProblem : public Plato::AbstractProblem
{
// private member data
private:
    static constexpr auto mNumSpatialDims = PhysicsT::mNumSpatialDims;                /*!< spatial dimensions*/
    static constexpr auto mNumNodesPerCell = PhysicsT::mNumNodesPerCell;              /*!< number of nodes per cell*/
    static constexpr auto mPressureDofOffset = PhysicsT::mPressureDofOffset;          /*!< number of pressure dofs offset*/
    static constexpr auto mNumGlobalDofsPerNode = PhysicsT::mNumDofsPerNode;          /*!< number of global degrees of freedom per node*/
    static constexpr auto mNumGlobalDofsPerCell = PhysicsT::mNumDofsPerCell;          /*!< number of global degrees of freedom per cell (i.e. element)*/
    static constexpr auto mNumLocalDofsPerCell = PhysicsT::mNumLocalDofsPerCell;      /*!< number of local degrees of freedom per cell (i.e. element)*/
    static constexpr auto mNumPressGradDofsPerCell = PhysicsT::mNumNodeStatePerCell;  /*!< number of projected pressure gradient degrees of freedom per cell (i.e. element)*/
    static constexpr auto mNumPressGradDofsPerNode = PhysicsT::mNumNodeStatePerNode;  /*!< number of projected pressure gradient degrees of freedom per node*/
    static constexpr auto mNumConfigDofsPerCell = mNumSpatialDims * mNumNodesPerCell; /*!< number of configuration (i.e. coordinates) degrees of freedom per cell (i.e. element) */

    // Required
    using ProjectorT = typename Plato::Projection<mNumSpatialDims, PhysicsT::mNumDofsPerNode, PhysicsT::mPressureDofOffset>;
    std::shared_ptr<Plato::VectorFunctionVMS<ProjectorT>> mProjectionEq;         /*!< global pressure gradient projection interface*/
    std::shared_ptr<Plato::GlobalVectorFunctionInc<PhysicsT>> mGlobalResidualEq; /*!< global equality constraint interface*/
    std::shared_ptr<Plato::LocalVectorFunctionInc<Plato::Plasticity<mNumSpatialDims>>> mLocalResidualEq; /*!< local equality constraint interface*/

    // Optional
    std::shared_ptr<Plato::LocalScalarFunctionInc> mObjective;  /*!< objective constraint interface*/
    std::shared_ptr<Plato::LocalScalarFunctionInc> mConstraint; /*!< constraint constraint interface*/

    Plato::OrdinalType mNumPseudoTimeSteps;       /*!< current number of pseudo time steps*/
    Plato::OrdinalType mMaxNumPseudoTimeSteps;    /*!< maximum number of pseudo time steps*/

    Plato::Scalar mPseudoTimeStep;                /*!< pseudo time step */
    Plato::Scalar mInitialNormResidual;           /*!< initial norm of global residual*/
    Plato::Scalar mDispControlConstant;           /*!< current pseudo time step */
    Plato::Scalar mCurrentNormStopTolerance;      /*!< current residual norm stopping tolerance - avoids unnecessary solves */
    Plato::Scalar mNumPseudoTimeStepMultiplier;   /*!< number of pseudo time step multiplier */

    Plato::ScalarVector mGlobalResidual;          /*!< global residual */
    Plato::ScalarVector mPressure;        /*!< projected pressure */

    Plato::ScalarMultiVector mLocalStates;        /*!< local state variables*/
    Plato::ScalarMultiVector mGlobalStates;       /*!< global state variables*/
    Plato::ScalarMultiVector mProjectedPressGrad; /*!< projected pressure gradient (# Time Steps, # Projected Pressure Gradient dofs)*/

    Teuchos::RCP<Plato::CrsMatrixType> mGlobalJacobian; /*!< global Jacobian matrix */

    Plato::ScalarVector mDirichletValues;         /*!< values associated with the Dirichlet boundary conditions*/
    Plato::LocalOrdinalVector mDirichletDofs;     /*!< list of degrees of freedom associated with the Dirichlet boundary conditions*/

    Plato::WorksetBase<Plato::SimplexPlasticity<mNumSpatialDims>> mWorksetBase; /*!< assembly routine interface */

    std::shared_ptr<Plato::NewtonRaphsonSolver<PhysicsT>> mNewtonSolver; /*!< Newton-Raphson solve interface */
    std::shared_ptr<Plato::BlockMatrixEntryOrdinal<mNumSpatialDims, mNumGlobalDofsPerNode>> mGlobalJacEntryOrdinal; /*!< global Jacobian matrix entry ordinal */

// public functions
public:
    /***************************************************************************//**
     * \brief PLATO Plasticity Problem constructor
     * \param [in] aMesh mesh database
     * \param [in] aMeshSets side sets database
     * \param [in] aInputs input parameters database
    *******************************************************************************/
    PlasticityProblem(Omega_h::Mesh& aMesh, Omega_h::MeshSets& aMeshSets, Teuchos::ParameterList& aInputs) :
            mLocalResidualEq(std::make_shared<Plato::LocalVectorFunctionInc<Plato::Plasticity<mNumSpatialDims>>>(aMesh, aMeshSets, mDataMap, aInputs)),
            mGlobalResidualEq(std::make_shared<Plato::GlobalVectorFunctionInc<PhysicsT>>(aMesh, aMeshSets, mDataMap, aInputs, aInputs.get<std::string>("PDE Constraint"))),
            mProjectionEq(std::make_shared<Plato::VectorFunctionVMS<ProjectorT>>(aMesh, aMeshSets, mDataMap, aInputs, std::string("State Gradient Projection"))),
            mObjective(nullptr),
            mConstraint(nullptr),
            mNumPseudoTimeSteps(Plato::ParseTools::getSubParam<Plato::OrdinalType>(aInputs, "Time Stepping", "Initial Num. Pseudo Time Steps", 20)),
            mMaxNumPseudoTimeSteps(Plato::ParseTools::getSubParam<Plato::OrdinalType>(aInputs, "Time Stepping", "Maximum Num. Pseudo Time Steps", 80)),
            mPseudoTimeStep(1.0/(static_cast<Plato::Scalar>(mNumPseudoTimeSteps))),
            mInitialNormResidual(std::numeric_limits<Plato::Scalar>::max()),
            mDispControlConstant(std::numeric_limits<Plato::Scalar>::min()),
            mCurrentNormStopTolerance(Plato::ParseTools::getSubParam<Plato::Scalar>(aInputs, "Newton-Raphson", "Current Residual Norm Stopping Tolerance", 1e-10)),
            mNumPseudoTimeStepMultiplier(Plato::ParseTools::getSubParam<Plato::Scalar>(aInputs, "Time Stepping", "Expansion Multiplier", 2)),
            mGlobalResidual("Global Residual", mGlobalResidualEq->size()),
            mPressure("Previous Pressure Field", aMesh.nverts()),
            mLocalStates("Local States", mNumPseudoTimeSteps, mLocalResidualEq->size()),
            mGlobalStates("Global States", mNumPseudoTimeSteps, mGlobalResidualEq->size()),
            mProjectedPressGrad("Projected Pressure Gradient", mNumPseudoTimeSteps, mProjectionEq->size()),
            mWorksetBase(aMesh),
            mNewtonSolver(std::make_shared<Plato::NewtonRaphsonSolver<PhysicsT>>(aMesh, aInputs))
    {
        this->initialize(aMesh, aMeshSets, aInputs);
    }

    explicit PlasticityProblem(Omega_h::Mesh& aMesh) :
            mLocalResidualEq(nullptr),
            mGlobalResidualEq(nullptr),
            mProjectionEq(nullptr),
            mObjective(nullptr),
            mConstraint(nullptr),
            mNumPseudoTimeSteps(20),
            mMaxNumPseudoTimeSteps(80),
            mPseudoTimeStep(1.0/(static_cast<Plato::Scalar>(mNumPseudoTimeSteps))),
            mInitialNormResidual(std::numeric_limits<Plato::Scalar>::max()),
            mDispControlConstant(std::numeric_limits<Plato::Scalar>::min()),
            mCurrentNormStopTolerance(1e-10),
            mNumPseudoTimeStepMultiplier(2),
            mGlobalResidual("Global Residual", aMesh.nverts() * mNumGlobalDofsPerNode),
            mPressure("Pressure Field", aMesh.nverts()),
            mLocalStates("Local States", mNumPseudoTimeSteps, aMesh.nelems() * mNumLocalDofsPerCell),
            mGlobalStates("Global States", mNumPseudoTimeSteps, aMesh.nverts() * mNumGlobalDofsPerNode),
            mProjectedPressGrad("Projected Pressure Gradient", mNumPseudoTimeSteps, aMesh.nverts() * mNumPressGradDofsPerNode),
            mWorksetBase(aMesh),
            mNewtonSolver(std::make_shared<Plato::NewtonRaphsonSolver<PhysicsT>>(aMesh))
    {
        mGlobalJacobian = Plato::CreateBlockMatrix<Plato::CrsMatrixType, mNumGlobalDofsPerNode, mNumGlobalDofsPerNode>(&aMesh);
        mGlobalJacEntryOrdinal = std::make_shared<Plato::BlockMatrixEntryOrdinal<mNumSpatialDims, mNumGlobalDofsPerNode>>(mGlobalJacobian, &aMesh);
    }

    /***************************************************************************//**
     * \brief PLATO Plasticity Problem destructor
    *******************************************************************************/
    virtual ~PlasticityProblem()
    {
    }

    void appendObjective(const std::shared_ptr<Plato::LocalScalarFunctionInc>& aObjective) 
    {
        mObjective = aObjective;   
    }

    void appendConstraint(const std::shared_ptr<Plato::LocalScalarFunctionInc>& aConstraint) 
    {
        mConstraint = aConstraint;   
    }

    void appendGlobalResidual(const std::shared_ptr<Plato::GlobalVectorFunctionInc<PhysicsT>>& aGlobalResidual)
    {
        mGlobalResidualEq = aGlobalResidual;
    }

    /***************************************************************************//**
     * \brief Read essential (Dirichlet) boundary conditions from the Exodus file.
     * \param [in] aMesh mesh database
     * \param [in] aMeshSets side sets database
     * \param [in] aInputs input parameters database
    *******************************************************************************/
    void readEssentialBoundaryConditions(Omega_h::Mesh& aMesh, Omega_h::MeshSets& aMeshSets, Teuchos::ParameterList& aInputs)
    {
        if(aInputs.isSublist("Essential Boundary Conditions") == false)
        {
            THROWERR("ESSENTIAL BOUNDARY CONDITIONS SUBLIST IS NOT DEFINED IN THE INPUT FILE")
        }
        Plato::EssentialBCs<PhysicsT> tDirichletBCs(aInputs.sublist("Essential Boundary Conditions", false));
        tDirichletBCs.get(aMeshSets, mDirichletDofs, mDirichletValues);
    }

    /***************************************************************************//**
     * \brief Set Dirichlet boundary conditions
     * \param [in] aDirichletDofs   degrees of freedom associated with Dirichlet boundary conditions
     * \param [in] aDirichletValues values associated with Dirichlet degrees of freedom
    *******************************************************************************/
    void setEssentialBoundaryConditions(const Plato::LocalOrdinalVector & aDirichletDofs, const Plato::ScalarVector & aDirichletValues)
    {
        if(aDirichletDofs.size() != aDirichletValues.size())
        {
            std::ostringstream tError;
            tError << "DIMENSION MISMATCH: THE NUMBER OF ELEMENTS IN INPUT DOFS AND VALUES ARRAY DO NOT MATCH."
                << "DOFS SIZE = " << aDirichletDofs.size() << " AND VALUES SIZE = " << aDirichletValues.size();
            THROWERR(tError.str())
        }
        mDirichletDofs = aDirichletDofs;
        mDirichletValues = aDirichletValues;
    }

    /***************************************************************************//**
     * \brief Return number of global degrees of freedom in solution.
     * \return Number of global degrees of freedom
    *******************************************************************************/
    Plato::OrdinalType getNumSolutionDofs() override
    {
        return (mGlobalResidualEq->size());
    }

    /***************************************************************************//**
     * \brief Set global state variables
     * \param [in] aGlobalState 2D view of global state variables - (NumTimeSteps, TotalDofs)
    *******************************************************************************/
    void setGlobalState(const Plato::ScalarMultiVector & aGlobalState) override
    {
        assert(aGlobalState.extent(0) == mGlobalStates.extent(0));
        assert(aGlobalState.extent(1) == mGlobalStates.extent(1));
        Kokkos::deep_copy(mGlobalStates, aGlobalState);
    }

    /***************************************************************************//**
     * \brief Return 2D view of global state variables - (NumTimeSteps, TotalDofs)
     * \return aGlobalState 2D view of global state variables
    *******************************************************************************/
    Plato::ScalarMultiVector getGlobalState() override
    {
        return mGlobalStates;
    }

    /***************************************************************************//**
     * \brief Return 2D view of global adjoint variables - (2, TotalDofs)
     * \return 2D view of global adjoint variables
    *******************************************************************************/
    Plato::ScalarMultiVector getAdjoint() override
    {
        THROWERR("ADJOINT MEMBER DATA IS NOT DEFINED");
    }

    /***************************************************************************//**
     * \brief Apply Dirichlet constraints
     * \param [in] aMatrix Compressed Row Storage (CRS) matrix
     * \param [in] aVector 1D view of Right-Hand-Side forces
    *******************************************************************************/
    void applyConstraints(const Teuchos::RCP<Plato::CrsMatrixType> & aMatrix, const Plato::ScalarVector & aVector) {return;}

    /***************************************************************************//**
     * \brief Fill right-hand-side vector values
    *******************************************************************************/
    void applyBoundaryLoads(const Plato::ScalarVector & aForce) override { return; }

    /***************************************************************************//**
     * \brief Update physics-based parameters within optimization iterations
     * \param [in] aControls 1D container of control variables
     * \param [in] aGlobalState 2D container of global state variables
    *******************************************************************************/
    void updateProblem(const Plato::ScalarVector & aControls,
                       const Plato::ScalarMultiVector & aGlobalState) override
    {
        mObjective->updateProblem(aGlobalState, mLocalStates, aControls);
        mConstraint->updateProblem(aGlobalState, mLocalStates, aControls);
    }

    /***************************************************************************//**
     * \brief Solve system of equations
     * \param [in] aControls 1D view of control variables
     * \return 2D view of state variables
    *******************************************************************************/
    Plato::ScalarMultiVector solution(const Plato::ScalarVector &aControls) override
    {
        // TODO: NOTES
        // 1. WRITE LOCAL STATES, PRESSURE, AND GLOBAL STATES HISTORY TO FILE - MEMORY CONCERNS
        //   1.1. NO NEED TO STORE MEMBER DATA FOR THESE QUANTITIES
        //   1.2. READ DATA FROM FILES DURING ADJOINT SOLVE
        // 4. HOW WILL OUTPUT DATA BE PRESENTED TO THE USERS, WE CANNOT SEND TIME-DEPENDENT DATA THROUGH THE ENGINE.
        if(aControls.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("INPUT CONTROL VECTOR IS EMPTY.")
        }

        bool tGlobalStateComputed = false;
        while (tGlobalStateComputed == false)
        {
            tGlobalStateComputed = this->solveForwardProblem(aControls);
            if (tGlobalStateComputed == true)
            {
                std::stringstream tMsg;
                tMsg << "\n**** Forward Solve Was Successful ****\n";
                mNewtonSolver->appendOutputMessage(tMsg);
                break;
            }
            else
            {
                break;
            }

            /*mNumPseudoTimeSteps = mNumPseudoTimeStepMultiplier * static_cast<Plato::Scalar>(mNumPseudoTimeSteps);

            if(mNumPseudoTimeSteps > mMaxNumPseudoTimeSteps)
            {
                mNewtonRaphsonDiagnosticsFile << "\n**** Unsuccessful Forward Solve.  Number of pseudo time steps is "
                    << "greater than the maximum number of pseudo time steps.  The number of current pseudo time "
                    << "steps is set to " << mNumPseudoTimeSteps << " and the maximum number of pseudo time steps "
                    << "is set to " << mMaxNumPseudoTimeSteps << ". ****\n";
                break;
            }

            this->resizeStateContainers();*/
        }

        return mGlobalStates;
    }

    /***************************************************************************//**
     * \fn Plato::Scalar objectiveValue(const Plato::ScalarVector & aControls,
     *                                  const Plato::ScalarMultiVector & aGlobalState)
     * \brief Evaluate objective function and return its value
     * \param [in] aControls 1D view of control variables
     * \param [in] aGlobalState 2D view of state variables
     * \return objective function value
    *******************************************************************************/
    Plato::Scalar objectiveValue(const Plato::ScalarVector & aControls,
                                 const Plato::ScalarMultiVector & aGlobalState) override
    {
        if(aControls.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nCONTROL 1D VIEW IS EMPTY.\n");
        }
        if(aGlobalState.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nGLOBAL STATE 2D VIEW IS EMPTY.\n");
        }
        if(mObjective == nullptr)
        {
            THROWERR("\nOBJECTIVE PTR IS NULL.\n");
        }

        auto tOutput = this->evaluateCriterion(*mObjective, aGlobalState, mLocalStates, aControls);

        return (tOutput);
    }

    /***************************************************************************//**
     * \brief Evaluate objective function and return its value
     * \param [in] aControls 1D view of control variables
     * \return objective function value
    *******************************************************************************/
    Plato::Scalar objectiveValue(const Plato::ScalarVector & aControls) override
    {
        if(aControls.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nCONTROL 1D VIEW IS EMPTY.\n");
        }
        if(mObjective == nullptr)
        {
            THROWERR("\nOBJECTIVE PTR IS NULL.\n");
        }

        auto tOutput = this->evaluateCriterion(*mObjective, mGlobalStates, mLocalStates, aControls);

        return (tOutput);
    }

    /***************************************************************************//**
     * \brief Evaluate constraint function and return its value
     * \param [in] aControls 1D view of control variables
     * \param [in] aGlobalState 2D view of state variables
     * \return constraint function value
    *******************************************************************************/
    Plato::Scalar constraintValue(const Plato::ScalarVector & aControls,
                                  const Plato::ScalarMultiVector & aGlobalState) override
    {
        if(aControls.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nCONTROL 1D VIEW IS EMPTY.\n");
        }
        if(aGlobalState.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nGLOBAL STATE 2D VIEW IS EMPTY.\n");
        }
        if(mConstraint == nullptr)
        {
            THROWERR("\nCONSTRAINT PTR IS NULL.\n");
        }

        auto tOutput = this->evaluateCriterion(*mConstraint, mGlobalStates, mLocalStates, aControls);

        return (tOutput);
    }

    /***************************************************************************//**
     * \brief Evaluate constraint function and return its value
     * \param [in] aControls 1D view of control variables
     * \return constraint function value
    *******************************************************************************/
    Plato::Scalar constraintValue(const Plato::ScalarVector & aControls) override
    {
        if(aControls.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nCONTROL 1D VIEW IS EMPTY.\n");
        }
        if(mConstraint == nullptr)
        {
            THROWERR("\nCONSTRAINT PTR IS NULL.\n");
        }

        auto tOutput = this->evaluateCriterion(*mConstraint, mGlobalStates, mLocalStates, aControls);

        return tOutput;
    }

    /***************************************************************************//**
     * \brief Evaluate objective partial derivative wrt control variables
     * \param [in] aControls 1D view of control variables
     * \return 1D view - objective partial derivative wrt control variables
    *******************************************************************************/
    Plato::ScalarVector objectiveGradient(const Plato::ScalarVector & aControls) override
    {
        if(aControls.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nCONTROL 1D VIEW IS EMPTY.\n");
        }
        if(mObjective == nullptr)
        {
            THROWERR("\nOBJECTIVE PTR IS NULL.\n");
        }

        auto tTotalDerivative = this->objectiveGradient(aControls, mGlobalStates);

        return tTotalDerivative;
    }

    /***************************************************************************//**
     * \brief Evaluate objective gradient wrt control variables
     * \param [in] aControls 1D view of control variables
     * \param [in] aGlobalState 2D view of global state variables
     * \return 1D view of the objective gradient wrt control variables
    *******************************************************************************/
    Plato::ScalarVector objectiveGradient(const Plato::ScalarVector & aControls,
                                          const Plato::ScalarMultiVector & aGlobalState) override
    {
        if(aControls.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nCONTROL 1D VIEW IS EMPTY.\n");
        }
        if(aGlobalState.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nGLOBAL STATE 2D VIEW IS EMPTY.\n");
        }
        if(mObjective == nullptr)
        {
            THROWERR("\nOBJECTIVE PTR IS NULL.\n");
        }

        auto tNumNodes = mGlobalResidualEq->numNodes();
        Plato::ScalarVector tTotalDerivative("Total Derivative", tNumNodes);
        // PDE constraint contribution to the total gradient with respect to control dofs
        this->backwardTimeIntegration(Plato::PartialDerivative::CONTROL, *mObjective, aControls, tTotalDerivative);
        // Design criterion contribution to the total gradient with respect to control dofs
        this->addCriterionPartialDerivativeZ(*mObjective, aControls, tTotalDerivative);

        return (tTotalDerivative);
    }

    /***************************************************************************//**
     * \brief Evaluate objective partial derivative wrt configuration variables
     * \param [in] aControls 1D view of control variables
     * \return 1D view - objective partial derivative wrt configuration variables
    *******************************************************************************/
    Plato::ScalarVector objectiveGradientX(const Plato::ScalarVector & aControls) override
    {
        if(aControls.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nCONTROL 1D VIEW IS EMPTY.\n");
        }
        if(mObjective == nullptr)
        {
            THROWERR("\nOBJECTIVE PTR IS NULL.\n");
        }

        auto tTotalDerivative = this->objectiveGradientX(aControls, mGlobalStates);

        return tTotalDerivative;
    }

    /***************************************************************************//**
     * \brief Evaluate objective gradient wrt configuration variables
     * \param [in] aControls 1D view of control variables
     * \param [in] aGlobalState 2D view of global state variables
     * \return 1D view of the objective gradient wrt control variables
    *******************************************************************************/
    Plato::ScalarVector objectiveGradientX(const Plato::ScalarVector & aControls,
                                           const Plato::ScalarMultiVector & aGlobalState) override
    {
        if(aControls.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nCONTROL 1D VIEW IS EMPTY.\n");
        }
        if(aGlobalState.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nGLOBAL STATE 2D VIEW IS EMPTY.\n");
        }
        if(mObjective == nullptr)
        {
            THROWERR("\nOBJECTIVE PTR IS NULL.\n");
        }

        Plato::ScalarVector tTotalDerivative("Total Derivative", mNumConfigDofsPerCell);
        // PDE constraint contribution to the total gradient with respect to configuration dofs
        this->backwardTimeIntegration(Plato::PartialDerivative::CONFIGURATION, *mObjective, aControls, tTotalDerivative);
        // Design criterion contribution to the total gradient with respect to configuration dofs
        this->addCriterionPartialDerivativeX(*mObjective, aControls, tTotalDerivative);

        return (tTotalDerivative);
    }

    /***************************************************************************//**
     * \brief Evaluate constraint partial derivative wrt control variables
     * \param [in] aControls 1D view of control variables
     * \return 1D view - constraint partial derivative wrt control variables
    *******************************************************************************/
    Plato::ScalarVector constraintGradient(const Plato::ScalarVector & aControls) override
    {
        if(aControls.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nCONTROL 1D VIEW IS EMPTY.\n");
        }
        if(mConstraint == nullptr)
        {
            THROWERR("\nCONSTRAINT PTR IS NULL.\n");
        }

        auto tTotalDerivative = this->constraintGradient(aControls, mGlobalStates);

        return tTotalDerivative;
    }

    /***************************************************************************//**
     * \brief Evaluate constraint partial derivative wrt control variables
     * \param [in] aControls 1D view of control variables
     * \param [in] aGlobalState 2D view of state variables
     * \return 1D view - constraint partial derivative wrt control variables
    *******************************************************************************/
    Plato::ScalarVector constraintGradient(const Plato::ScalarVector & aControls,
                                           const Plato::ScalarMultiVector & aGlobalState) override
    {
        if(aControls.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nCONTROL 1D VIEW IS EMPTY.\n");
        }
        if(aGlobalState.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nGLOBAL STATE 2D VIEW IS EMPTY.\n");
        }
        if(mConstraint == nullptr)
        {
            THROWERR("\nCONSTRAINT PTR IS NULL.\n");
        }

        auto tNumNodes = mGlobalResidualEq->numNodes();
        Plato::ScalarVector tTotalDerivative("Total Derivative", tNumNodes);
        // PDE constraint contribution to the total gradient with respect to control dofs
        this->backwardTimeIntegration(Plato::PartialDerivative::CONTROL, *mConstraint, aControls, tTotalDerivative);
        // Design criterion contribution to the total gradient with respect to control dofs
        this->addCriterionPartialDerivativeZ(*mConstraint, aControls, tTotalDerivative);

        return (tTotalDerivative);
    }

    /***************************************************************************//**
     * \brief Evaluate constraint partial derivative wrt configuration variables
     * \param [in] aControls 1D view of control variables
     * \return 1D view - constraint partial derivative wrt configuration variables
    *******************************************************************************/
    Plato::ScalarVector constraintGradientX(const Plato::ScalarVector & aControls) override
    {
        if(aControls.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nCONTROL 1D VIEW IS EMPTY.\n");
        }
        if(mConstraint == nullptr)
        {
            THROWERR("\nCONSTRAINT PTR IS NULL.\n");
        }

        auto tTotalDerivative = this->constraintGradientX(aControls, mGlobalStates);

        return tTotalDerivative;
    }

    /***************************************************************************//**
     * \brief Evaluate constraint partial derivative wrt configuration variables
     * \param [in] aControls 1D view of control variables
     * \param [in] aGlobalState 2D view of state variables
     * \return 1D view - constraint partial derivative wrt configuration variables
    *******************************************************************************/
    Plato::ScalarVector constraintGradientX(const Plato::ScalarVector & aControls,
                                            const Plato::ScalarMultiVector & aGlobalState) override
    {
        if(aControls.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nCONTROL 1D VIEW IS EMPTY.\n");
        }
        if(aGlobalState.size() <= static_cast<Plato::OrdinalType>(0))
        {
            THROWERR("\nGLOBAL STATE 2D VIEW IS EMPTY.\n");
        }
        if(mConstraint == nullptr)
        {
            THROWERR("\nCONSTRAINT PTR IS NULL.\n");
        }

        Plato::ScalarVector tTotalDerivative("Total Derivative", mNumConfigDofsPerCell);
        // PDE constraint contribution to the total gradient with respect to configuration dofs
        this->backwardTimeIntegration(Plato::PartialDerivative::CONFIGURATION, *mConstraint, aControls, tTotalDerivative);
        // Design criterion contribution to the total gradient with respect to configuration dofs
        this->addCriterionPartialDerivativeX(*mConstraint, aControls, tTotalDerivative);
        return (tTotalDerivative);
    }

// private functions
private:
    /***************************************************************************//**
     * \brief Initialize member data
     * \param [in] aControls current set of controls, i.e. design variables
    *******************************************************************************/
    void initialize(Omega_h::Mesh& aMesh, Omega_h::MeshSets& aMeshSets, Teuchos::ParameterList& aInputParams)
    {
        this->allocateObjectiveFunction(aMesh, aMeshSets, aInputParams);
        this->allocateConstraintFunction(aMesh, aMeshSets, aInputParams);
        mGlobalJacobian = Plato::CreateBlockMatrix<Plato::CrsMatrixType, mNumGlobalDofsPerNode, mNumGlobalDofsPerNode>(&aMesh);
        mGlobalJacEntryOrdinal = std::make_shared<Plato::BlockMatrixEntryOrdinal<mNumSpatialDims, mNumGlobalDofsPerNode>>(mGlobalJacobian, &aMesh);
    }

    /***************************************************************************//**
     * \brief Resize global state, local state and projected pressure gradient containers
     * \param [in] aControls current set of controls, i.e. design variables
    *******************************************************************************/
    void resizeStateContainers()
    {
        mPseudoTimeStep = 1.0/(static_cast<Plato::Scalar>(mNumPseudoTimeSteps));
        mLocalStates = Plato::ScalarMultiVector("Local States", mNumPseudoTimeSteps, mLocalResidualEq->size());
        mGlobalStates = Plato::ScalarMultiVector("Global States", mNumPseudoTimeSteps, mGlobalResidualEq->size());
        mProjectedPressGrad = Plato::ScalarMultiVector("Projected Pressure Gradient", mNumPseudoTimeSteps, mProjectionEq->size());
    }

    void initializeNewtonSolver()
    {
        mNewtonSolver->setDirichletValuesMultiplier(mPseudoTimeStep);

        mNewtonSolver->appendDirichletDofs(mDirichletDofs);
        mNewtonSolver->appendDirichletValues(mDirichletValues);

        mNewtonSolver->appendLocalEquation(mLocalResidualEq);
        mNewtonSolver->appendGlobalEquation(mGlobalResidualEq);

        mDataMap.mScalarValues["LoadControlConstant"] = mPseudoTimeStep;
    }

    /***************************************************************************//**
     * \brief Solve forward problem
     * \param [in] aControls 1-D view of controls, e.g. design variables
     * \return flag used to indicate forward problem was solved to completion
    *******************************************************************************/
    bool solveForwardProblem(const Plato::ScalarVector & aControls)
    {
        Plato::ForwardProblemStates tStateData;
        auto tNumCells = mLocalResidualEq->numCells();
        tStateData.mDeltaGlobalState = Plato::ScalarVector("Global State Increment", mGlobalResidualEq->size());

        this->initializeNewtonSolver();

        bool tToleranceSatisfied = false;
        for(Plato::OrdinalType tCurrentStepIndex = 0; tCurrentStepIndex < mNumPseudoTimeSteps; tCurrentStepIndex++)
        {
            std::stringstream tMsg;
            tMsg << "TIME STEP #" << tCurrentStepIndex + static_cast<Plato::OrdinalType>(1) << ", TOTAL TIME = "
                << mPseudoTimeStep * static_cast<Plato::Scalar>(tCurrentStepIndex + 1) << "\n";
            mNewtonSolver->appendOutputMessage(tMsg);

            tStateData.mCurrentStepIndex = tCurrentStepIndex;
            this->cacheStateData(tStateData);

            // update local and global states
            bool tNewtonRaphsonConverged = mNewtonSolver->solve(aControls, tStateData);

            if(tNewtonRaphsonConverged == false)
            {
                std::stringstream tMsg;
                tMsg << "**** Newton-Raphson Solver did not converge at time step #"
                    << tCurrentStepIndex << ".  Number of pseudo time steps will be increased to "
                    << static_cast<Plato::OrdinalType>(mNumPseudoTimeSteps * mNumPseudoTimeStepMultiplier) << ". ****\n\n";
                mNewtonSolver->appendOutputMessage(tMsg);
                return tToleranceSatisfied;
            }

            // update projected pressure gradient state
            this->updateProjectedPressureGradient(aControls, tStateData);
        }

        tToleranceSatisfied = true;
        return tToleranceSatisfied;
    }

    /***************************************************************************//**
     * \brief Assemble path dependent tangent stiffness matrix, which is defined as
     * /f$ K_{T} = \frac{\partial{R}}{\partial{u}} - \frac{\partial{R}}{\partial{c}} *
     * \left[ \left( \frac{\partial{H}}{\partial{c}} \right)^{-1} * \frac{\partial{H}}
     * {\partial{u}} \right] /f$.  Here, R is the global residual, H is the local
     * residual, u are the global states, and c are the local states.
     *
     * \param [in] aControls 1D view of control variables, i.e. design variables
     * \param [in] aStateData state data manager
     * \param [in] aInvLocalJacobianT inverse of transpose local Jacobian wrt local states
    *******************************************************************************/
    template<class StateDataType>
    void assembleTangentMatrix(const Plato::ScalarVector & aControls,
                                            const StateDataType & aStateData,
                                            const Plato::ScalarArray3D& aInvLocalJacobianT)
    {
        // Compute cell Schur Complement, i.e. dR/dc * (dH/dc)^{-1} * dH/du, where H is the local
        // residual, R is the global residual, c are the local states and u are the global states
        auto tSchurComplement = this->computeSchurComplement(aControls, aStateData, aInvLocalJacobianT);

        // Compute cell Jacobian of the global residual with respect to the current global state WorkSet (WS)
        auto tDrDu = mGlobalResidualEq->gradient_u(aStateData.mCurrentGlobalState, aStateData.mPreviousGlobalState,
                                                   aStateData.mCurrentLocalState, aStateData.mPreviousLocalState,
                                                   aStateData.mProjectedPressGrad, aControls, aStateData.mCurrentStepIndex);

        // Add cell Schur complement to dR/du, where R is the global residual and u are the global states
        const Plato::Scalar tBeta = 1.0;
        const Plato::Scalar tAlpha = -1.0;
        auto tNumCells = mGlobalResidualEq->numCells();
        Plato::update_array_3D(tNumCells, tAlpha, tSchurComplement, tBeta, tDrDu);

        // Assemble full Jacobian
        auto tJacobianEntries = mGlobalJacobian->entries();
        Plato::fill(0.0, tJacobianEntries);
        Plato::assemble_jacobian(tNumCells, mNumGlobalDofsPerCell, mNumGlobalDofsPerCell,
                                 *mGlobalJacEntryOrdinal, tDrDu, tJacobianEntries);
    }

    /***************************************************************************//**
     * \brief Compute Schur complement, which is defined as /f$ A = \frac{\partial{R}}
     * {\partial{c}} * \left[ \left(\frac{\partial{H}}{\partial{c}}\right)^{-1} *
     * \frac{\partial{H}}{\partial{u}} \right] /f$, where R is the global residual, H
     * is the local residual, u are the global states, and c are the local states.
     *
     * \param [in] aControls 1D view of control variables, i.e. design variables
     * \param [in] aStateData state data manager
     * \param [in] aInvLocalJacobianT inverse of transpose local Jacobian wrt local states
     * \return 3D view with Schur complement per cell
    *******************************************************************************/
    template<class StateDataType>
    Plato::ScalarArray3D computeSchurComplement(const Plato::ScalarVector & aControls,
                                                const StateDataType & aStateData,
                                                const Plato::ScalarArray3D& aInvLocalJacobianT)
    {
        // Compute cell Jacobian of the local residual with respect to the current global state WorkSet (WS)
        auto tDhDu = mLocalResidualEq->gradient_u(aStateData.mCurrentGlobalState, aStateData.mPreviousGlobalState,
                                                 aStateData.mCurrentLocalState, aStateData.mPreviousLocalState,
                                                 aControls, aStateData.mCurrentStepIndex);

        // Compute cell C = (dH/dc)^{-1}*dH/du, where H is the local residual, c are the local states and u are the global states
        Plato::Scalar tBeta = 0.0;
        const Plato::Scalar tAlpha = 1.0;
        auto tNumCells = mLocalResidualEq->numCells();
        Plato::ScalarArray3D tInvDhDcTimesDhDu("InvDhDc times DhDu", tNumCells, mNumLocalDofsPerCell, mNumGlobalDofsPerCell);
        Plato::multiply_matrix_workset(tNumCells, tAlpha, aInvLocalJacobianT, tDhDu, tBeta, tInvDhDcTimesDhDu);

        // Compute cell Jacobian of the global residual with respect to the current local state WorkSet (WS)
        auto tDrDc = mGlobalResidualEq->gradient_c(aStateData.mCurrentGlobalState, aStateData.mPreviousGlobalState,
                                                  aStateData.mCurrentLocalState, aStateData.mPreviousLocalState,
                                                  aStateData.mProjectedPressGrad, aControls, aStateData.mCurrentStepIndex);

        // Compute cell Schur = dR/dc * (dH/dc)^{-1} * dH/du, where H is the local residual,
        // R is the global residual, c are the local states and u are the global states
        Plato::ScalarArray3D tSchurComplement("Schur Complement", tNumCells, mNumGlobalDofsPerCell, mNumGlobalDofsPerCell);
        Plato::multiply_matrix_workset(tNumCells, tAlpha, tDrDc, tInvDhDcTimesDhDu, tBeta, tSchurComplement);

        return tSchurComplement;
    }

    /***************************************************************************//**
     * \brief Update projected pressure gradient.
     * \param [in]     aControls  1-D view of controls, e.g. design variables
     * \param [in/out] aStateData data manager with current and previous global and local state data
    *******************************************************************************/
    void updateProjectedPressureGradient(const Plato::ScalarVector &aControls,
                                         Plato::ForwardProblemStates &aStateData)
    {
        Plato::OrdinalType tNextStepIndex = aStateData.mCurrentStepIndex + static_cast<Plato::OrdinalType>(1);
        if(tNextStepIndex >= mNumPseudoTimeSteps)
        {
            return;
        }

        // copy projection state, i.e. pressure
        Plato::extract<mNumGlobalDofsPerNode, mPressureDofOffset>(aStateData.mCurrentGlobalState, mPressure);

        // compute projected pressure gradient
        auto tNextProjectedPressureGradient = Kokkos::subview(mProjectedPressGrad, tNextStepIndex, Kokkos::ALL());
        Plato::fill(0.0, tNextProjectedPressureGradient);
        auto tProjResidual = mProjectionEq->value(tNextProjectedPressureGradient, mPressure, aControls, tNextStepIndex);
        auto tProjJacobian = mProjectionEq->gradient_u(tNextProjectedPressureGradient, mPressure, aControls, tNextStepIndex);
        Plato::Solve::RowSummed<PhysicsT::mNumSpatialDims>(tProjJacobian, aStateData.mProjectedPressGrad, tProjResidual);
    }

    /***************************************************************************//**
     * \brief Compute displacement norm,
     * \f$ \frac{\Vert \delta{u}_{i}^{T}\delta{u}_i \Vert}{\Vert \Delta{u}_0^{T}\Delta{u}_0 \Vert} \f$
     * \param [in]     aStateData  state data
     * \param [in\out] aOutputData Newton-Raphson solver output data
    *******************************************************************************/
    void computeDisplacementNorm(const Plato::ForwardProblemStates &aStateData, Plato::NewtonRaphsonOutputData & aOutputData)
    {
        if(aOutputData.mCurrentIteration == static_cast<Plato::OrdinalType>(0))
        {
            aOutputData.mReferenceNorm = Plato::norm(aStateData.mCurrentGlobalState);
            aOutputData.mCurrentNorm = aOutputData.mReferenceNorm;
        }
        else
        {
            aOutputData.mCurrentNorm = Plato::norm(aStateData.mDeltaGlobalState);
            aOutputData.mRelativeNorm = aOutputData.mCurrentNorm /
                (aOutputData.mReferenceNorm + std::numeric_limits<Plato::Scalar>::epsilon());
        }
    }

    /***************************************************************************//**
     * \brief Compute residual norm, \f$ \mid \Vert R_{i}^{T} - \Vert R_{i-1} \Vert \mid \f$
     * \param [in\out] aOutputData Newton-Raphson solver output data
    *******************************************************************************/
    void computeResidualNorm(Plato::NewtonRaphsonOutputData & aOutputData)
    {
        if(aOutputData.mCurrentIteration == static_cast<Plato::OrdinalType>(0))
        {
            aOutputData.mReferenceNorm = Plato::norm(mGlobalResidual);
            aOutputData.mCurrentNorm = aOutputData.mReferenceNorm;
        }
        else
        {
            aOutputData.mCurrentNorm = Plato::norm(mGlobalResidual);
            aOutputData.mRelativeNorm = std::abs(aOutputData.mCurrentNorm - aOutputData.mReferenceNorm);
            aOutputData.mReferenceNorm = aOutputData.mCurrentNorm;
        }
    }

    /***************************************************************************//**
     * \brief Compute residual norm, \f$ \frac{\Vert R_{i}^{T}}{\Vert R_{0} \Vert} \f$
     * \param [in\out] aOutputData Newton-Raphson solver output data
    *******************************************************************************/
    void computeRelativeResidualNorm(Plato::NewtonRaphsonOutputData & aOutputData)
    {
        if(aOutputData.mCurrentIteration == static_cast<Plato::OrdinalType>(0))
        {
            aOutputData.mReferenceNorm = Plato::norm(mGlobalResidual);
            aOutputData.mCurrentNorm = aOutputData.mReferenceNorm;
        }
        else
        {
            aOutputData.mCurrentNorm = Plato::norm(mGlobalResidual);
            aOutputData.mRelativeNorm = aOutputData.mCurrentNorm /
                    (aOutputData.mReferenceNorm + std::numeric_limits<Plato::Scalar>::epsilon());
        }
    }

    /***************************************************************************//**
     * \brief Get previous state
     * \param [in]     aCurrentStepIndex current time step index
     * \param [in]     aStates           states at each time step
     * \param [in/out] aOutput           previous state
    *******************************************************************************/
    void getPreviousState(const Plato::OrdinalType & aCurrentStepIndex,
                          const Plato::ScalarMultiVector & aStates,
                          Plato::ScalarVector & aOutput) const
    {
        auto tPreviousStepIndex = aCurrentStepIndex - static_cast<Plato::OrdinalType>(1);
        if(tPreviousStepIndex >= static_cast<Plato::OrdinalType>(0))
        {
            aOutput = Kokkos::subview(aStates, tPreviousStepIndex, Kokkos::ALL());
        }
        else
        {
            auto tLength = aStates.extent(1);
            aOutput = Plato::ScalarVector("Local State t=i-1", tLength);
            Plato::fill(0.0, aOutput);
        }
    }

    /***************************************************************************//**
     * \brief Evaluate criterion
     * \param [in] aCriterion   criterion scalar function interface
     * \param [in] aGlobalState global states for all time steps
     * \param [in] aLocalState  local states for all time steps
     * \param [in] aControls    current controls, e.g. design variables
     * \return new criterion value
    *******************************************************************************/
    Plato::Scalar evaluateCriterion(Plato::LocalScalarFunctionInc & aCriterion,
                                    const Plato::ScalarMultiVector & aGlobalState,
                                    const Plato::ScalarMultiVector & aLocalState,
                                    const Plato::ScalarVector & aControls)
    {
        Plato::ScalarVector tPreviousLocalState;
        Plato::ScalarVector tPreviousGlobalState;

        Plato::Scalar tOutput = 0;
        for(Plato::OrdinalType tCurrentStepIndex = 0; tCurrentStepIndex < mNumPseudoTimeSteps; tCurrentStepIndex++)
        {
            // SET CURRENT STATES
            auto tCurrentLocalState = Kokkos::subview(aLocalState, tCurrentStepIndex, Kokkos::ALL());
            auto tCurrentGlobalState = Kokkos::subview(aGlobalState, tCurrentStepIndex, Kokkos::ALL());

            // SET PREVIOUS AND FUTURE STATES
            this->getPreviousState(tCurrentStepIndex, aLocalState, tPreviousLocalState);
            this->getPreviousState(tCurrentStepIndex, aGlobalState, tPreviousGlobalState);

            tOutput += aCriterion.value(tCurrentGlobalState, tPreviousGlobalState,
                                        tCurrentLocalState, tPreviousLocalState, 
                                        aControls, tCurrentStepIndex);
        }

        return tOutput;
    }

    /***************************************************************************//**
     * \brief Add contribution from partial derivative of criterion with respect to
     * controls to total derivative of criterion with respect to controls.
     * \param [in]     aCriterion     design criterion interface
     * \param [in]     aControls      current controls, e.g. design variables
     * \param [in/out] aTotalGradient total derivative of criterion with respect to controls
    *******************************************************************************/
    void addCriterionPartialDerivativeZ(Plato::LocalScalarFunctionInc & aCriterion,
                                        const Plato::ScalarVector & aControls,
                                        Plato::ScalarVector & aTotalGradient)
    {
        Plato::ScalarVector tPreviousLocalState;
        Plato::ScalarVector tPreviousGlobalState;
        for(Plato::OrdinalType tCurrentStepIndex = 0; tCurrentStepIndex < mNumPseudoTimeSteps; tCurrentStepIndex++)
        {
            auto tCurrentLocalState = Kokkos::subview(mLocalStates, tCurrentStepIndex, Kokkos::ALL());
            auto tCurrentGlobalState = Kokkos::subview(mGlobalStates, tCurrentStepIndex, Kokkos::ALL());

            // SET PREVIOUS LOCAL STATES
            this->getPreviousState(tCurrentStepIndex, mLocalStates, tPreviousLocalState);
            this->getPreviousState(tCurrentStepIndex, mGlobalStates, tPreviousGlobalState);

            auto tDfDz = aCriterion.gradient_z(tCurrentGlobalState, tPreviousGlobalState,
                                               tCurrentLocalState, tPreviousLocalState, 
                                               aControls, tCurrentStepIndex);
            mWorksetBase.assembleScalarGradientZ(tDfDz, aTotalGradient);
        }
    }

    /***************************************************************************//**
     * \brief Add contribution from partial derivative of criterion with respect to
     * configuration to total derivative of criterion with respect to configuration.
     * \param [in]     aCriterion     design criterion interface
     * \param [in]     aControls      current controls, e.g. design variables
     * \param [in/out] aTotalGradient total derivative of criterion with respect to configuration
    *******************************************************************************/
    void addCriterionPartialDerivativeX(Plato::LocalScalarFunctionInc & aCriterion,
                                        const Plato::ScalarVector & aControls,
                                        Plato::ScalarVector & aTotalGradient)
    {
        Plato::ScalarVector tPreviousLocalState;
        Plato::ScalarVector tPreviousGlobalState;
        for(Plato::OrdinalType tCurrentStepIndex = 0; tCurrentStepIndex < mNumPseudoTimeSteps; tCurrentStepIndex++)
        {
            auto tCurrentLocalState = Kokkos::subview(mLocalStates, tCurrentStepIndex, Kokkos::ALL());
            auto tCurrentGlobalState = Kokkos::subview(mGlobalStates, tCurrentStepIndex, Kokkos::ALL());

            // SET PREVIOUS AND FUTURE LOCAL STATES
            this->getPreviousState(tCurrentStepIndex, mLocalStates, tPreviousLocalState);
            this->getPreviousState(tCurrentStepIndex, mGlobalStates, tPreviousGlobalState);

            auto tDfDX = aCriterion.gradient_x(tCurrentGlobalState, tPreviousGlobalState,
                                               tCurrentLocalState, tPreviousLocalState, 
                                               aControls, tCurrentStepIndex);
            mWorksetBase.assembleVectorGradientX(tDfDX, aTotalGradient);
        }
    }

    /***************************************************************************//**
     * \brief Add contribution from partial differential equation (PDE) constraint
     *   to the total derivative of the criterion, i.e. scalar function.
     * \param [in]     aCriterion criterion scalar function interface
     * \param [in]     aControls current controls, e.g. design variables
     * \param [in/out] aOutput   total derivative of criterion with respect to controls
    *******************************************************************************/
    void backwardTimeIntegration(const Plato::PartialDerivative::derivative_t & aType,
                                 Plato::LocalScalarFunctionInc & aCriterion,
                                 const Plato::ScalarVector & aControls,
                                 Plato::ScalarVector aTotalDerivative)
    {
        // Create state data manager
        auto tNumCells = mLocalResidualEq->numCells();
        Plato::StateData tCurrentStates(aType);
        Plato::StateData tPreviousStates(aType);
        Plato::AdjointProblemStates tAdjointStates(mGlobalResidualEq->size(), mLocalResidualEq->size(), mProjectionEq->size());
        Plato::ScalarArray3D tInvLocalJacobianT("Inverse Transpose DhDc", tNumCells, mNumLocalDofsPerCell, mNumLocalDofsPerCell);

        // outer loop for pseudo time steps
        auto tLastStepIndex = mNumPseudoTimeSteps - static_cast<Plato::OrdinalType>(1);
        for(tCurrentStates.mCurrentStepIndex = tLastStepIndex; tCurrentStates.mCurrentStepIndex >= 0; tCurrentStates.mCurrentStepIndex--)
        {
            tPreviousStates.mCurrentStepIndex = tCurrentStates.mCurrentStepIndex + 1;
            if(tPreviousStates.mCurrentStepIndex < mNumPseudoTimeSteps)
            {
                this->updateStateData(tPreviousStates);
            }

            this->updateStateData(tCurrentStates);
            this->updateAdjointData(tAdjointStates);
            this->updateInverseLocalJacobian(aControls, tCurrentStates, tInvLocalJacobianT);

            this->updateProjPressGradAdjointVars(aControls, tCurrentStates, tAdjointStates);
            this->updateGlobalAdjointVars(aCriterion, aControls, tCurrentStates, tPreviousStates, tInvLocalJacobianT, tAdjointStates);
            this->updateLocalAdjointVars(aCriterion, aControls, tCurrentStates, tPreviousStates, tInvLocalJacobianT, tAdjointStates);

            this->updatePartialDerivativePDE(aControls, tCurrentStates, tAdjointStates, aTotalDerivative);
        }
    }

    void updatePartialDerivativePDE(const Plato::ScalarVector &aControls,
                                    const Plato::StateData &aStateData,
                                    const Plato::AdjointProblemStates &aAdjointStates,
                                    Plato::ScalarVector &aOutput)
    {
        switch(aStateData.mPartialDerivativeType)
        {
            case Plato::PartialDerivative::CONTROL:
            {
                this->addPDEpartialDerivativeZ(aControls, aStateData, aAdjointStates, aOutput);
                break;
            }
            case Plato::PartialDerivative::CONFIGURATION:
            {
                this->addPDEpartialDerivativeX(aControls, aStateData, aAdjointStates, aOutput);
                break;
            }
            default:
            {
                PRINTERR("PARTIAL DERIVATIVE IS NOT DEFINED. OPTIONS ARE CONTROL AND CONFIGURATION")
            }
        }
    }

    /***************************************************************************//**
     * \brief Update state data for time step n, i.e. current time step:
     * \param [in] aStateData state data manager
     * \param [in] aZeroEntries flag - zero all entries in current states (default = false)
    *******************************************************************************/
    void cacheStateData(Plato::ForwardProblemStates &aStateData)
    {
        // GET CURRENT STATE
        aStateData.mCurrentLocalState = Kokkos::subview(mLocalStates, aStateData.mCurrentStepIndex, Kokkos::ALL());
        aStateData.mCurrentGlobalState = Kokkos::subview(mGlobalStates, aStateData.mCurrentStepIndex, Kokkos::ALL());
        aStateData.mProjectedPressGrad = Kokkos::subview(mProjectedPressGrad, aStateData.mCurrentStepIndex, Kokkos::ALL());

        // GET PREVIOUS STATE
        this->getPreviousState(aStateData.mCurrentStepIndex, mLocalStates, aStateData.mPreviousLocalState);
        this->getPreviousState(aStateData.mCurrentStepIndex, mGlobalStates, aStateData.mPreviousGlobalState);

        // SET ENTRIES IN CURRENT STATES TO ZERO
        Plato::fill(static_cast<Plato::Scalar>(0.0), aStateData.mCurrentLocalState);
        Plato::fill(static_cast<Plato::Scalar>(0.0), aStateData.mCurrentGlobalState);
        Plato::fill(static_cast<Plato::Scalar>(0.0), aStateData.mProjectedPressGrad);
        Plato::fill(static_cast<Plato::Scalar>(0.0), mPressure);
    }

    /***************************************************************************//**
     * \brief Update state data for time step n, i.e. current time step:
     * \param [in] aStateData state data manager
     * \param [in] aZeroEntries flag - zero all entries in current states (default = false)
    *******************************************************************************/
    void updateStateData(Plato::StateData &aStateData)
    {
        // GET CURRENT STATE
        aStateData.mCurrentLocalState = Kokkos::subview(mLocalStates, aStateData.mCurrentStepIndex, Kokkos::ALL());
        aStateData.mCurrentGlobalState = Kokkos::subview(mGlobalStates, aStateData.mCurrentStepIndex, Kokkos::ALL());
        aStateData.mProjectedPressGrad = Kokkos::subview(mProjectedPressGrad, aStateData.mCurrentStepIndex, Kokkos::ALL());
        Plato::extract<mNumGlobalDofsPerNode, mPressureDofOffset>(aStateData.mCurrentGlobalState, mPressure);

        // GET PREVIOUS STATE. 
        this->getPreviousState(aStateData.mCurrentStepIndex, mLocalStates, aStateData.mPreviousLocalState);
        this->getPreviousState(aStateData.mCurrentStepIndex, mGlobalStates, aStateData.mPreviousGlobalState);
    }

    /***************************************************************************//**
     * \brief Update adjoint data for time step n, i.e. current time step:
     * \param [in] aAdjointData adjoint data manager
    *******************************************************************************/
    void updateAdjointData(Plato::AdjointProblemStates& aAdjointStates)
    {
        // NOTE: CURRENT ADJOINT VARIABLES ARE UPDATED AT SOLVE TIME. THERE IS NO NEED TO SET THEM TO ZERO HERE.
        const Plato::Scalar tAlpha = 1.0; const Plato::Scalar tBeta = 0.0;
        Plato::update(tAlpha, aAdjointStates.mCurrentLocalAdjoint, tBeta, aAdjointStates.mPreviousLocalAdjoint);
        Plato::update(tAlpha, aAdjointStates.mCurrentGlobalAdjoint, tBeta, aAdjointStates.mPreviousGlobalAdjoint);
        Plato::update(tAlpha, aAdjointStates.mProjPressGradAdjoint, tBeta, aAdjointStates.mPreviousProjPressGradAdjoint);
    }

    /***************************************************************************//**
     * \brief Compute the contibution from the partial derivative of partial differential
     *   equation (PDE) with respect to the control degrees of freedom (dofs).  The PDE
     *   contribution to the total gradient with respect to the control dofs is given by:
     *
     *  /f$ \left(\frac{df}{dz}\right)_{t=n} = \left(\frac{\partial{f}}{\partial{z}}\right)_{t=n}
     *         + \left(\frac{\partial{R}}{\partial{z}}\right)_{t=n}^{T}\lambda_{t=n}
     *         + \left(\frac{\partial{H}}{\partial{z}}\right)_{t=n}^{T}\gamma_{t=n}
     *         + \left(\frac{\partial{P}}{\partial{z}}\right)_{t=n}^{T}\mu_{t=n} /f$,
     *
     * where R is the global residual, H is the local residual, P is the projection residual,
     * and /f$\lambda/f$ is the global adjoint vector, /f$\gamma/f$ is the local adjoint vector,
     * and /f$\mu/f$ is the projection adjoint vector. The pseudo time is denoted by t, where n
     * denotes the current step index.
     *
     * \param [in] aControls 1D view of control variables, i.e. design variables
     * \param [in] aStateData state data manager
     * \param [in] aAdjointData adjoint data manager
     * \param [in/out] aGradient total derivative wrt controls
    *********************************************************************************/
    void addPDEpartialDerivativeZ(const Plato::ScalarVector &aControls,
                                  const Plato::StateData &aStateData,
                                  const Plato::AdjointProblemStates &aAdjointStates,
                                  Plato::ScalarVector &aTotalGradient)
    {
        auto tNumCells = mGlobalResidualEq->numCells();
        Plato::ScalarMultiVector tGradientControl("Gradient WRT Control", tNumCells, mNumNodesPerCell);

        // add global adjoint contribution to total gradient, i.e. DfDz += (DrDz)^T * lambda
        Plato::ScalarMultiVector tCurrentLambda("Current Global State Adjoint", tNumCells, mNumGlobalDofsPerCell);
        mWorksetBase.worksetState(aAdjointStates.mCurrentGlobalAdjoint, tCurrentLambda);
        auto tDrDz = mGlobalResidualEq->gradient_z(aStateData.mCurrentGlobalState, aStateData.mPreviousGlobalState,
                                                  aStateData.mCurrentLocalState, aStateData.mPreviousLocalState,
                                                  aStateData.mProjectedPressGrad, aControls, aStateData.mCurrentStepIndex);
        const Plato::Scalar tAlpha = 1.0; Plato::Scalar tBeta = 0.0;
        Plato::matrix_times_vector_workset("T", tAlpha, tDrDz, tCurrentLambda, tBeta, tGradientControl);

        // add projected pressure gradient adjoint contribution to total gradient, i.e. DfDz += (DpDz)^T * gamma
        Plato::ScalarMultiVector tCurrentGamma("Current Projected Pressure Gradient Adjoint", tNumCells, mNumPressGradDofsPerCell);
        mWorksetBase.worksetNodeState(aAdjointStates.mProjPressGradAdjoint, tCurrentGamma);
        auto tDpDz = mProjectionEq->gradient_z_workset(aStateData.mProjectedPressGrad, mPressure,
                                                       aControls, aStateData.mCurrentStepIndex);
        tBeta = 1.0;
        Plato::matrix_times_vector_workset("T", tAlpha, tDpDz, tCurrentGamma, tBeta, tGradientControl);

        // compute local adjoint contribution to total gradient, i.e. (DhDz)^T * mu
        Plato::ScalarMultiVector tCurrentMu("Current Local State Adjoint", tNumCells, mNumLocalDofsPerCell);
        mWorksetBase.worksetLocalState(aAdjointStates.mCurrentLocalAdjoint, tCurrentMu);
        auto tDhDz = mLocalResidualEq->gradient_z(aStateData.mCurrentGlobalState, aStateData.mPreviousGlobalState,
                                                  aStateData.mCurrentLocalState, aStateData.mPreviousLocalState,
                                                  aControls, aStateData.mCurrentStepIndex);
        Plato::matrix_times_vector_workset("T", tAlpha, tDhDz, tCurrentMu, tBeta, tGradientControl);

        mWorksetBase.assembleScalarGradientZ(tGradientControl, aTotalGradient);
    }

    /***************************************************************************//**
     * \brief Compute the contibution from the partial derivative of partial differential
     *   equation (PDE) with respect to the configuration degrees of freedom (dofs).  The
     *   PDE contribution to the total gradient with respect to the configuration dofs is
     *   given by:
     *
     *  /f$ \left(\frac{df}{dz}\right)_{t=n} = \left(\frac{\partial{f}}{\partial{x}}\right)_{t=n}
     *         + \left(\frac{\partial{R}}{\partial{x}}\right)_{t=n}^{T}\lambda_{t=n}
     *         + \left(\frac{\partial{H}}{\partial{x}}\right)_{t=n}^{T}\gamma_{t=n}
     *         + \left(\frac{\partial{P}}{\partial{x}}\right)_{t=n}^{T}\mu_{t=n} /f$,
     *
     * where R is the global residual, H is the local residual, P is the projection residual,
     * and /f$\lambda/f$ is the global adjoint vector, /f$\gamma/f$ is the local adjoint vector,
     * x denotes the configuration variables, and /f$\mu/f$ is the projection adjoint vector.
     * The pseudo time is denoted by t, where n denotes the current step index.
     *
     * \param [in] aControls 1D view of control variables, i.e. design variables
     * \param [in] aStateData state data manager
     * \param [in] aAdjointData adjoint data manager
     * \param [in/out] aGradient total derivative wrt configuration
    *******************************************************************************/
    void addPDEpartialDerivativeX(const Plato::ScalarVector &aControls,
                                  const Plato::StateData &aStateData,
                                  const Plato::AdjointProblemStates &aAdjointStates,
                                  Plato::ScalarVector &aGradient)
    {
        // Allocate return gradient
        auto tNumCells = mGlobalResidualEq->numCells();
        Plato::ScalarMultiVector tGradientConfiguration("Gradient WRT Configuration", tNumCells, mNumConfigDofsPerCell);

        // add global adjoint contribution to total gradient, i.e. DfDx += (DrDx)^T * lambda
        Plato::ScalarMultiVector tCurrentLambda("Current Global State Adjoint", tNumCells, mNumGlobalDofsPerCell);
        mWorksetBase.worksetState(aAdjointStates.mCurrentGlobalAdjoint, tCurrentLambda);
        auto tDrDx = mGlobalResidualEq->gradient_x(aStateData.mCurrentGlobalState, aStateData.mPreviousGlobalState,
                                                  aStateData.mCurrentLocalState, aStateData.mPreviousLocalState,
                                                  aStateData.mProjectedPressGrad, aControls, aStateData.mCurrentStepIndex);
        const Plato::Scalar tAlpha = 1.0; Plato::Scalar tBeta = 0.0;
        Plato::matrix_times_vector_workset("T", tAlpha, tDrDx, tCurrentLambda, tBeta, tGradientConfiguration);

        // add projected pressure gradient adjoint contribution to total gradient, i.e. DfDx += (DpDx)^T * gamma
        Plato::ScalarMultiVector tCurrentGamma("Current Projected Pressure Gradient Adjoint", tNumCells, mNumPressGradDofsPerCell);
        mWorksetBase.worksetNodeState(aAdjointStates.mProjPressGradAdjoint, tCurrentGamma);
        auto tDpDx = mProjectionEq->gradient_x_workset(aStateData.mProjectedPressGrad,
                                                      mPressure,
                                                      aControls,
                                                      aStateData.mCurrentStepIndex);
        tBeta = 1.0;
        Plato::matrix_times_vector_workset("T", tAlpha, tDpDx, tCurrentGamma, tBeta, tGradientConfiguration);

        // compute local contribution to total gradient, i.e. (DhDx)^T * mu
        Plato::ScalarMultiVector tCurrentMu("Current Local State Adjoint", tNumCells, mNumLocalDofsPerCell);
        mWorksetBase.worksetLocalState(aAdjointStates.mCurrentLocalAdjoint, tCurrentMu);
        auto tDhDx = mLocalResidualEq->gradient_x(aStateData.mCurrentGlobalState, aStateData.mPreviousGlobalState,
                                                   aStateData.mCurrentLocalState, aStateData.mPreviousLocalState,
                                                   aControls, aStateData.mCurrentStepIndex);
        Plato::matrix_times_vector_workset("T", tAlpha, tDhDx, tCurrentMu, tBeta, tGradientConfiguration);

        mWorksetBase.assembleVectorGradientX(tGradientConfiguration, aGradient);
    }

    /***************************************************************************//**
     * \brief Update projected pressure gradient adjoint variables, /f$ \gamma_k /f$
     *   as follows:
     *  /f$
     *    \gamma_{k} =
     *      -\left(
     *          \left(\frac{\partial{P}}{\partial{\pi}}\right)_{t=k}^{T}
     *       \right)^{-1}
     *       \left[
     *          \left(\frac{\partial{R}}{\partial{\pi}}\right)_{t=k+1}^{T}\lambda_{k+1}
     *       \right]
     *  /f$,
     * where R is the global residual, P is the projected pressure gradient residual,
     * and /f$\pi/f$ is the projected pressure gradient. The pseudo time step index is
     * denoted by k.
     *
     * \param [in] aControls    1D view of control variables, i.e. design variables
     * \param [in] aStateData   state data manager
     * \param [in] aAdjointData adjoint data manager
    *******************************************************************************/
    void updateProjPressGradAdjointVars(const Plato::ScalarVector & aControls,
                                        const Plato::StateData& aStateData,
                                        Plato::AdjointProblemStates& aAdjointStates)
    {
        auto tLastStepIndex = mNumPseudoTimeSteps - static_cast<Plato::OrdinalType>(1);
        if(aStateData.mCurrentStepIndex == tLastStepIndex)
        {
            Plato::fill(static_cast<Plato::Scalar>(0.0), aAdjointStates.mProjPressGradAdjoint);
            return;
        }

        // Compute Jacobian tDrDp_{k+1}^T, i.e. transpose of Jacobian with respect to projected pressure gradient
        auto tDrDp_T =
            mGlobalResidualEq->gradient_n_T_assembled(aStateData.mCurrentGlobalState, aStateData.mPreviousGlobalState,
                                                     aStateData.mCurrentLocalState , aStateData.mPreviousLocalState,
                                                     aStateData.mProjectedPressGrad, aControls, aStateData.mCurrentStepIndex);

        // Compute tDrDp_{k+1}^T * lambda_{k+1}
        auto tNumProjPressGradDofs = mProjectionEq->size();
        Plato::ScalarVector tResidual("Projected Pressure Gradient Residual", tNumProjPressGradDofs);
        Plato::MatrixTimesVectorPlusVector(tDrDp_T, aAdjointStates.mPreviousGlobalAdjoint, tResidual);
        Plato::scale(static_cast<Plato::Scalar>(-1), tResidual);

        // Solve for current projected pressure gradient adjoint, i.e.
        //   gamma_k =  INV(tDpDp_k^T) * (tDrDp_{k+1}^T * lambda_{k+1})
        auto tProjJacobian = mProjectionEq->gradient_u_T(aStateData.mProjectedPressGrad, mPressure,
                                                        aControls, aStateData.mCurrentStepIndex);

        Plato::fill(static_cast<Plato::Scalar>(0.0), aAdjointStates.mProjPressGradAdjoint);
        Plato::Solve::RowSummed<PhysicsT::mNumSpatialDims>(tProjJacobian, aAdjointStates.mProjPressGradAdjoint, tResidual);
    }

    /***************************************************************************//**
     * \brief Update local adjoint vector using the following equation:
     *
     *  /f$ \mu_k =
     *      -\left(
     *          \left(\frac{\partial{H}}{\partial{c}}\right)_{t=k}^{T}
     *       \right)^{-1}
     *       \left[
     *           \left( \frac{\partial{R}}{\partial{c}} \right)_{t=k}^{T}\lambda_k +
     *           \left( \frac{\partial{f}}{\partial{c}}_{k}
     *                + \left( \frac{\partial{H}}{\partial{c}} \right)_{t=k+1}^{T} \mu_{k+1}
     *       \right]
     *  /f$,
     *
     * where R is the global residual, H is the local residual, u is the global state,
     * c is the local state, f is the performance criterion (e.g. objective function),
     * and /f$\gamma/f$ is the local adjoint vector. The pseudo time is denoted by t,
     * where n denotes the current step index and n+1 is the previous time step index.
     *
     * \param [in] aCriterion performance criterion interface
     * \param [in] aControls 1D view of control variables, i.e. design variables
     * \param [in] aStateData state data manager
     * \param [in] aInvLocalJacobianT inverse of transpose local Jacobian wrt local states
     * \param [in] aAdjointData adjoint data manager
    *******************************************************************************/
    void updateLocalAdjointVars(Plato::LocalScalarFunctionInc& aCriterion,
                                const Plato::ScalarVector & aControls,
                                const Plato::StateData& aCurrentStates,
                                const Plato::StateData& aPreviousStates,
                                const Plato::ScalarArray3D& aInvLocalJacobianT,
                                Plato::AdjointProblemStates & aAdjointStates)
    {
        // Compute DfDc_{k}
        auto tDfDc = aCriterion.gradient_c(aCurrentStates.mCurrentGlobalState, aCurrentStates.mPreviousGlobalState,
                                           aCurrentStates.mCurrentLocalState, aCurrentStates.mPreviousLocalState, 
                                           aControls, aCurrentStates.mCurrentStepIndex);

        // Compute DfDc_k + ( DrDc_k^T * lambda_k )
        auto tNumCells = mLocalResidualEq->numCells();
        Plato::ScalarMultiVector tCurrentLambda("Current Global Adjoint Workset", tNumCells, mNumGlobalDofsPerCell);
        mWorksetBase.worksetState(aAdjointStates.mCurrentGlobalAdjoint, tCurrentLambda);
        auto tDrDc = mGlobalResidualEq->gradient_c(aCurrentStates.mCurrentGlobalState, aCurrentStates.mPreviousGlobalState,
                                                  aCurrentStates.mCurrentLocalState , aCurrentStates.mPreviousLocalState,
                                                  aCurrentStates.mProjectedPressGrad, aControls, aCurrentStates.mCurrentStepIndex);
        Plato::Scalar tAlpha = 1.0; Plato::Scalar tBeta = 1.0;
        Plato::matrix_times_vector_workset("T", tAlpha, tDrDc, tCurrentLambda, tBeta, tDfDc);

        auto tFinalStepIndex = mNumPseudoTimeSteps - static_cast<Plato::OrdinalType>(1);
        if(aCurrentStates.mCurrentStepIndex != tFinalStepIndex)
        {
            // Compute DfDc_k + ( DrDc_k^T * lambda_k ) + DfDc_{k+1}
            const Plato::Scalar tAlpha = 1.0; const Plato::Scalar tBeta = 1.0;
            auto tDfDcp = aCriterion.gradient_cp(aPreviousStates.mCurrentGlobalState, aPreviousStates.mPreviousGlobalState,
                                                 aPreviousStates.mCurrentLocalState, aPreviousStates.mPreviousLocalState, 
                                                 aControls, aCurrentStates.mCurrentStepIndex);
            Plato::update_array_2D(tAlpha, tDfDcp, tBeta, tDfDc);

            // Compute DfDc_k + ( DrDc_k^T * lambda_k ) + DfDc_{k+1} + ( DhDc_{k+1}^T * mu_{k+1} )
            Plato::ScalarMultiVector tPreviousMu("Previous Local Adjoint Workset", tNumCells, mNumLocalDofsPerCell);
            mWorksetBase.worksetLocalState(aAdjointStates.mPreviousLocalAdjoint, tPreviousMu);
            auto tDhDcp = mLocalResidualEq->gradient_cp(aPreviousStates.mCurrentGlobalState, aPreviousStates.mPreviousGlobalState,
                                                       aPreviousStates.mCurrentLocalState , aPreviousStates.mPreviousLocalState,
                                                       aControls, aCurrentStates.mCurrentStepIndex);
            Plato::matrix_times_vector_workset("T", tAlpha, tDhDcp, tPreviousMu, tBeta, tDfDc);
            
            // Compute RHS_{local} = DfDc_k + ( DrDc_k^T * lambda_k ) + DfDc_{k+1} + ( DhDc_{k+1}^T * mu_{k+1} ) + ( DrDc_{k+1}^T * lambda_{k+1} )
            Plato::ScalarMultiVector tPrevLambda("Previous Global Adjoint Workset", tNumCells, mNumGlobalDofsPerCell);
            mWorksetBase.worksetState(aAdjointStates.mPreviousGlobalAdjoint, tPrevLambda);
            auto tDrDcp = mGlobalResidualEq->gradient_cp(aPreviousStates.mCurrentGlobalState, aPreviousStates.mPreviousGlobalState,
                                                         aPreviousStates.mCurrentLocalState , aPreviousStates.mPreviousLocalState,
                                                         aCurrentStates.mProjectedPressGrad, aControls, aCurrentStates.mCurrentStepIndex);
            Plato::matrix_times_vector_workset("T", tAlpha, tDrDcp, tPrevLambda, tBeta, tDfDc);
        }

        // Solve for current local adjoint variables, i.e. mu_k = -Inv(tDhDc_k^T) * RHS_{local}
        tAlpha = -1.0; tBeta = 0.0;
        Plato::ScalarMultiVector tCurrentMu("Current Local Adjoint Workset", tNumCells, mNumLocalDofsPerCell);
        Plato::matrix_times_vector_workset("T", tAlpha, aInvLocalJacobianT, tDfDc, tBeta, tCurrentMu);
        Plato::flatten_vector_workset<mNumLocalDofsPerCell>(tNumCells, tCurrentMu, aAdjointStates.mCurrentLocalAdjoint);
    }

    /***************************************************************************//**
     * \brief Update current global adjoint variables, i.e. /f$ \lambda_k /f$
     * \param [in] aCriterion         performance criterion interface
     * \param [in] aControls          1D view of control variables, i.e. design variables
     * \param [in] aCurrentStates         state data manager
     * \param [in] aInvLocalJacobianT inverse of transpose local Jacobian wrt local states
     * \param [in] aAdjointData       adjoint data manager
    *******************************************************************************/
    void updateGlobalAdjointVars(Plato::LocalScalarFunctionInc& aCriterion,
                                 const Plato::ScalarVector & aControls,
                                 const Plato::StateData& aCurrentStates,
                                 const Plato::StateData& aPreviousStates,
                                 const Plato::ScalarArray3D& aInvLocalJacobianT,
                                 Plato::AdjointProblemStates & aAdjointStates)
    {
        // Assemble adjoint Jacobian into mGlobalJacobian
        this->assembleTangentMatrix(aControls, aCurrentStates, aInvLocalJacobianT);
        // Assemble right hand side vector into mGlobalResidual
        this->assembleGlobalAdjointRHS(aCriterion, aControls, aCurrentStates, aPreviousStates, aInvLocalJacobianT, aAdjointStates);
        // Apply Dirichlet conditions for adjoint problem
        this->applyAdjointConstraints(mGlobalJacobian, mGlobalResidual);
        // Solve for lambda_k = (K_{tangent})_k^{-T} * F_k^{adjoint}
        Plato::fill(static_cast<Plato::Scalar>(0.0), aAdjointStates.mCurrentGlobalAdjoint);
        Plato::Solve::Consistent<mNumGlobalDofsPerNode>(mGlobalJacobian, aAdjointStates.mCurrentGlobalAdjoint, mGlobalResidual, false);
    }

    /***************************************************************************//**
     * \brief Apply Dirichlet constraints for adjoint problem
     * \param [in] aMatrix Compressed Row Storage (CRS) matrix
     * \param [in] aVector 1D view of Right-Hand-Side forces
    *******************************************************************************/
    void applyAdjointConstraints(const Teuchos::RCP<Plato::CrsMatrixType> & aMatrix, const Plato::ScalarVector & aVector)
    {
        Plato::ScalarVector tAdjointDirichletValues("Dirichlet Values", mDirichletValues.size());
        Plato::scale(static_cast<Plato::Scalar>(0.0), tAdjointDirichletValues);

        if(aMatrix->isBlockMatrix())
        {
            Plato::applyBlockConstraints<mNumGlobalDofsPerNode>(aMatrix, aVector, mDirichletDofs, tAdjointDirichletValues);
        }
        else
        {
            Plato::applyConstraints<mNumGlobalDofsPerNode>(aMatrix, aVector, mDirichletDofs, tAdjointDirichletValues);
        }
    }

    /***************************************************************************//**
     * \brief Compute contribution from local residual to global adjoint
     *   right-hand-side vector as follows:
     * /f$
     *  t=k\ \mbox{time step k}
     *   \bm{F}_k =
     *     -\left(
     *          \frac{f}{u}_k + \frac{P}{u}_k^T \gamma_k
     *        - \frac{H}{u}_k^T \left( \frac{H}{c}_k^{-T} \left[ \frac{F}{c}_k + \frac{H}{c}_{k+1}^T\mu_{k+1} \right] \right)
     *      \right)
     *  t=N\ \mbox{final time step}
     *   \bm{F}_k =
     *     -\left(
     *          \frac{f}{u}_k - \frac{H}{u}_k^T \left( \frac{H}{c}_k^{-T} \frac{F}{c}_k \right)
     *      \right)
     * /f$
     * \param [in] aCriterion         interface to design criterion
     * \param [in] aControls          design variables
     * \param [in] aStateData         state data structure
     * \param [in] aInvLocalJacobianT inverse of local Jacobian cell matrices
     * \param [in] aAdjointData       adjoint data structure
    *******************************************************************************/
    Plato::ScalarMultiVector
    computeLocalAdjointRHS(Plato::LocalScalarFunctionInc &aCriterion,
                           const Plato::ScalarVector &aControls,
                           const Plato::StateData &aCurrentStates,
                           const Plato::StateData &aPreviousStates,
                           const Plato::ScalarArray3D &aInvLocalJacobianT,
                           const Plato::AdjointProblemStates & aAdjointStates)
    {
        // Compute partial derivative of objective with respect to current local states
        auto tDfDc = aCriterion.gradient_c(aCurrentStates.mCurrentGlobalState, aCurrentStates.mPreviousGlobalState,
                                           aCurrentStates.mCurrentLocalState, aCurrentStates.mPreviousLocalState, 
                                           aControls, aCurrentStates.mCurrentStepIndex);

        auto tFinalStepIndex = mNumPseudoTimeSteps - static_cast<Plato::OrdinalType>(1);
        if(aCurrentStates.mCurrentStepIndex != tFinalStepIndex)
        {
            // Compute DfDx_k + DfDc_{k+1}, where k denotes the time step index
            const Plato::Scalar tAlpha = 1.0; const Plato::Scalar tBeta = 1.0;
            auto tDfDcp = aCriterion.gradient_cp(aPreviousStates.mCurrentGlobalState, aPreviousStates.mPreviousGlobalState,
                                                 aPreviousStates.mCurrentLocalState, aPreviousStates.mPreviousLocalState, 
                                                 aControls, aCurrentStates.mCurrentStepIndex);
            Plato::update_array_2D(tAlpha, tDfDcp, tBeta, tDfDc);

            // Compute DfDc_k + DfDc_{k+1} + ( DhDc_{k+1}^T * mu_{k+1} )
            auto tNumCells = mLocalResidualEq->numCells();
            Plato::ScalarMultiVector tPrevMu("Previous Local Adjoint Workset", tNumCells, mNumLocalDofsPerCell);
            mWorksetBase.worksetLocalState(aAdjointStates.mPreviousLocalAdjoint, tPrevMu);
            auto tDhDcp = mLocalResidualEq->gradient_cp(aPreviousStates.mCurrentGlobalState, aPreviousStates.mPreviousGlobalState,
                                                        aPreviousStates.mCurrentLocalState , aPreviousStates.mPreviousLocalState,
                                                        aControls, aCurrentStates.mCurrentStepIndex);
            Plato::matrix_times_vector_workset("T", tAlpha, tDhDcp, tPrevMu, tBeta, tDfDc);
            
            // Compute DfDc_k + DfDc_{k+1} + ( DhDc_{k+1}^T * mu_{k+1} ) + ( DrDc_{k+1}^T * lambda_{k+1} )
            Plato::ScalarMultiVector tPrevLambda("Previous Global Adjoint Workset", tNumCells, mNumGlobalDofsPerCell);
            mWorksetBase.worksetState(aAdjointStates.mPreviousGlobalAdjoint, tPrevLambda);
            auto tDrDcp = mGlobalResidualEq->gradient_cp(aPreviousStates.mCurrentGlobalState, aPreviousStates.mPreviousGlobalState,
                                                         aPreviousStates.mCurrentLocalState , aPreviousStates.mPreviousLocalState,
                                                         aPreviousStates.mProjectedPressGrad, aControls, aCurrentStates.mCurrentStepIndex);
            Plato::matrix_times_vector_workset("T", tAlpha, tDrDcp, tPrevLambda, tBeta, tDfDc);
        }

        // Compute Inv(tDhDc_k^T) * [ DfDc_k + DfDc_{k+1} + ( DhDc_{k+1}^T * mu_{k+1} ) + ( DrDc_{k+1}^T * lambda_{k+1} ) ]
        auto tNumCells = mLocalResidualEq->numCells();
        const Plato::Scalar tAlpha = 1.0; const Plato::Scalar tBeta = 0.0;
        Plato::ScalarMultiVector tLocalStateWorkSet("InvLocalJacobianTimesLocalVec", tNumCells, mNumLocalDofsPerCell);
        Plato::matrix_times_vector_workset("T", tAlpha, aInvLocalJacobianT, tDfDc, tBeta, tLocalStateWorkSet);

        // Compute local RHS <- tDhDu_k^T * { Inv(tDhDc_k^T) * [ DfDc_k + DfDc_{k+1} + ( DhDc_{k+1}^T * mu_{k+1} ) + ( DrDc_{k+1}^T * lambda_{k+1} ) }
        auto tDhDu = mLocalResidualEq->gradient_u(aCurrentStates.mCurrentGlobalState, aCurrentStates.mPreviousGlobalState,
                                                  aCurrentStates.mCurrentLocalState , aCurrentStates.mPreviousLocalState,
                                                  aControls, aCurrentStates.mCurrentStepIndex);
        Plato::ScalarMultiVector tLocalRHS("Local Adjoint RHS", tNumCells, mNumGlobalDofsPerCell);
        Plato::matrix_times_vector_workset("T", tAlpha, tDhDu, tLocalStateWorkSet, tBeta, tLocalRHS);

        return (tLocalRHS);
    }

    Plato::ScalarMultiVector
    computeProjPressGradAdjointRHS(const Plato::ScalarVector& aControls,
                                   const Plato::StateData& aStateData,
                                   const Plato::AdjointProblemStates& aAdjointStates)
    {
        // Compute partial derivative of projected pressure gradient residual wrt pressure field, i.e. DpDn
        auto tDpDn = mProjectionEq->gradient_n_workset(aStateData.mProjectedPressGrad,
                                                      mPressure,
                                                      aControls,
                                                      aStateData.mCurrentStepIndex);

        // Compute projected pressure gradient adjoint workset
        auto tNumCells = mProjectionEq->numCells();
        Plato::ScalarMultiVector tGamma("Projected Pressure Gradient Adjoint", tNumCells, mNumPressGradDofsPerCell);
        mWorksetBase.worksetNodeState(aAdjointStates.mProjPressGradAdjoint, tGamma);

        // Compute DpDn_k^T * gamma_k
        const Plato::Scalar tAlpha = 1.0; const Plato::Scalar tBeta = 1.0;
        const auto tNumPressureDofsPerCell = mProjectionEq->numNodeStatePerCell();
        Plato::ScalarMultiVector tOuput("DpDn_{k+1}^T * gamma_{k+1}", tNumCells, tNumPressureDofsPerCell);
        Plato::matrix_times_vector_workset("T", tAlpha, tDpDn, tGamma, tBeta, tOuput);

        return (tOuput);
    }

    /***************************************************************************//**
     * \brief Assemble global adjoint right hand side vector, which is given by:
     *
     * /f$ \bm{f} = \frac{\partial{f}}{\partial{u}}\right)_{t=n} - \left(\frac{\partial{H}}{\partial{u}}
     * \right)_{t=n}^{T} * \left[ \left( \left(\frac{\partial{H}}{\partial{c}}\right)_{t=n}^{T} \right)^{-1} *
     * \left(\frac{\partial{f}}{\partial{c}} + \frac{\partial{H}}{\partial{v}}\right)_{t=n+1}^{T} \gamma_{n+1}
     * \right]/f$,
     *
     * where R is the global residual, H is the local residual, u is the global state,
     * c is the local state, f is the performance criterion (e.g. objective function),
     * and /f$\gamma/f$ is the local adjoint vector. The pseudo time is denoted by t,
     * where n denotes the current step index and n+1 is the previous time step index.
     *
     * \param [in] aCriterion         performance criterion interface
     * \param [in] aControls          1D view of control variables, i.e. design variables
     * \param [in] aCurrentStates     current state data set
     * \param [in] aInvLocalJacobianT inverse of transpose local Jacobian wrt local states
     * \param [in] aAdjointData       adjoint data manager
    *******************************************************************************/
    void assembleGlobalAdjointRHS(Plato::LocalScalarFunctionInc& aCriterion,
                                  const Plato::ScalarVector& aControls,
                                  const Plato::StateData& aCurrentStates,
                                  const Plato::StateData& aPreviousStates,
                                  const Plato::ScalarArray3D& aInvLocalJacobianT,
                                  const Plato::AdjointProblemStates& aAdjointStates)
    {
        // Compute partial derivative of objective with respect to current global states
        auto tDfDu = aCriterion.gradient_u(aCurrentStates.mCurrentGlobalState, aCurrentStates.mPreviousGlobalState,
                                           aCurrentStates.mCurrentLocalState, aCurrentStates.mPreviousLocalState, 
                                           aControls, aCurrentStates.mCurrentStepIndex);

        // Compute previous adjoint states contribution to global adjoint rhs
        auto tFinalStepIndex = mNumPseudoTimeSteps - static_cast<Plato::OrdinalType>(1);
        if(aCurrentStates.mCurrentStepIndex != tFinalStepIndex)
        {
            // Compute partial derivative of objective with respect to previous global states, i.e. DfDu_{k+1}
            const Plato::Scalar tAlpha = 1.0; const Plato::Scalar tBeta = 1.0;
            auto tDfDup = aCriterion.gradient_up(aPreviousStates.mCurrentGlobalState, aPreviousStates.mPreviousGlobalState,
                                                 aPreviousStates.mCurrentLocalState, aPreviousStates.mPreviousLocalState, 
                                                 aControls, aCurrentStates.mCurrentStepIndex);
            Plato::update_array_2D(tAlpha, tDfDup, tBeta, tDfDu);

            // Compute projected pressure gradient contribution to global adjoint rhs, i.e. DpDu_{k+1}^T * gamma_{k+1}
            auto tProjPressGradAdjointRHS = this->computeProjPressGradAdjointRHS(aControls, aPreviousStates, aAdjointStates);
            Plato::axpy_array_2D<mNumGlobalDofsPerNode, mPressureDofOffset>(tAlpha, tProjPressGradAdjointRHS, tDfDu);
            
            // Compute global residual contribution to global adjoint RHS, i.e. DrDu_{k+1}^T * lambda_{k+1}
            auto tNumCells = mGlobalResidualEq->numCells();
            Plato::ScalarMultiVector tPrevLambda("Previous Global Adjoint Workset", tNumCells, mNumGlobalDofsPerCell);
            mWorksetBase.worksetState(aAdjointStates.mPreviousGlobalAdjoint, tPrevLambda);
            auto tDrDup = mGlobalResidualEq->gradient_up(aPreviousStates.mCurrentGlobalState, aPreviousStates.mPreviousGlobalState,
                                                         aPreviousStates.mCurrentLocalState , aPreviousStates.mPreviousLocalState,
                                                         aPreviousStates.mProjectedPressGrad, aControls, aCurrentStates.mCurrentStepIndex);
            Plato::matrix_times_vector_workset("T", tAlpha, tDrDup, tPrevLambda, tBeta, tDfDu);
            
            // Compute local residual contribution to global adjoint RHS, i.e. DhDu_{k+1}^T * mu_{k+1}
            Plato::ScalarMultiVector tPrevMu("Previous Local Adjoint Workset", tNumCells, mNumLocalDofsPerCell);
            mWorksetBase.worksetLocalState(aAdjointStates.mPreviousLocalAdjoint, tPrevMu);
            auto tDhDup = mLocalResidualEq->gradient_up(aPreviousStates.mCurrentGlobalState, aPreviousStates.mPreviousGlobalState,
                                                        aPreviousStates.mCurrentLocalState , aPreviousStates.mPreviousLocalState,
                                                        aControls, aCurrentStates.mCurrentStepIndex);
            Plato::matrix_times_vector_workset("T", tAlpha, tDhDup, tPrevMu, tBeta, tDfDu);
        }

        // Compute and add local contribution to global adjoint rhs, i.e. tDfDu_k - F_k^{local}
        auto tLocalStateAdjointRHS =
            this->computeLocalAdjointRHS(aCriterion, aControls, aCurrentStates, aPreviousStates, aInvLocalJacobianT, aAdjointStates);
        const Plato::Scalar  tAlpha = -1.0; const Plato::Scalar tBeta = 1.0;
        Plato::update_array_2D(tAlpha, tLocalStateAdjointRHS, tBeta, tDfDu);

        // Assemble -( DfDu_k + DfDup + (DpDup_T * gamma_{k+1}) - F_k^{local} )
        Plato::fill(static_cast<Plato::Scalar>(0), mGlobalResidual);
        mWorksetBase.assembleVectorGradientU(tDfDu, mGlobalResidual);
        Plato::scale(static_cast<Plato::Scalar>(-1), mGlobalResidual);
    }

    /***************************************************************************//**
     * \brief Update inverse of local Jacobian wrt local states, i.e.
     * /f$ \left[ \left( \frac{\partial{H}}{\partial{c}} \right)_{t=n} \right]^{-1}, /f$:
     *
     * where H is the local residual and c is the local state vector. The pseudo time is
     * denoted by t, where n denotes the current step index.
     *
     * \param [in] aControls 1D view of control variables, i.e. design variables
     * \param [in] aStateData state data manager
     * \param [in] aInvLocalJacobianT inverse of local Jacobian wrt local states
    *******************************************************************************/
    template<class StateDataType>
    void updateInverseLocalJacobian(const Plato::ScalarVector & aControls,
                                    const StateDataType & aStateData,
                                    Plato::ScalarArray3D& aInvLocalJacobianT)
    {
        auto tNumCells = mLocalResidualEq->numCells();
        auto tDhDc = mLocalResidualEq->gradient_c(aStateData.mCurrentGlobalState, aStateData.mPreviousGlobalState,
                                                 aStateData.mCurrentLocalState , aStateData.mPreviousLocalState,
                                                 aControls, aStateData.mCurrentStepIndex);
        Plato::inverse_matrix_workset<mNumLocalDofsPerCell, mNumLocalDofsPerCell>(tNumCells, tDhDc, aInvLocalJacobianT);
    }

    /***************************************************************************//**
     * \brief Allocate objective function interface and adjoint containers
     * \param [in] aMesh mesh database
     * \param [in] aMeshSets side sets database
     * \param [in] aInputParams input parameters database
    *******************************************************************************/
    void allocateObjectiveFunction(Omega_h::Mesh& aMesh, Omega_h::MeshSets& aMeshSets, Teuchos::ParameterList& aInputParams)
    {
        if(aInputParams.isType<std::string>("Objective"))
        {
            auto tUserDefinedName = aInputParams.get<std::string>("Objective");
            Plato::PathDependentScalarFunctionFactory<PhysicsT> tObjectiveFunctionFactory;
            mObjective = tObjectiveFunctionFactory.create(aMesh, aMeshSets, mDataMap, aInputParams, tUserDefinedName);
        }
        else
        {
            WARNING("OBJECTIVE FUNCTION IS DISABLED FOR THIS PROBLEM")
        }
    }

    /***************************************************************************//**
     * \brief Allocate constraint function interface and adjoint containers
     * \param [in] aMesh mesh database
     * \param [in] aMeshSets side sets database
     * \param [in] aInputParams input parameters database
    *******************************************************************************/
    void allocateConstraintFunction(Omega_h::Mesh& aMesh, Omega_h::MeshSets& aMeshSets, Teuchos::ParameterList& aInputParams)
    {
        if(aInputParams.isType<std::string>("Constraint"))
        {
            Plato::PathDependentScalarFunctionFactory<PhysicsT> tContraintFunctionFactory;
            auto tUserDefinedName = aInputParams.get<std::string>("Constraint");
            mConstraint = tContraintFunctionFactory.create(aMesh, aMeshSets, mDataMap, aInputParams, tUserDefinedName);
        }
        else
        {
            WARNING("CONSTRAINT IS DISABLED FOR THIS PROBLEM")
        }
    }
};
// class PlasticityProblem



















template<typename SimplexPhysics>
struct DiagnosticDataPlasticity
{
public:
    Plato::ScalarVector mControl;
    Plato::ScalarVector mPresssure;
    Plato::ScalarVector mPrevLocalState;
    Plato::ScalarVector mPrevGlobalState;
    Plato::ScalarVector mCurrentLocalState;
    Plato::ScalarVector mCurrentGlobalState;

    DiagnosticDataPlasticity(const Plato::OrdinalType & aNumVerts, const Plato::OrdinalType & aNumCells) :
            mControl(Plato::ScalarVector("Control", aNumVerts)),
            mPresssure(Plato::ScalarVector("Pressure", aNumVerts * SimplexPhysics::mNumNodeStatePerNode)),
            mPrevLocalState(Plato::ScalarVector("Previous Local State", aNumCells * SimplexPhysics::mNumLocalDofsPerCell)),
            mPrevGlobalState(Plato::ScalarVector("Previous Global State", aNumVerts * SimplexPhysics::mNumDofsPerNode)),
            mCurrentLocalState(Plato::ScalarVector("Current Local State", aNumCells * SimplexPhysics::mNumLocalDofsPerCell)),
            mCurrentGlobalState(Plato::ScalarVector("Current Global State", aNumVerts * SimplexPhysics::mNumDofsPerNode))
    {
        this->initialize();
    }

    ~DiagnosticDataPlasticity()
    {
    }

private:
    void initialize()
    {
        auto tHostControl = Kokkos::create_mirror(mControl);
        Plato::random(0.5, 0.75, tHostControl);
        Kokkos::deep_copy(mControl, tHostControl);

        auto tHostPresssure = Kokkos::create_mirror(mPresssure);
        Plato::random(0.1, 0.5, tHostPresssure);
        Kokkos::deep_copy(mPresssure, tHostPresssure);

        auto tHostPrevLocalState = Kokkos::create_mirror(mPrevLocalState);
        Plato::random(0.1, 0.9, tHostPrevLocalState);
        Kokkos::deep_copy(mPrevLocalState, tHostPrevLocalState);

        auto tHostPrevGlobalState = Kokkos::create_mirror(mPrevGlobalState);
        Plato::random(1, 5, tHostPrevGlobalState);
        Kokkos::deep_copy(mPrevGlobalState, tHostPrevGlobalState);

        auto tHostCurrentLocalState = Kokkos::create_mirror(mCurrentLocalState);
        Plato::random(1.0, 2.0, tHostCurrentLocalState);
        Kokkos::deep_copy(mCurrentLocalState, tHostCurrentLocalState);

        auto tHostCurrentGlobalState = Kokkos::create_mirror(mCurrentGlobalState);
        Plato::random(1, 5, tHostCurrentGlobalState);
        Kokkos::deep_copy(mCurrentGlobalState, tHostCurrentGlobalState);
    }
};
















template<class PlatoProblem>
inline Plato::Scalar test_objective_grad_wrt_control(PlatoProblem & aProblem, Omega_h::Mesh & aMesh)
{
    // Allocate Data
    const Plato::OrdinalType tNumVerts = aMesh.nverts();
    Plato::ScalarVector tControls = Plato::ScalarVector("Controls", tNumVerts);
    Plato::fill(0.5, tControls);

    Plato::ScalarVector tStep = Plato::ScalarVector("Step", tNumVerts);
    auto tHostStep = Kokkos::create_mirror(tStep);
    Plato::random(0.025, 0.05, tHostStep);
    Kokkos::deep_copy(tStep, tHostStep);

    // Compute gradient
    auto tGlobalStates = aProblem.solution(tControls);
    auto tObjGradZ = aProblem.objectiveGradient(tControls, tGlobalStates);
    auto tGradientDotStep = Plato::dot(tObjGradZ, tStep);
    
    std::ostringstream tOutput;
    tOutput << std::right << std::setw(18) << "\nStep Size" << std::setw(20) << "Grad'*Step" 
        << std::setw(18) << "FD Approx" << std::setw(20) << "abs(Error)" << "\n";

    constexpr Plato::OrdinalType tSuperscriptLowerBound = 1;
    constexpr Plato::OrdinalType tSuperscriptUpperBound = 6;
    auto tTrialControl = Plato::ScalarVector("Trial Control", tNumVerts);

    std::vector<Plato::Scalar> tFiniteDiffApproxError;
    for(Plato::OrdinalType tIndex = tSuperscriptLowerBound; tIndex <= tSuperscriptUpperBound; tIndex++)
    {
        auto tEpsilon = static_cast<Plato::Scalar>(1) / std::pow(static_cast<Plato::Scalar>(10), tIndex);

        // four point finite difference approximation
        Plato::update(1.0, tControls, 0.0, tTrialControl);
        Plato::update(tEpsilon, tStep, 1.0, tTrialControl);
        tGlobalStates = aProblem.solution(tTrialControl);
        auto tValuePlus1Eps = aProblem.objectiveValue(tTrialControl, tGlobalStates);

        Plato::update(1.0, tControls, 0.0, tTrialControl);
        Plato::update(-tEpsilon, tStep, 1.0, tTrialControl);
        tGlobalStates = aProblem.solution(tTrialControl);
        auto tValueMinus1Eps = aProblem.objectiveValue(tTrialControl, tGlobalStates);

        Plato::update(1.0, tControls, 0.0, tTrialControl);
        Plato::update(2.0 * tEpsilon, tStep, 1.0, tTrialControl);
        tGlobalStates = aProblem.solution(tTrialControl);
        auto tValuePlus2Eps = aProblem.objectiveValue(tTrialControl, tGlobalStates);

        Plato::update(1.0, tControls, 0.0, tTrialControl);
        Plato::update(-2.0 * tEpsilon, tStep, 1.0, tTrialControl);
        tGlobalStates = aProblem.solution(tTrialControl);
        auto tValueMinus2Eps = aProblem.objectiveValue(tTrialControl, tGlobalStates);

        auto tNumerator = -tValuePlus2Eps + static_cast<Plato::Scalar>(8.) * tValuePlus1Eps
                - static_cast<Plato::Scalar>(8.) * tValueMinus1Eps + tValueMinus2Eps;
        auto tDenominator = static_cast<Plato::Scalar>(12.) * tEpsilon;
        auto tFiniteDiffAppx = tNumerator / tDenominator;
        auto tAppxError = abs(tFiniteDiffAppx - tGradientDotStep);
        tFiniteDiffApproxError.push_back(tAppxError);

        tOutput << std::right << std::scientific << std::setprecision(8) << std::setw(14) << tEpsilon << std::setw(19)
            << tGradientDotStep << std::setw(19) << tFiniteDiffAppx << std::setw(19) << tAppxError << "\n";
    }
    std::cout << tOutput.str().c_str();
    
    const auto tMinError = *std::min_element(tFiniteDiffApproxError.begin(), tFiniteDiffApproxError.end());
    return tMinError;
}













/******************************************************************************//**
 * \brief Test partial derivative of scalar function with path-dependent variables
 *        with respect to the control variables.
 * \param [in] aMesh           mesh database
 * \param [in] aScalarFunction scalar function to evaluate derivative of
 * \param [in] aTimeStep       time step index
**********************************************************************************/
template<typename SimplexPhysics>
inline void
test_partial_local_scalar_func_wrt_control
(std::shared_ptr<Plato::LocalScalarFunctionInc> & aScalarFunc, Omega_h::Mesh & aMesh, Plato::Scalar aTimeStep = 0.0)
{
    const Plato::OrdinalType tNumCells = aMesh.nelems();
    const Plato::OrdinalType tNumVerts = aMesh.nverts();
    Plato::DiagnosticDataPlasticity<SimplexPhysics> tData(tNumVerts, tNumCells);
    auto tPartialZ = aScalarFunc->gradient_z(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                             tData.mCurrentLocalState, tData.mPrevLocalState, 
                                             tData.mControl, aTimeStep);

    Plato::WorksetBase<SimplexPhysics> tWorksetBase(aMesh);
    Plato::ScalarVector tAssembledPartialZ("assembled partial control", tNumVerts);
    tWorksetBase.assembleScalarGradientZ(tPartialZ, tAssembledPartialZ);

    Plato::ScalarVector tStep("control step", tNumVerts);
    auto tHostStep = Kokkos::create_mirror(tStep);
    Plato::random(0.05, 0.1, tHostStep);
    Kokkos::deep_copy(tStep, tHostStep);
    const Plato::Scalar tGradientDotStep = Plato::dot(tAssembledPartialZ, tStep);

    std::cout << std::right << std::setw(18) << "\nStep Size" << std::setw(20) << "Grad'*Step"
              << std::setw(18) << "FD Approx" << std::setw(20) << "abs(Error)" << "\n";

    constexpr Plato::OrdinalType tSuperscriptLowerBound = 1;
    constexpr Plato::OrdinalType tSuperscriptUpperBound = 6;
    Plato::ScalarVector tTrialControl("trial control", tNumVerts);

    for(Plato::OrdinalType tIndex = tSuperscriptLowerBound; tIndex <= tSuperscriptUpperBound; tIndex++)
    {
        Plato::Scalar tEpsilon = tEpsilon = static_cast<Plato::Scalar>(1) /
                std::pow(static_cast<Plato::Scalar>(10), tIndex);
        // four point finite difference approximation
        Plato::update(1.0, tData.mControl, 0.0, tTrialControl);
        Plato::update(tEpsilon, tStep, 1.0, tTrialControl);
        Plato::Scalar tValuePlus1Eps = aScalarFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                          tData.mCurrentLocalState, tData.mPrevLocalState, 
                                                          tTrialControl, aTimeStep);
        Plato::update(1.0, tData.mControl, 0.0, tTrialControl);
        Plato::update(-tEpsilon, tStep, 1.0, tTrialControl);
        Plato::Scalar tValueMinus1Eps = aScalarFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                           tData.mCurrentLocalState, tData.mPrevLocalState, 
                                                           tTrialControl, aTimeStep);
        Plato::update(1.0, tData.mControl, 0.0, tTrialControl);
        Plato::update(2.0 * tEpsilon, tStep, 1.0, tTrialControl);
        Plato::Scalar tValuePlus2Eps = aScalarFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                          tData.mCurrentLocalState, tData.mPrevLocalState, 
                                                          tTrialControl, aTimeStep);
        Plato::update(1.0, tData.mControl, 0.0, tTrialControl);
        Plato::update(-2.0 * tEpsilon, tStep, 1.0, tTrialControl);
        Plato::Scalar tValueMinus2Eps = aScalarFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                           tData.mCurrentLocalState, tData.mPrevLocalState, 
                                                           tTrialControl, aTimeStep);

        Plato::Scalar tNumerator = -tValuePlus2Eps + static_cast<Plato::Scalar>(8.) * tValuePlus1Eps
                - static_cast<Plato::Scalar>(8.) * tValueMinus1Eps + tValueMinus2Eps;
        Plato::Scalar tDenominator = static_cast<Plato::Scalar>(12.) * tEpsilon;
        Plato::Scalar tFiniteDiffAppx = tNumerator / tDenominator;
        Plato::Scalar tAppxError = abs(tFiniteDiffAppx - tGradientDotStep);

        std::cout << std::right << std::scientific << std::setprecision(8) << std::setw(14) << tEpsilon << std::setw(19)
              << tGradientDotStep << std::setw(19) << tFiniteDiffAppx << std::setw(19) << tAppxError << "\n";
    }
}
// function test_partial_local_scalar_func_wrt_control

/******************************************************************************//**
 * \brief Test partial derivative of scalar function with path-dependent variables
 *        with respect to the current global state variables.
 * \param [in] aMesh           mesh database
 * \param [in] aScalarFunction scalar function to evaluate derivative of
 * \param [in] aTimeStep       time step index
**********************************************************************************/
template<typename SimplexPhysics>
inline void
test_partial_local_scalar_func_wrt_current_global_state
(std::shared_ptr<Plato::LocalScalarFunctionInc> & aScalarFunc, Omega_h::Mesh & aMesh, Plato::Scalar aTimeStep = 0.0)
{
    const Plato::OrdinalType tNumCells = aMesh.nelems();
    const Plato::OrdinalType tNumVerts = aMesh.nverts();
    Plato::DiagnosticDataPlasticity<SimplexPhysics> tData(tNumVerts, tNumCells);
    auto tPartialU = aScalarFunc->gradient_u(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                             tData.mCurrentLocalState, tData.mPrevLocalState, 
                                             tData.mControl, aTimeStep);

    const auto tTotalNumGlobalDofs = tNumVerts * SimplexPhysics::mNumDofsPerNode;
    Plato::ScalarVector tAssembledPartialU("assembled partial current global state", tTotalNumGlobalDofs);
    Plato::WorksetBase<SimplexPhysics> tWorksetBase(aMesh);
    tWorksetBase.assembleVectorGradientU(tPartialU, tAssembledPartialU);

    Plato::ScalarVector tStep("current global state step", tTotalNumGlobalDofs);
    auto tHostStep = Kokkos::create_mirror(tStep);
    Plato::random(0.05, 0.1, tHostStep);
    Kokkos::deep_copy(tStep, tHostStep);
    const auto tGradientDotStep = Plato::dot(tAssembledPartialU, tStep);

    std::cout << std::right << std::setw(18) << "\nStep Size" << std::setw(20) << "Grad'*Step"
              << std::setw(18) << "FD Approx" << std::setw(20) << "abs(Error)" << "\n";

    constexpr Plato::OrdinalType tSuperscriptLowerBound = 1;
    constexpr Plato::OrdinalType tSuperscriptUpperBound = 6;
    Plato::ScalarVector tTrialCurrentGlobalState("trial current global state", tTotalNumGlobalDofs);

    for(Plato::OrdinalType tIndex = tSuperscriptLowerBound; tIndex <= tSuperscriptUpperBound; tIndex++)
    {
        auto tEpsilon = static_cast<Plato::Scalar>(1) / std::pow(static_cast<Plato::Scalar>(10), tIndex);

        // four point finite difference approximation
        Plato::update(1.0, tData.mCurrentGlobalState, 0.0, tTrialCurrentGlobalState);
        Plato::update(tEpsilon, tStep, 1.0, tTrialCurrentGlobalState);
        auto tValuePlus1Eps = aScalarFunc->value(tTrialCurrentGlobalState, tData.mPrevGlobalState,
                                                 tData.mCurrentLocalState, tData.mPrevLocalState, 
                                                 tData.mControl, aTimeStep);

        Plato::update(1.0, tData.mCurrentGlobalState, 0.0, tTrialCurrentGlobalState);
        Plato::update(-tEpsilon, tStep, 1.0, tTrialCurrentGlobalState);
        auto tValueMinus1Eps = aScalarFunc->value(tTrialCurrentGlobalState, tData.mPrevGlobalState,
                                                  tData.mCurrentLocalState, tData.mPrevLocalState, 
                                                  tData.mControl, aTimeStep);

        Plato::update(1.0, tData.mCurrentGlobalState, 0.0, tTrialCurrentGlobalState);
        Plato::update(2.0 * tEpsilon, tStep, 1.0, tTrialCurrentGlobalState);
        auto tValuePlus2Eps = aScalarFunc->value(tTrialCurrentGlobalState, tData.mPrevGlobalState,
                                                 tData.mCurrentLocalState, tData.mPrevLocalState, 
                                                 tData.mControl, aTimeStep);

        Plato::update(1.0, tData.mCurrentGlobalState, 0.0, tTrialCurrentGlobalState);
        Plato::update(-2.0 * tEpsilon, tStep, 1.0, tTrialCurrentGlobalState);
        auto tValueMinus2Eps = aScalarFunc->value(tTrialCurrentGlobalState, tData.mPrevGlobalState,
                                                  tData.mCurrentLocalState, tData.mPrevLocalState, 
                                                  tData.mControl, aTimeStep);

        auto tNumerator = -tValuePlus2Eps + static_cast<Plato::Scalar>(8.) * tValuePlus1Eps
                - static_cast<Plato::Scalar>(8.) * tValueMinus1Eps + tValueMinus2Eps;
        auto tDenominator = static_cast<Plato::Scalar>(12.) * tEpsilon;
        auto tFiniteDiffAppx = tNumerator / tDenominator;
        auto tAppxError = abs(tFiniteDiffAppx - tGradientDotStep);

        std::cout << std::right << std::scientific << std::setprecision(8) << std::setw(14) << tEpsilon << std::setw(19)
              << tGradientDotStep << std::setw(19) << tFiniteDiffAppx << std::setw(19) << tAppxError << "\n";
    }
}
// function test_partial_local_scalar_func_wrt_current_global_state

/******************************************************************************//**
 * \brief Test partial derivative of scalar function with path-dependent variables
 *        with respect to the current local state variables.
 * \param [in] aMesh           mesh database
 * \param [in] aScalarFunction scalar function to evaluate derivative of
 * \param [in] aTimeStep       time step index
**********************************************************************************/
template<typename SimplexPhysics>
inline void
test_partial_local_scalar_func_wrt_current_local_state
(std::shared_ptr<Plato::LocalScalarFunctionInc> & aScalarFunc, Omega_h::Mesh & aMesh, Plato::Scalar aTimeStep = 0.0)
{
    const Plato::OrdinalType tNumCells = aMesh.nelems();
    const Plato::OrdinalType tNumVerts = aMesh.nverts();
    Plato::DiagnosticDataPlasticity<SimplexPhysics> tData(tNumVerts, tNumCells);
    auto tPartialC = aScalarFunc->gradient_c(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                             tData.mCurrentLocalState, tData.mPrevLocalState, 
                                             tData.mControl, aTimeStep);

    const auto tTotalNumLocalDofs = tNumCells * SimplexPhysics::mNumLocalDofsPerCell;
    Plato::ScalarVector tAssembledPartialC("assembled partial current local state", tTotalNumLocalDofs);
    Plato::WorksetBase<SimplexPhysics> tWorksetBase(aMesh);
    tWorksetBase.assembleVectorGradientC(tPartialC, tAssembledPartialC);

    Plato::ScalarVector tStep("current local state step", tTotalNumLocalDofs);
    auto tHostStep = Kokkos::create_mirror(tStep);
    Plato::random(0.05, 0.1, tHostStep);
    Kokkos::deep_copy(tStep, tHostStep);
    const auto tGradientDotStep = Plato::dot(tAssembledPartialC, tStep);

    std::cout << std::right << std::setw(18) << "\nStep Size" << std::setw(20) << "Grad'*Step"
              << std::setw(18) << "FD Approx" << std::setw(20) << "abs(Error)" << "\n";

    constexpr Plato::OrdinalType tSuperscriptLowerBound = 1;
    constexpr Plato::OrdinalType tSuperscriptUpperBound = 6;
    Plato::ScalarVector tTrialCurrentLocalState("trial current local state", tTotalNumLocalDofs);

    for(Plato::OrdinalType tIndex = tSuperscriptLowerBound; tIndex <= tSuperscriptUpperBound; tIndex++)
    {
        auto tEpsilon = static_cast<Plato::Scalar>(1) / std::pow(static_cast<Plato::Scalar>(10), tIndex);

        // four point finite difference approximation
        Plato::update(1.0, tData.mCurrentLocalState, 0.0, tTrialCurrentLocalState);
        Plato::update(tEpsilon, tStep, 1.0, tTrialCurrentLocalState);
        auto tValuePlus1Eps = aScalarFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                 tTrialCurrentLocalState, tData.mPrevLocalState, 
                                                 tData.mControl, aTimeStep);

        Plato::update(1.0, tData.mCurrentLocalState, 0.0, tTrialCurrentLocalState);
        Plato::update(-tEpsilon, tStep, 1.0, tTrialCurrentLocalState);
        auto tValueMinus1Eps = aScalarFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                  tTrialCurrentLocalState, tData.mPrevLocalState, 
                                                  tData.mControl, aTimeStep);

        Plato::update(1.0, tData.mCurrentLocalState, 0.0, tTrialCurrentLocalState);
        Plato::update(2.0 * tEpsilon, tStep, 1.0, tTrialCurrentLocalState);
        auto tValuePlus2Eps = aScalarFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                 tTrialCurrentLocalState, tData.mPrevLocalState, 
                                                 tData.mControl, aTimeStep);

        Plato::update(1.0, tData.mCurrentLocalState, 0.0, tTrialCurrentLocalState);
        Plato::update(-2.0 * tEpsilon, tStep, 1.0, tTrialCurrentLocalState);
        auto tValueMinus2Eps = aScalarFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                  tTrialCurrentLocalState, tData.mPrevLocalState, 
                                                  tData.mControl, aTimeStep);

        auto tNumerator = -tValuePlus2Eps + static_cast<Plato::Scalar>(8.) * tValuePlus1Eps
                - static_cast<Plato::Scalar>(8.) * tValueMinus1Eps + tValueMinus2Eps;
        auto tDenominator = static_cast<Plato::Scalar>(12.) * tEpsilon;
        auto tFiniteDiffAppx = tNumerator / tDenominator;
        auto tAppxError = abs(tFiniteDiffAppx - tGradientDotStep);

        std::cout << std::right << std::scientific << std::setprecision(8) << std::setw(14) << tEpsilon << std::setw(19)
              << tGradientDotStep << std::setw(19) << tFiniteDiffAppx << std::setw(19) << tAppxError << "\n";
    }
}
// function test_partial_local_scalar_func_wrt_current_local_state


template<typename SimplexPhysicsT>
inline void assemble_global_vector_jacobian_times_step
(const Plato::VectorEntryOrdinal<SimplexPhysicsT::mNumSpatialDims, SimplexPhysicsT::mNumDofsPerNode> & aEntryOrdinal,
 const Plato::ScalarArray3D & aWorkset,
 const Plato::ScalarVector & aVector,
 const Plato::ScalarVector & aOutput)
{
    const auto tNumCells = aWorkset.extent(0);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tNumCells), LAMBDA_EXPRESSION(const Plato::OrdinalType & aCellOrdinal)
    {
        for (Plato::OrdinalType tNodeIndex = 0; tNodeIndex < SimplexPhysicsT::mNumNodesPerCell; tNodeIndex++)
        {
            for (Plato::OrdinalType tGlobalDofIndex = 0; tGlobalDofIndex < SimplexPhysicsT::mNumDofsPerNode; tGlobalDofIndex++)
            {
                Plato::Scalar tValue = 0.0;
                auto tColIndex = aCellOrdinal * SimplexPhysicsT::mNumLocalDofsPerCell;
                for (Plato::OrdinalType tLocalDofIndex = 0; tLocalDofIndex < SimplexPhysicsT::mNumLocalDofsPerCell; tLocalDofIndex++)
                {
                    tColIndex += tLocalDofIndex;
                    tValue += aWorkset(aCellOrdinal, tGlobalDofIndex, tLocalDofIndex) * aVector(tColIndex);
                }
                const auto tRowIndex = aEntryOrdinal(aCellOrdinal, tNodeIndex, tGlobalDofIndex);
                //printf("CellIndex = %d, NodeIndex = %d, GlobalDofIndex = %d, RowIndex = %d\n", aCellOrdinal, tNodeIndex, tGlobalDofIndex, tRowIndex);
                Kokkos::atomic_add(&aOutput(tRowIndex), tValue);
            }
        }
    }, "assemble global vector Jacobian times vector");
}







/******************************************************************************//**
 * \brief Test partial derivative of vector function with path-dependent variables
 *        with respect to the current local state variables.
 * \param [in] aMesh           mesh database
 * \param [in] aScalarFunction scalar function to evaluate derivative of
 * \param [in] aTimeStep       time step index
**********************************************************************************/
template<typename SimplexPhysicsT, typename PhysicsT>
inline void
test_partial_global_jacobian_wrt_current_local_states
(std::shared_ptr<Plato::GlobalVectorFunctionInc<PhysicsT>> & aVectorFunc, Omega_h::Mesh & aMesh, Plato::Scalar aTimeStep = 0.0)
{
    // Compute workset Jacobians
    const Plato::OrdinalType tNumCells = aMesh.nelems();
    const Plato::OrdinalType tNumVerts = aMesh.nverts();
    Plato::DiagnosticDataPlasticity<SimplexPhysicsT> tData(tNumVerts, tNumCells);
    auto tJacobianCurrentC = aVectorFunc->gradient_c(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                     tData.mCurrentLocalState, tData.mPrevLocalState,
                                                     tData.mPresssure, tData.mControl, aTimeStep);

    // Assemble Jacobian and apply descent direction to assembled Jacobian
    auto const tTotalNumLocalStateDofs = tNumCells * SimplexPhysicsT::mNumLocalDofsPerCell;
    Plato::ScalarVector tStep("Step", tTotalNumLocalStateDofs);
    auto tHostStep = Kokkos::create_mirror(tStep);
    Plato::random(0.05, 0.1, tHostStep);
    Kokkos::deep_copy(tStep, tHostStep);
    auto const tTotalNumGlobalStateDofs = tNumVerts * SimplexPhysicsT::mNumDofsPerNode;
    Plato::ScalarVector tJacCtimesStep("JacCtimesVec", tTotalNumGlobalStateDofs);
    Plato::VectorEntryOrdinal<SimplexPhysicsT::mNumSpatialDims, SimplexPhysicsT::mNumDofsPerNode>
            tGlobalVectorEntryOrdinal(&aMesh);
    Plato::assemble_global_vector_jacobian_times_step<SimplexPhysicsT>
            (tGlobalVectorEntryOrdinal, tJacobianCurrentC, tStep, tJacCtimesStep);
    auto tNormTrueDerivative = Plato::norm(tJacCtimesStep);

    std::cout << std::right << std::setw(18) << "\nStep Size" << std::setw(20) << "Grad'*Step"
              << std::setw(18) << "FD Approx" << std::setw(20) << "abs(Error)" << "\n";

    constexpr Plato::OrdinalType tSuperscriptLowerBound = 1;
    constexpr Plato::OrdinalType tSuperscriptUpperBound = 6;
    Plato::ScalarVector tFiniteDiffResidualAppx("Finite Diff Appx", tTotalNumGlobalStateDofs);
    Plato::ScalarVector tTrialCurrentLocalStates("Trial Current Local States", tTotalNumLocalStateDofs);
    for(Plato::OrdinalType tIndex = tSuperscriptLowerBound; tIndex <= tSuperscriptUpperBound; tIndex++)
    {
        auto tEpsilon = static_cast<Plato::Scalar>(1) / std::pow(static_cast<Plato::Scalar>(10), tIndex);

        // four point finite difference approximation
        Plato::update(1.0, tData.mCurrentLocalState, 0.0, tTrialCurrentLocalStates);
        Plato::update(tEpsilon, tStep, 1.0, tTrialCurrentLocalStates);
        auto tResidualPlus1Eps = aVectorFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                    tTrialCurrentLocalStates, tData.mPrevLocalState,
                                                    tData.mPresssure, tData.mControl, aTimeStep);
        Plato::update(8.0, tResidualPlus1Eps, 0.0, tFiniteDiffResidualAppx);

        Plato::update(1.0, tData.mCurrentLocalState, 0.0, tTrialCurrentLocalStates);
        Plato::update(-tEpsilon, tStep, 1.0, tTrialCurrentLocalStates);
        auto tResidualMinus1Eps = aVectorFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                     tTrialCurrentLocalStates, tData.mPrevLocalState,
                                                     tData.mPresssure, tData.mControl, aTimeStep);
        Plato::update(-8.0, tResidualMinus1Eps, 1.0, tFiniteDiffResidualAppx);

        Plato::update(1.0, tData.mCurrentLocalState, 0.0, tTrialCurrentLocalStates);
        Plato::update(2.0 * tEpsilon, tStep, 1.0, tTrialCurrentLocalStates);
        auto tResidualPlus2Eps = aVectorFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                    tTrialCurrentLocalStates, tData.mPrevLocalState,
                                                    tData.mPresssure, tData.mControl, aTimeStep);
        Plato::update(-1.0, tResidualPlus2Eps, 1.0, tFiniteDiffResidualAppx);

        Plato::update(1.0, tData.mCurrentLocalState, 0.0, tTrialCurrentLocalStates);
        Plato::update(-2.0 * tEpsilon, tStep, 1.0, tTrialCurrentLocalStates);
        auto tResidualMinus2Eps = aVectorFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                     tTrialCurrentLocalStates, tData.mPrevLocalState,
                                                     tData.mPresssure, tData.mControl, aTimeStep);
        Plato::update(1.0, tResidualMinus2Eps, 1.0, tFiniteDiffResidualAppx);

        auto tAlpha = static_cast<Plato::Scalar>(1) / (static_cast<Plato::Scalar>(12) * tEpsilon);
        Plato::scale(tAlpha, tFiniteDiffResidualAppx);
        auto tNormFiniteDiffResidualApprox = Plato::norm(tFiniteDiffResidualAppx);

        Plato::update(-1, tJacCtimesStep, 1., tFiniteDiffResidualAppx);
        auto tNumerator = Plato::norm(tFiniteDiffResidualAppx);
        auto tDenominator = std::numeric_limits<Plato::Scalar>::epsilon() + tNormTrueDerivative;
        auto tRelativeError = tNumerator / tDenominator;

        std::cout << std::right << std::scientific << std::setprecision(8) << std::setw(14) << tEpsilon << std::setw(19)
              << tNormTrueDerivative << std::setw(19) << tNormFiniteDiffResidualApprox << std::setw(19) << tRelativeError << "\n";
    }
}
// function test_partial_global_jacobian_wrt_current_local_states


/******************************************************************************//**
 * \brief Test partial derivative of vector function with path-dependent variables
 *        with respect to the previous local state variables.
 * \param [in] aMesh           mesh database
 * \param [in] aScalarFunction scalar function to evaluate derivative of
 * \param [in] aTimeStep       time step index
**********************************************************************************/
template<typename SimplexPhysicsT, typename PhysicsT>
inline void
test_partial_global_jacobian_wrt_previous_local_states
(std::shared_ptr<Plato::GlobalVectorFunctionInc<PhysicsT>> & aVectorFunc, Omega_h::Mesh & aMesh, Plato::Scalar aTimeStep = 0.0)
{
    // Compute workset Jacobians
    const Plato::OrdinalType tNumCells = aMesh.nelems();
    const Plato::OrdinalType tNumVerts = aMesh.nverts();
    Plato::DiagnosticDataPlasticity<SimplexPhysicsT> tData(tNumVerts, tNumCells);
    auto tJacobianPreviousC = aVectorFunc->gradient_cp(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                      tData.mCurrentLocalState, tData.mPrevLocalState,
                                                      tData.mPresssure, tData.mControl, aTimeStep);

    // Assemble Jacobian and apply descent direction to assembled Jacobian
    auto const tTotalNumLocalStateDofs = tNumCells * SimplexPhysicsT::mNumLocalDofsPerCell;
    Plato::ScalarVector tStep("Step", tTotalNumLocalStateDofs);
    auto tHostStep = Kokkos::create_mirror(tStep);
    Plato::random(0.05, 0.1, tHostStep);
    Kokkos::deep_copy(tStep, tHostStep);
    auto const tTotalNumGlobalStateDofs = tNumVerts * SimplexPhysicsT::mNumDofsPerNode;
    Plato::ScalarVector tJacPrevCtimesStep("JacPrevCtimesVec", tTotalNumGlobalStateDofs);
    Plato::VectorEntryOrdinal<SimplexPhysicsT::mNumSpatialDims, SimplexPhysicsT::mNumDofsPerNode>
            tGlobalVectorEntryOrdinal(&aMesh);
    Plato::assemble_global_vector_jacobian_times_step<SimplexPhysicsT>
            (tGlobalVectorEntryOrdinal, tJacobianPreviousC, tStep, tJacPrevCtimesStep);
    auto tNormTrueDerivative = Plato::norm(tJacPrevCtimesStep);

    std::cout << std::right << std::setw(18) << "\nStep Size" << std::setw(20) << "Grad'*Step"
              << std::setw(18) << "FD Approx" << std::setw(20) << "abs(Error)" << "\n";

    constexpr Plato::OrdinalType tSuperscriptLowerBound = 1;
    constexpr Plato::OrdinalType tSuperscriptUpperBound = 6;
    Plato::ScalarVector tFiniteDiffResidualAppx("Finite Diff Appx", tTotalNumGlobalStateDofs);
    Plato::ScalarVector tTrialPreviousLocalStates("Trial Previous Local States", tTotalNumLocalStateDofs);
    for(Plato::OrdinalType tIndex = tSuperscriptLowerBound; tIndex <= tSuperscriptUpperBound; tIndex++)
    {
        auto tEpsilon = static_cast<Plato::Scalar>(1) / std::pow(static_cast<Plato::Scalar>(10), tIndex);

        // four point finite difference approximation
        Plato::update(1.0, tData.mPrevLocalState, 0.0, tTrialPreviousLocalStates);
        Plato::update(tEpsilon, tStep, 1.0, tTrialPreviousLocalStates);
        auto tResidualPlus1Eps = aVectorFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                    tData.mCurrentLocalState, tTrialPreviousLocalStates,
                                                    tData.mPresssure, tData.mControl, aTimeStep);
        Plato::update(8.0, tResidualPlus1Eps, 0.0, tFiniteDiffResidualAppx);

        Plato::update(1.0, tData.mPrevLocalState, 0.0, tTrialPreviousLocalStates);
        Plato::update(-tEpsilon, tStep, 1.0, tTrialPreviousLocalStates);
        auto tResidualMinus1Eps = aVectorFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                     tData.mCurrentLocalState, tTrialPreviousLocalStates,
                                                     tData.mPresssure, tData.mControl, aTimeStep);
        Plato::update(-8.0, tResidualMinus1Eps, 1.0, tFiniteDiffResidualAppx);

        Plato::update(1.0, tData.mPrevLocalState, 0.0, tTrialPreviousLocalStates);
        Plato::update(2.0 * tEpsilon, tStep, 1.0, tTrialPreviousLocalStates);
        auto tResidualPlus2Eps = aVectorFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                    tData.mCurrentLocalState, tTrialPreviousLocalStates,
                                                    tData.mPresssure, tData.mControl, aTimeStep);
        Plato::update(-1.0, tResidualPlus2Eps, 1.0, tFiniteDiffResidualAppx);

        Plato::update(1.0, tData.mPrevLocalState, 0.0, tTrialPreviousLocalStates);
        Plato::update(-2.0 * tEpsilon, tStep, 1.0, tTrialPreviousLocalStates);
        auto tResidualMinus2Eps = aVectorFunc->value(tData.mCurrentGlobalState, tData.mPrevGlobalState,
                                                     tData.mCurrentLocalState, tTrialPreviousLocalStates,
                                                     tData.mPresssure, tData.mControl, aTimeStep);
        Plato::update(1.0, tResidualMinus2Eps, 1.0, tFiniteDiffResidualAppx);

        auto tAlpha = static_cast<Plato::Scalar>(1) / (static_cast<Plato::Scalar>(12) * tEpsilon);
        Plato::scale(tAlpha, tFiniteDiffResidualAppx);
        auto tNormFiniteDiffResidualApprox = Plato::norm(tFiniteDiffResidualAppx);

        Plato::update(-1, tJacPrevCtimesStep, 1., tFiniteDiffResidualAppx);
        auto tNumerator = Plato::norm(tFiniteDiffResidualAppx);
        auto tDenominator = std::numeric_limits<Plato::Scalar>::epsilon() + tNormTrueDerivative;
        auto tRelativeError = tNumerator / tDenominator;

        std::cout << std::right << std::scientific << std::setprecision(8) << std::setw(14) << tEpsilon << std::setw(19)
              << tNormTrueDerivative << std::setw(19) << tNormFiniteDiffResidualApprox << std::setw(19) << tRelativeError << "\n";
    }
}
// function test_partial_global_jacobian_wrt_previous_local_states













}
// namespace Plato

namespace ElastoPlasticityTest
{


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_FlattenVectorWorkset_Errors)
{
    // CALL FUNCTION - TEST tLocalStateWorset IS EMPTY
    Plato::ScalarVector tAssembledLocalState;
    Plato::ScalarMultiVector tLocalStateWorset;
    constexpr Plato::OrdinalType tNumCells = 1;
    constexpr Plato::OrdinalType tNumLocalDofsPerCell = 14;
    TEST_THROW(Plato::flatten_vector_workset<tNumLocalDofsPerCell>(tNumCells, tLocalStateWorset, tAssembledLocalState), std::runtime_error);

    // CALL FUNCTION - TEST tAssembledLocalState IS EMPTY
    tLocalStateWorset = Plato::ScalarMultiVector("local state WS", tNumCells, tNumLocalDofsPerCell);
    TEST_THROW(Plato::flatten_vector_workset<tNumLocalDofsPerCell>(tNumCells, tLocalStateWorset, tAssembledLocalState), std::runtime_error);

    // CALL FUNCTION - TEST NUMBER OF CELLS IS EMPTY
    constexpr Plato::OrdinalType tEmptyNumCells = 0;
    tAssembledLocalState = Plato::ScalarVector("assembled local state", tNumCells * tNumLocalDofsPerCell);
    TEST_THROW(Plato::flatten_vector_workset<tNumLocalDofsPerCell>(tEmptyNumCells, tLocalStateWorset, tAssembledLocalState), std::runtime_error);
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_FlattenVectorWorkset)
{
    // PREPARE DATA
    constexpr Plato::OrdinalType tNumCells = 3;
    constexpr Plato::OrdinalType tNumLocalDofsPerCell = 14;
    Plato::ScalarMultiVector tLocalStateWorset("local state WS", tNumCells, tNumLocalDofsPerCell);
    auto tHostLocalStateWorset = Kokkos::create_mirror(tLocalStateWorset);

    for (size_t tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for (size_t tDofIndex = 0; tDofIndex < tNumLocalDofsPerCell; tDofIndex++)
        {
            tHostLocalStateWorset(tCellIndex, tDofIndex) = (tNumLocalDofsPerCell * tCellIndex) + (tDofIndex + 1.0);
            //printf("(%d,%d) = %f\n", tCellIndex, tDofIndex, tHostLocalStateWorset(tCellIndex, tDofIndex));
        }
    }
    Kokkos::deep_copy(tLocalStateWorset, tHostLocalStateWorset);

    Plato::ScalarVector tAssembledLocalState("assembled local state", tNumCells * tNumLocalDofsPerCell);

    // CALL FUNCTION
    TEST_NOTHROW(Plato::flatten_vector_workset<tNumLocalDofsPerCell>(tNumCells, tLocalStateWorset, tAssembledLocalState));

    // TEST RESULTS
    constexpr Plato::Scalar tTolerance = 1e-4;
    auto tHostAssembledLocalState = Kokkos::create_mirror(tAssembledLocalState);
    Kokkos::deep_copy(tHostAssembledLocalState, tAssembledLocalState);
    std::vector<std::vector<Plato::Scalar>> tGold =
      {{1,2,3,4,5,6,7,8,9,10,11,12,13,14},
       {15,16,17,18,19,20,21,22,23,24,25,26,27,28},
       {29,30,31,32,33,34,35,36,37,38,39,40,41,42}};
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        const auto tDofOffset = tCellIndex * tNumLocalDofsPerCell;
        for(Plato::OrdinalType tDofIndex = 0; tDofIndex < tNumLocalDofsPerCell; tDofIndex++)
        {
            //printf("(%d,%d) = %f\n", tCellIndex, tDofIndex, tHostAssembledLocalState(tDofOffset + tDofIndex));
            TEST_FLOATING_EQUALITY(tHostAssembledLocalState(tDofOffset + tDofIndex), tGold[tCellIndex][tDofIndex], tTolerance);
        }
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_fill3DView_Error)
{
    // PREPARE DATA
    constexpr Plato::OrdinalType tNumRows = 14;
    constexpr Plato::OrdinalType tNumCols = 14;
    constexpr Plato::OrdinalType tNumCells = 2;

    // CALL FUNCTION - TEST tMatrixWorkSet IS EMPTY
    constexpr Plato::Scalar tAlpha = 2.0;
    Plato::ScalarArray3D tMatrixWorkSet;
    TEST_THROW( (Plato::fill_array_3D<tNumRows, tNumCols>(tNumCells, tAlpha, tMatrixWorkSet)), std::runtime_error );

    // CALL FUNCTION - TEST tNumCells IS ZERO
    Plato::OrdinalType tBadNumCells = 0;
    tMatrixWorkSet = Plato::ScalarArray3D("Matrix A WS", tNumCells, tNumRows, tNumCols);
    TEST_THROW( (Plato::fill_array_3D<tNumRows, tNumCols>(tBadNumCells, tAlpha, tMatrixWorkSet)), std::runtime_error );

    // CALL FUNCTION - TEST tNumCells IS NEGATIVE
    tBadNumCells = -1;
    TEST_THROW( (Plato::fill_array_3D<tNumRows, tNumCols>(tBadNumCells, tAlpha, tMatrixWorkSet)), std::runtime_error );
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_fill3DView)
{
    // PREPARE DATA
    constexpr Plato::OrdinalType tNumRows = 14;
    constexpr Plato::OrdinalType tNumCols = 14;
    constexpr Plato::OrdinalType tNumCells = 2;
    Plato::ScalarArray3D tA("Matrix A WS", tNumCells, tNumRows, tNumCols);

    // CALL FUNCTION
    Plato::Scalar tAlpha = 2.0;
    TEST_NOTHROW( (Plato::fill_array_3D<tNumRows, tNumCols>(tNumCells, tAlpha, tA)) );

    // TEST RESULTS
    constexpr Plato::Scalar tGold = 2.0;
    constexpr Plato::Scalar tTolerance = 1e-4;
    auto tHostA = Kokkos::create_mirror(tA);
    Kokkos::deep_copy(tHostA, tA);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tRowIndex = 0; tRowIndex < tNumRows; tRowIndex++)
        {
            for(Plato::OrdinalType tColIndex = 0; tColIndex < tNumCols; tColIndex++)
            {
                //printf("(%d,%d,%d) = %f\n", aCellOrdinal, tRowIndex, tColIndex, tHostA(tCellIndex, tRowIndex, tColIndex));
                TEST_FLOATING_EQUALITY(tHostA(tCellIndex, tRowIndex, tColIndex), tGold, tTolerance);
            }
        }
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_UpdateMatrixWorkset_Error)
{
    // CALL FUNCTION - INPUT VIEW IS EMPTY
    Plato::ScalarArray3D tB;
    Plato::ScalarArray3D tA;
    constexpr Plato::OrdinalType tNumCells = 2;
    Plato::Scalar tAlpha = 1; Plato::Scalar tBeta = 3;
    TEST_THROW( (Plato::update_array_3D(tNumCells, tAlpha, tA, tBeta, tB)), std::runtime_error );

    // CALL FUNCTION - OUTPUT VIEW IS EMPTY
    Plato::OrdinalType tNumRows = 4;
    Plato::OrdinalType tNumCols = 4;
    tA = Plato::ScalarArray3D("Matrix A WS", tNumCells, tNumRows, tNumCols);
    TEST_THROW( (Plato::update_array_3D(tNumCells, tAlpha, tA, tBeta, tB)), std::runtime_error );

    // CALL FUNCTION - ROW DIM MISTMATCH
    tNumRows = 3;
    Plato::ScalarArray3D tC = Plato::ScalarArray3D("Matrix C WS", tNumCells, tNumRows, tNumCols);
    tNumRows = 4;
    Plato::ScalarArray3D tD = Plato::ScalarArray3D("Matrix D WS", tNumCells, tNumRows, tNumCols);
    TEST_THROW( (Plato::update_array_3D(tNumCells, tAlpha, tC, tBeta, tD)), std::runtime_error );

    // CALL FUNCTION - COLUMN DIM MISTMATCH
    tNumCols = 5;
    Plato::ScalarArray3D tE = Plato::ScalarArray3D("Matrix E WS", tNumCells, tNumRows, tNumCols);
    TEST_THROW( (Plato::update_array_3D(tNumCells, tAlpha, tD, tBeta, tE)), std::runtime_error );

    // CALL FUNCTION - NEGATIVE NUMBER OF CELLS
    tNumRows = 4; tNumCols = 4;
    Plato::OrdinalType tBadNumCells = -1;
    tB = Plato::ScalarArray3D("Matrix B WS", tNumCells, tNumRows, tNumCols);
    TEST_THROW( (Plato::update_array_3D(tBadNumCells, tAlpha, tA, tBeta, tB)), std::runtime_error );

    // CALL FUNCTION - ZERO NUMBER OF CELLS
    tBadNumCells = 0;
    TEST_THROW( (Plato::update_array_3D(tBadNumCells, tAlpha, tA, tBeta, tB)), std::runtime_error );
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_UpdateMatrixWorkset)
{
    // PREPARE DATA
    constexpr Plato::OrdinalType tNumRows = 14;
    constexpr Plato::OrdinalType tNumCols = 14;
    constexpr Plato::OrdinalType tNumCells = 2;
    Plato::ScalarArray3D tA("Matrix A WS", tNumCells, tNumRows, tNumCols);
    Plato::Scalar tAlpha = 2;
    TEST_NOTHROW( (Plato::fill_array_3D<tNumRows, tNumCols>(tNumCells, tAlpha, tA)) );

    tAlpha = 1;
    Plato::ScalarArray3D tB("Matrix A WS", tNumCells, tNumRows, tNumCols);
    TEST_NOTHROW( (Plato::fill_array_3D<tNumRows, tNumCols>(tNumCells, tAlpha, tB)) );

    // CALL FUNCTION
    tAlpha = 2;
    Plato::Scalar tBeta = 3;
    TEST_NOTHROW( (Plato::update_array_3D(tNumCells, tAlpha, tA, tBeta, tB)) );

    // TEST RESULTS
    constexpr Plato::Scalar tGold = 7.0;
    constexpr Plato::Scalar tTolerance = 1e-4;
    auto tHostB = Kokkos::create_mirror(tB);
    Kokkos::deep_copy(tHostB, tB);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tRowIndex = 0; tRowIndex < tNumRows; tRowIndex++)
        {
            for(Plato::OrdinalType tColIndex = 0; tColIndex < tNumCols; tColIndex++)
            {
                //printf("(%d,%d,%d) = %f\n", aCellOrdinal, tRowIndex, tColIndex, tHostB(tCellIndex, tRowIndex, tColIndex));
                TEST_FLOATING_EQUALITY(tHostB(tCellIndex, tRowIndex, tColIndex), tGold, tTolerance);
            }
        }
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_UpdateVectorWorkset_Error)
{
    // CALL FUNCTION - DIM(1) MISMATCH
    Plato::OrdinalType tNumDofsPerCell = 3;
    constexpr Plato::OrdinalType tNumCells = 2;
    Plato::ScalarMultiVector tVecX("vector X WS", tNumCells, tNumDofsPerCell);
    tNumDofsPerCell = 4;
    Plato::ScalarMultiVector tVecY("vector Y WS", tNumCells, tNumDofsPerCell);
    Plato::Scalar tAlpha = 1; Plato::Scalar tBeta = 3;
    TEST_THROW( (Plato::update_array_2D(tAlpha, tVecX, tBeta, tVecY)), std::runtime_error );

    // CALL FUNCTION - DIM(0) MISMATCH
    Plato::OrdinalType tBadNumCells = 4;
    Plato::ScalarMultiVector tVecZ("vector Y WS", tBadNumCells, tNumDofsPerCell);
    TEST_THROW( (Plato::update_array_2D(tAlpha, tVecY, tBeta, tVecZ)), std::runtime_error );
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_UpdateVectorWorkset)
{
    // PREPARE DATA
    constexpr Plato::OrdinalType tNumCells = 2;
    constexpr Plato::OrdinalType tNumLocalDofsPerCell = 6;
    Plato::ScalarMultiVector tVecX("vector X WS", tNumCells, tNumLocalDofsPerCell);
    Plato::ScalarMultiVector tVecY("vector Y WS", tNumCells, tNumLocalDofsPerCell);
    auto tHostVecX = Kokkos::create_mirror(tVecX);
    auto tHostVecY = Kokkos::create_mirror(tVecY);

    for (size_t tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for (size_t tDofIndex = 0; tDofIndex < tNumLocalDofsPerCell; tDofIndex++)
        {
            tHostVecX(tCellIndex, tDofIndex) = (tNumLocalDofsPerCell * tCellIndex) + (tDofIndex + 1.0);
            tHostVecY(tCellIndex, tDofIndex) = (tNumLocalDofsPerCell * tCellIndex) + (tDofIndex + 1.0);
            //printf("X(%d,%d) = %f\n", tCellIndex, tDofIndex, tHostVecX(tCellIndex, tDofIndex));
            //printf("Y(%d,%d) = %f\n", tCellIndex, tDofIndex, tHostVecY(tCellIndex, tDofIndex));
        }
    }
    Kokkos::deep_copy(tVecX, tHostVecX);
    Kokkos::deep_copy(tVecY, tHostVecY);

    // CALL FUNCTION
    Plato::Scalar tAlpha = 1; Plato::Scalar tBeta = 2;
    TEST_NOTHROW( (Plato::update_array_2D(tAlpha, tVecX, tBeta, tVecY)) );

    // TEST OUTPUT
    constexpr Plato::Scalar tTolerance = 1e-4;
    tHostVecY = Kokkos::create_mirror(tVecY);
    Kokkos::deep_copy(tHostVecY, tVecY);
    std::vector<std::vector<Plato::Scalar>> tGold =
      {{3, 6, 9, 12, 15, 18}, {21, 24, 27, 30, 33, 36}};
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tDofIndex = 0; tDofIndex < tNumLocalDofsPerCell; tDofIndex++)
        {
            //printf("(%d,%d) = %f\n", tCellIndex, tDofIndex, tHostVecY(tCellIndex, tDofIndex));
            TEST_FLOATING_EQUALITY(tHostVecY(tCellIndex, tDofIndex), tGold[tCellIndex][tDofIndex], tTolerance);
        }
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_MultiplyMatrixWorkset_Error)
{
    // PREPARE DATA
    Plato::ScalarArray3D tA;
    Plato::ScalarArray3D tB;
    Plato::ScalarArray3D tC;

    // CALL FUNCTION - A IS EMPTY
    constexpr Plato::OrdinalType tNumCells = 2;
    Plato::Scalar tAlpha = 1; Plato::Scalar tBeta = 1;
    TEST_THROW( (Plato::multiply_matrix_workset(tNumCells, tAlpha, tA, tB, tBeta, tC)), std::runtime_error );

    // CALL FUNCTION - B IS EMPTY
    constexpr Plato::OrdinalType tNumRows = 4;
    constexpr Plato::OrdinalType tNumCols = 4;
    tA = Plato::ScalarArray3D("Matrix A", tNumCells, tNumRows, tNumCols);
    TEST_THROW( (Plato::multiply_matrix_workset(tNumCells, tAlpha, tA, tB, tBeta, tC)), std::runtime_error );

    // CALL FUNCTION - C IS EMPTY
    tB = Plato::ScalarArray3D("Matrix B", tNumCells, tNumRows + 1, tNumCols);
    TEST_THROW( (Plato::multiply_matrix_workset(tNumCells, tAlpha, tA, tB, tBeta, tC)), std::runtime_error );

    // CALL FUNCTION - NUM ROWS/COLUMNS MISMATCH IN INPUT MATRICES
    tC = Plato::ScalarArray3D("Matrix C", tNumCells, tNumRows, tNumCols);
    TEST_THROW( (Plato::multiply_matrix_workset(tNumCells, tAlpha, tA, tB, tBeta, tC)), std::runtime_error );

    // CALL FUNCTION - NUM ROWS MISMATCH IN INPUT AND OUTPUT MATRICES
    Plato::ScalarArray3D tD("Matrix D", tNumCells, tNumRows, tNumCols);
    TEST_THROW( (Plato::multiply_matrix_workset(tNumCells, tAlpha, tA, tD, tBeta, tB)), std::runtime_error );

    // CALL FUNCTION - NUM COLUMNS MISMATCH IN INPUT AND OUTPUT MATRICES
    Plato::ScalarArray3D tH("Matrix H", tNumCells, tNumRows, tNumCols + 1);
    TEST_THROW( (Plato::multiply_matrix_workset(tNumCells, tAlpha, tA, tC, tBeta, tH)), std::runtime_error );

    // CALL FUNCTION - NUM CELLS MISMATCH IN A
    Plato::ScalarArray3D tE("Matrix E", tNumCells, tNumRows, tNumCols);
    TEST_THROW( (Plato::multiply_matrix_workset(tNumCells + 1, tAlpha, tA, tD, tBeta, tE)), std::runtime_error );

    // CALL FUNCTION - NUM CELLS MISMATCH IN F
    Plato::ScalarArray3D tF("Matrix F", tNumCells + 1, tNumRows, tNumCols);
    TEST_THROW( (Plato::multiply_matrix_workset(tNumCells, tAlpha, tA, tF, tBeta, tE)), std::runtime_error );

    // CALL FUNCTION - NUM CELLS MISMATCH IN E
    Plato::ScalarArray3D tG("Matrix G", tNumCells + 1, tNumRows, tNumCols);
    TEST_THROW( (Plato::multiply_matrix_workset(tNumCells, tAlpha, tA, tD, tBeta, tG)), std::runtime_error );
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_MultiplyMatrixWorkset_One)
{
    // PREPARE DATA FOR TEST ONE
    constexpr Plato::OrdinalType tNumRows = 4;
    constexpr Plato::OrdinalType tNumCols = 4;
    constexpr Plato::OrdinalType tNumCells = 3;
    Plato::ScalarArray3D tA("Matrix A WS", tNumCells, tNumRows, tNumCols);
    Plato::Scalar tAlpha = 2;
    TEST_NOTHROW( (Plato::fill_array_3D<tNumRows, tNumCols>(tNumCells, tAlpha, tA)) );
    Plato::ScalarArray3D tB("Matrix B WS", tNumCells, tNumRows, tNumCols);
    tAlpha = 1;
    TEST_NOTHROW( (Plato::fill_array_3D<tNumRows, tNumCols>(tNumCells, tAlpha, tB)) );
    Plato::ScalarArray3D tC("Matrix C WS", tNumCells, tNumRows, tNumCols);
    tAlpha = 3;
    TEST_NOTHROW( (Plato::fill_array_3D<tNumRows, tNumCols>(tNumCells, tAlpha, tC)) );

    // CALL FUNCTION
    Plato::Scalar tBeta = 1;
    TEST_NOTHROW( (Plato::multiply_matrix_workset(tNumCells, tAlpha, tA, tB, tBeta, tC)) );

    // TEST RESULTS
    constexpr Plato::Scalar tGold = 27.0;
    constexpr Plato::Scalar tTolerance = 1e-4;
    auto tHostC = Kokkos::create_mirror(tC);
    Kokkos::deep_copy(tHostC, tC);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tRowIndex = 0; tRowIndex < tNumRows; tRowIndex++)
        {
            for(Plato::OrdinalType tColIndex = 0; tColIndex < tNumCols; tColIndex++)
            {
                //printf("(%d,%d,%d) = %f\n", tCellIndex, tRowIndex, tColIndex, tHostC(tCellIndex, tRowIndex, tColIndex));
                TEST_FLOATING_EQUALITY(tHostC(tCellIndex, tRowIndex, tColIndex), tGold, tTolerance);
            }
        }
    }

    // PREPARE DATA FOR TEST TWO
    constexpr Plato::OrdinalType tNumRows2 = 3;
    constexpr Plato::OrdinalType tNumCols2 = 3;
    Plato::ScalarArray3D tD("Matrix D WS", tNumCells, tNumRows2, tNumCols2);
    Plato::ScalarArray3D tE("Matrix E WS", tNumCells, tNumRows2, tNumCols2);
    Plato::ScalarArray3D tF("Matrix F WS", tNumCells, tNumRows2, tNumCols2);
    std::vector<std::vector<Plato::Scalar>> tData = {{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
    auto tHostD = Kokkos::create_mirror(tD);
    auto tHostE = Kokkos::create_mirror(tE);
    auto tHostF = Kokkos::create_mirror(tF);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tRowIndex = 0; tRowIndex < tNumRows2; tRowIndex++)
        {
            for(Plato::OrdinalType tColIndex = 0; tColIndex < tNumCols2; tColIndex++)
            {
                tHostD(tCellIndex, tRowIndex, tColIndex) = tData[tRowIndex][tColIndex];
                tHostE(tCellIndex, tRowIndex, tColIndex) = tData[tRowIndex][tColIndex];
                tHostF(tCellIndex, tRowIndex, tColIndex) = tData[tRowIndex][tColIndex];
            }
        }
    }
    Kokkos::deep_copy(tD, tHostD);
    Kokkos::deep_copy(tE, tHostE);
    Kokkos::deep_copy(tF, tHostF);

    // CALL FUNCTION - NO TRANSPOSE
    tAlpha = 1.5; tBeta = 2.5;
    TEST_NOTHROW( (Plato::multiply_matrix_workset(tNumCells, tAlpha, tD, tE, tBeta, tF)) );

    // 2. TEST RESULTS
    std::vector<std::vector<Plato::Scalar>> tGoldOut = { {47.5, 59, 70.5}, {109, 134, 159}, {170.5, 209, 247.5} };
    tHostF = Kokkos::create_mirror(tF);
    Kokkos::deep_copy(tHostF, tF);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tRowIndex = 0; tRowIndex < tNumRows2; tRowIndex++)
        {
            for(Plato::OrdinalType tColIndex = 0; tColIndex < tNumCols2; tColIndex++)
            {
                //printf("Result(%d,%d,%d) = %f\n", tCellIndex, tRowIndex, tColIndex, tHostF(tCellIndex, tRowIndex, tColIndex));
                TEST_FLOATING_EQUALITY(tHostF(tCellIndex, tRowIndex, tColIndex), tGoldOut[tRowIndex][tColIndex], tTolerance);
            }
        }
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_MultiplyMatrixWorkset_Two)
{
    // PREPARE DATA FOR TEST
    constexpr Plato::OrdinalType tNumCells = 1;
    constexpr Plato::OrdinalType tNumOutCols = 9;
    constexpr Plato::OrdinalType tNumOutRows = 10;
    constexpr Plato::OrdinalType tNumInnrCols = 10;
    Plato::ScalarArray3D tA("Matrix A WS", tNumCells, tNumOutRows, tNumInnrCols);
    auto tHostA = Kokkos::create_mirror(tA);
    tHostA(0,0,0) = 0.999134832918946; tHostA(0,0,1) = -8.65167081054137e-7; tHostA(0,0,2) = -0.665513165892955; tHostA(0,0,3) = 0.332756499757352; tHostA(0,0,4) = 0;
      tHostA(0,0,5) = 0.332756499757352; tHostA(0,0,6) = -8.65167382846366e-7; tHostA(0,0,7) = 4.32583520111433e-7; tHostA(0,0,8) = 0; tHostA(0,0,9) = 4.32583520113168e-7;
    tHostA(0,1,0) = -0.000865167081054158; tHostA(0,1,1) = -8.65167081054158e-7; tHostA(0,1,2) = -0.665513165892955; tHostA(0,1,3) = 0.332756499757352; tHostA(0,1,4) = 0;
      tHostA(0,1,5) = 0.332756499757352; tHostA(0,1,6) = -8.65167382846366e-7; tHostA(0,1,7) = 4.32583520111433e-7; tHostA(0,1,8) = 0; tHostA(0,1,9) = 4.32583520111433e-7;
    tHostA(0,2,0) = -0.000865167081030844; tHostA(0,2,1) = -8.65167081030844e-7; tHostA(0,2,2) = 0.334486834124979; tHostA(0,2,3) = 0.332756499748386; tHostA(0,2,4) = 0;
      tHostA(0,2,5) = 0.332756499748385; tHostA(0,2,6) = -9.31701002265914e-7; tHostA(0,2,7) = 3.66049931096926e-7; tHostA(0,2,8) = 0; tHostA(0,2,9) = 3.66049931099094e-7;
    tHostA(0,3,0) = 0.000432583432413186; tHostA(0,3,1) = 4.32583432413186e-7; tHostA(0,3,2) = 0.332756499781941; tHostA(0,3,3) = 0.767070265244303; tHostA(0,3,4) = 0;
      tHostA(0,3,5) = -0.0998269318370781; tHostA(0,3,6) = 3.66049980498706e-7; tHostA(0,3,7) = -3.69341927428275e-7; tHostA(0,3,8) = 0; tHostA(0,3,9) = -1.96308599308918e-7;
    tHostA(0,4,0) = 0; tHostA(0,4,1) = 0; tHostA(0,4,2) = 0; tHostA(0,4,3) = 0; tHostA(0,4,4) = 0.928703624178876;
      tHostA(0,4,5) = 0; tHostA(0,4,6) = 0; tHostA(0,4,7) = 0; tHostA(0,4,8) = -1.85370035651194e-7; tHostA(0,4,9) = 0;
    tHostA(0,5,0) = 0.000432583432413187; tHostA(0,5,1) = 4.32583432413187e-7; tHostA(0,5,2) = 0.332756499781942; tHostA(0,5,3) = -0.0998269318370783; tHostA(0,5,4) = 0;
      tHostA(0,5,5) = 0.767070265244303; tHostA(0,5,6) = 3.66049980498706e-7; tHostA(0,5,7) = -1.96308599309351e-7; tHostA(0,5,8) = 0; tHostA(0,5,9) = -3.69341927426107e-07;
    tHostA(0,6,0) = -0.576778291445566; tHostA(0,6,1) = -0.000576778291445566; tHostA(0,6,2) = -443.675626551306; tHostA(0,6,3) = 221.837757816214; tHostA(0,6,4) = 0;
      tHostA(0,6,5) = 221.837757816214; tHostA(0,6,6) = 0.999379227378489; tHostA(0,6,7) = 0.000244033383405728; tHostA(0,6,8) = 0; tHostA(0,6,9) = 0.000244033383405728;
    tHostA(0,7,0) = 0.288388970538191; tHostA(0,7,1) = 0.000288388970538191; tHostA(0,7,2) = 221.837678518269; tHostA(0,7,3) = -155.286336004547; tHostA(0,7,4) = 0;
      tHostA(0,7,5) = -66.5512870543163; tHostA(0,7,6) = 0.000244033322428616; tHostA(0,7,7) = 0.999753676091541; tHostA(0,7,8) = 0; tHostA(0,7,9) = -0.000130872405284865;
    tHostA(0,8,0) = 0; tHostA(0,8,1) = 0; tHostA(0,8,2) = 0; tHostA(0,8,3) = 0; tHostA(0,8,4) = -47.5307664670919;
      tHostA(0,8,5) = 0; tHostA(0,8,6) = 0; tHostA(0,8,7) = 0; tHostA(0,8,8) = 0.999876504868183; tHostA(0,8,9) = 0;
    tHostA(0,9,0) = 0.288388970538190; tHostA(0,9,1) = 0.000288388970538190; tHostA(0,9,2) = 221.837678518269; tHostA(0,9,3) = -66.5512870543163; tHostA(0,9,4) = 0;
      tHostA(0,9,5) = -155.286336004547; tHostA(0,9,6) = 0.000244033322428672; tHostA(0,9,7) = -0.000130872405284421; tHostA(0,9,8) = 0; tHostA(0,9,9) = 0.999753676091540;
    Kokkos::deep_copy(tA, tHostA);

    Plato::ScalarArray3D tB("Matrix B WS", tNumCells, tNumInnrCols, tNumOutCols);
    auto tHostB = Kokkos::create_mirror(tB);
    tHostB(0,0,0) = 0; tHostB(0,0,1) = 0; tHostB(0,0,2) = 0; tHostB(0,0,3) = 0; tHostB(0,0,4) = 0;
      tHostB(0,0,5) = 0; tHostB(0,0,6) = 0; tHostB(0,0,7) = 0; tHostB(0,0,8) = 0;
    tHostB(0,1,0) = -769230.8; tHostB(0,1,1) = 0;; tHostB(0,1,2) = 0; tHostB(0,1,3) = 769230.8; tHostB(0,1,4) = 384615.4;
      tHostB(0,1,5) = 0; tHostB(0,1,6) = 0; tHostB(0,1,7) = -384615.4; tHostB(0,1,8) = 0;
    tHostB(0,2,0) = 0; tHostB(0,2,1) = 0; tHostB(0,2,2) = 0; tHostB(0,2,3) = 0; tHostB(0,2,4) = 0;
      tHostB(0,2,5) = 0; tHostB(0,2,6) = 0; tHostB(0,2,7) = 0; tHostB(0,2,8) = 0;
    tHostB(0,3,0) = 0; tHostB(0,3,1) = 0; tHostB(0,3,2) = 0; tHostB(0,3,3) = 0; tHostB(0,3,4) = 0.076779750;
      tHostB(0,3,5) = 0; tHostB(0,3,6) = 0; tHostB(0,3,7) = -0.07677975; tHostB(0,3,8) = 0;
    tHostB(0,4,0) = 0; tHostB(0,4,1) = 0.07677975; tHostB(0,4,2) = 0; tHostB(0,4,3) = 0.07677975; tHostB(0,4,4) = -0.07677975;
      tHostB(0,4,5) = 0; tHostB(0,4,6) = -0.07677975; tHostB(0,4,7) = 0; tHostB(0,4,8) = 0;
    tHostB(0,5,0) = 0; tHostB(0,5,1) = 0; tHostB(0,5,2) = 0; tHostB(0,5,3) = 0; tHostB(0,5,4) = -0.07677975;
      tHostB(0,5,5) = 0; tHostB(0,5,6) = 0; tHostB(0,5,7) = 0.07677975; tHostB(0,5,8) = 0;
    tHostB(0,6,0) = 0; tHostB(0,6,1) = 0; tHostB(0,6,2) = 0; tHostB(0,6,3) = 0; tHostB(0,6,4) = 0;
      tHostB(0,6,5) = 0; tHostB(0,6,6) = 0; tHostB(0,6,7) = 0; tHostB(0,6,8) = 0;
    tHostB(0,7,0) = 0; tHostB(0,7,1) = 0; tHostB(0,7,2) = 0; tHostB(0,7,3) = 0; tHostB(0,7,4) = 51.1865;
      tHostB(0,7,5) = 0; tHostB(0,7,6) = 0; tHostB(0,7,7) = -51.1865; tHostB(0,7,8) = 0;
    tHostB(0,8,0) = 0; tHostB(0,8,1) = 51.1865; tHostB(0,8,2) = 0; tHostB(0,8,3) = 51.1865; tHostB(0,8,4) = -51.1865;
      tHostB(0,8,5) = 0; tHostB(0,8,6) = -51.1865; tHostB(0,8,7) = 0; tHostB(0,8,8) = 0;
    tHostB(0,9,0) = 0; tHostB(0,9,1) = 0; tHostB(0,9,2) = 0; tHostB(0,9,3) = 0; tHostB(0,9,4) = -51.1865;
      tHostB(0,9,5) = 0; tHostB(0,9,6) = 0; tHostB(0,9,7) = 51.1865; tHostB(0,9,8) = 0;
    Kokkos::deep_copy(tB, tHostB);

    // CALL FUNCTION
    constexpr Plato::Scalar tBeta = 0.0;
    constexpr Plato::Scalar tAlpha = 1.0;
    Plato::ScalarArray3D tC("Matrix C WS", tNumCells, tNumOutRows, tNumOutCols);
    TEST_NOTHROW( (Plato::multiply_matrix_workset(tNumCells, tAlpha, tA, tB, tBeta, tC)) );

    // 2. TEST RESULTS
    Plato::ScalarArray3D tGold("Gold", tNumCells, tNumOutRows, tNumOutCols);
    auto tHostGold = Kokkos::create_mirror(tGold);
    tHostGold(0,0,0) = 0.665513165892939; tHostGold(0,0,1) = 0; tHostGold(0,0,2) = 0; tHostGold(0,0,3) = -0.665513165892939; tHostGold(0,0,4) = -0.332756582946470;
      tHostGold(0,0,5) = 0; tHostGold(0,0,6) = 0; tHostGold(0,0,7) = 0.332756582946470; tHostGold(0,0,8) = 0;
    tHostGold(0,1,0) = 0.665513165892955; tHostGold(0,1,1) = 0; tHostGold(0,1,2) = 0; tHostGold(0,1,3) = -0.665513165892955; tHostGold(0,1,4) = -0.332756582946477;
      tHostGold(0,1,5) = 0; tHostGold(0,1,6) = 0; tHostGold(0,1,7) = 0.332756582946477; tHostGold(0,1,8) = 0;
    tHostGold(0,2,0) = 0.665513165875021; tHostGold(0,2,1) = 0; tHostGold(0,2,2) = 0; tHostGold(0,2,3) = -0.665513165875021; tHostGold(0,2,4) = -0.332756582937511;
      tHostGold(0,2,5) = 0; tHostGold(0,2,6) = 0; tHostGold(0,2,7) = 0.332756582937511; tHostGold(0,2,8) = 0;
    tHostGold(0,3,0) = -0.332756499781941;tHostGold(0,3,1) = 0; tHostGold(0,3,2) = 0; tHostGold(0,3,3) = 0.332756499781941; tHostGold(0,3,4) = 0.232929542988130 ;
      tHostGold(0,3,5) = 0; tHostGold(0,3,6) = 0; tHostGold(0,3,7) = -0.23292954298813; tHostGold(0,3,8) = 0;
    tHostGold(0,4,0) = 0; tHostGold(0,4,1) = 0.0712961436452182; tHostGold(0,4,2) = 0; tHostGold(0,4,3) = 0.0712961436452182; tHostGold(0,4,4) = -0.0712961436452182;
      tHostGold(0,4,5) = 0; tHostGold(0,4,6) = -0.0712961436452182; tHostGold(0,4,7) = 0; tHostGold(0,4,8) = 0;
    tHostGold(0,5,0) = -0.332756499781942; tHostGold(0,5,1) = 0; tHostGold(0,5,2) = 0; tHostGold(0,5,3) = 0.332756499781942; tHostGold(0,5,4) = 0.0998269567938113;
      tHostGold(0,5,5) = 0; tHostGold(0,5,6) = 0; tHostGold(0,5,7) = -0.0998269567938113; tHostGold(0,5,8) = 0;
    tHostGold(0,6,0) = 443.675626551306; tHostGold(0,6,1) = 0; tHostGold(0,6,2) = 0; tHostGold(0,6,3) = -443.675626551306; tHostGold(0,6,4) = -221.837813275653;
      tHostGold(0,6,5) = 0; tHostGold(0,6,6) = 0; tHostGold(0,6,7) = 221.837813275653; tHostGold(0,6,8) = 0;
    tHostGold(0,7,0) = -221.837678518269; tHostGold(0,7,1) = 0; tHostGold(0,7,2) = 0; tHostGold(0,7,3) = 221.837678518269; tHostGold(0,7,4) = 155.286374826131;
      tHostGold(0,7,5) = 0; tHostGold(0,7,6) = 0; tHostGold(0,7,7) = -155.286374826131; tHostGold(0,7,8) = 0;
    tHostGold(0,8,0) = 0; tHostGold(0,8,1) = 47.5307783497835; tHostGold(0,8,2) = 0; tHostGold(0,8,3) = 47.5307783497835; tHostGold(0,8,4) = -47.5307783497835;
      tHostGold(0,8,5) = 0; tHostGold(0,8,6) = -47.5307783497835; tHostGold(0,8,7) = 0; tHostGold(0,8,8) = 0;
    tHostGold(0,9,0) = -221.837678518269; tHostGold(0,9,1) = 0; tHostGold(0,9,2) = 0; tHostGold(0,9,3) = 221.837678518269; tHostGold(0,9,4) = 66.5513036921381;
      tHostGold(0,9,5) = 0; tHostGold(0,9,6) = 0; tHostGold(0,9,7) = -66.5513036921381; tHostGold(0,9,8) = 0;

    auto tHostC = Kokkos::create_mirror(tC);
    Kokkos::deep_copy(tHostC, tC);
    constexpr Plato::Scalar tTolerance = 1e-4;
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tC.extent(0); tCellIndex++)
    {
        for(Plato::OrdinalType tRowIndex = 0; tRowIndex < tC.extent(1); tRowIndex++)
        {
            for(Plato::OrdinalType tColIndex = 0; tColIndex < tC.extent(2); tColIndex++)
            {
                //printf("Result(%d,%d,%d) = %f\n", tCellIndex + 1, tRowIndex + 1, tColIndex+ 1, tHostC(tCellIndex, tRowIndex, tColIndex));
                TEST_FLOATING_EQUALITY(tHostGold(tCellIndex, tRowIndex, tColIndex), tHostC(tCellIndex, tRowIndex, tColIndex), tTolerance);
            }
        }
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_MatrixTimesVectorWorkset_Error)
{
    // PREPARE DATA
    Plato::ScalarArray3D tA;
    Plato::ScalarMultiVector tX;
    Plato::ScalarMultiVector tY;

    // CALL FUNCTION - MATRIX A IS EMPTY
    constexpr Plato::OrdinalType tNumCells = 3;
    Plato::Scalar tAlpha = 1.5; Plato::Scalar tBeta = 2.5;
    TEST_THROW( (Plato::matrix_times_vector_workset("N", tAlpha, tA, tX, tBeta, tY)), std::runtime_error );

    // CALL FUNCTION - VECTOR X IS EMPTY
    constexpr Plato::OrdinalType tNumCols = 2;
    constexpr Plato::OrdinalType tNumRows = 3;
    tA = Plato::ScalarArray3D("A Matrix WS", tNumCells, tNumRows, tNumCols);
    TEST_THROW( (Plato::matrix_times_vector_workset("N", tAlpha, tA, tX, tBeta, tY)), std::runtime_error );

    // CALL FUNCTION - VECTOR Y IS EMPTY
    tX = Plato::ScalarMultiVector("X Vector WS", tNumCells, tNumCols);
    TEST_THROW( (Plato::matrix_times_vector_workset("N", tAlpha, tA, tX, tBeta, tY)), std::runtime_error );

    // CALL FUNCTION - NUM CELL MISMATCH IN INPUT MATRIX
    tY = Plato::ScalarMultiVector("Y Vector WS", tNumCells + 1, tNumRows);
    TEST_THROW( (Plato::matrix_times_vector_workset("N", tAlpha, tA, tX, tBeta, tY)), std::runtime_error );

    // CALL FUNCTION - NUM CELL MISMATCH IN INPUT VECTOR X
    Plato::ScalarMultiVector tVecX("X Vector WS", tNumCells + 1, tNumRows);
    TEST_THROW( (Plato::matrix_times_vector_workset("N", tAlpha, tA, tVecX, tBeta, tY)), std::runtime_error );
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_MatrixTimesVectorWorkset)
{
    // 1. PREPARE DATA FOR TEST ONE
    constexpr Plato::OrdinalType tNumRows = 3;
    constexpr Plato::OrdinalType tNumCols = 2;
    constexpr Plato::OrdinalType tNumCells = 3;

    // 1.1 PREPARE MATRIX DATA
    Plato::ScalarArray3D tA("A Matrix WS", tNumCells, tNumRows, tNumCols);
    std::vector<std::vector<Plato::Scalar>> tMatrixData = {{1, 2}, {3, 4}, {5, 6}};
    auto tHostA = Kokkos::create_mirror(tA);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tRowIndex = 0; tRowIndex < tNumRows; tRowIndex++)
        {
            for(Plato::OrdinalType tColIndex = 0; tColIndex < tNumCols; tColIndex++)
            {
                tHostA(tCellIndex, tRowIndex, tColIndex) =
                        static_cast<Plato::Scalar>(tCellIndex + 1) * tMatrixData[tRowIndex][tColIndex];
            }
        }
    }
    Kokkos::deep_copy(tA, tHostA);

    // 1.2 PREPARE X VECTOR DATA
    Plato::ScalarMultiVector tX("X Vector WS", tNumCells, tNumCols);
    std::vector<Plato::Scalar> tXdata = {1, 2};
    auto tHostX = Kokkos::create_mirror(tX);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tColIndex = 0; tColIndex < tNumCols; tColIndex++)
        {
            tHostX(tCellIndex, tColIndex) = static_cast<Plato::Scalar>(tCellIndex + 1) * tXdata[tColIndex];
        }
    }
    Kokkos::deep_copy(tX, tHostX);

    // 1.3 PREPARE Y VECTOR DATA
    Plato::ScalarMultiVector tY("Y Vector WS", tNumCells, tNumRows);
    std::vector<Plato::Scalar> tYdata = {1, 2, 3};
    auto tHostY = Kokkos::create_mirror(tY);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tRowIndex = 0; tRowIndex < tNumRows; tRowIndex++)
        {
            tHostY(tCellIndex, tRowIndex) = static_cast<Plato::Scalar>(tCellIndex + 1) * tYdata[tRowIndex];
        }
    }
    Kokkos::deep_copy(tY, tHostY);

    // 1.4 CALL FUNCTION - NO TRANSPOSE
    Plato::Scalar tAlpha = 1.5; Plato::Scalar tBeta = 2.5;
    TEST_NOTHROW( (Plato::matrix_times_vector_workset("N", tAlpha, tA, tX, tBeta, tY)) );

    // 1.5 TEST RESULTS
    tHostY = Kokkos::create_mirror(tY);
    Kokkos::deep_copy(tHostY, tY);
    constexpr Plato::Scalar tTolerance = 1e-4;
    std::vector<std::vector<Plato::Scalar>> tGoldOne = { {10, 21.5, 33}, {35, 76, 117}, {75, 163.5, 252} };
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tRowIndex = 0; tRowIndex < tNumRows; tRowIndex++)
        {
            //printf("(%d,%d) = %f\n", tCellIndex, tRowIndex, tHostY(tCellIndex, tRowIndex));
            TEST_FLOATING_EQUALITY(tHostY(tCellIndex, tRowIndex), tGoldOne[tCellIndex][tRowIndex], tTolerance);
        }
    }

    // 2.1 PREPARE DATA FOR X VECTOR - TEST TWO
    Plato::ScalarMultiVector tVecX("X Vector WS", tNumCells, tNumRows);
    std::vector<Plato::Scalar> tVecXdata = {1, 2, 3};
    auto tHostVecX = Kokkos::create_mirror(tVecX);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tRowIndex = 0; tRowIndex < tNumRows; tRowIndex++)
        {
            tHostVecX(tCellIndex, tRowIndex) = static_cast<Plato::Scalar>(tCellIndex + 1) * tVecXdata[tRowIndex];
        }
    }
    Kokkos::deep_copy(tVecX, tHostVecX);

    // 2.2 PREPARE Y VECTOR DATA
    Plato::ScalarMultiVector tVecY("Y Vector WS", tNumCells, tNumCols);
    std::vector<Plato::Scalar> tVecYdata = {1, 2};
    auto tHostVecY = Kokkos::create_mirror(tVecY);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tColIndex = 0; tColIndex < tNumCols; tColIndex++)
        {
            tHostVecY(tCellIndex, tColIndex) = static_cast<Plato::Scalar>(tCellIndex + 1) * tVecYdata[tColIndex];
        }
    }
    Kokkos::deep_copy(tVecY, tHostVecY);

    // 2.2 CALL FUNCTION - TRANSPOSE
    TEST_NOTHROW( (Plato::matrix_times_vector_workset("T", tAlpha, tA, tVecX, tBeta, tVecY)) );

    // 2.3 TEST RESULTS
    tHostVecY = Kokkos::create_mirror(tVecY);
    Kokkos::deep_copy(tHostVecY, tVecY);
    std::vector<std::vector<Plato::Scalar>> tGoldTwo = { {35.5, 47}, {137, 178}, {304.5, 393} };
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tColIndex = 0; tColIndex < tNumCols; tColIndex++)
        {
            //printf("(%d,%d) = %f\n", tCellIndex, tColIndex, tHostVecY(tCellIndex, tColIndex));
            TEST_FLOATING_EQUALITY(tHostVecY(tCellIndex, tColIndex), tGoldTwo[tCellIndex][tColIndex], tTolerance);
        }
    }

    // 3. TEST VALIDITY OF TRANSPOSE
    TEST_THROW( (Plato::matrix_times_vector_workset("C", tAlpha, tA, tVecX, tBeta, tVecY)), std::runtime_error );
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_IdentityWorkset)
{
    // PREPARE DATA FOR TEST
    constexpr Plato::OrdinalType tNumRows = 4;
    constexpr Plato::OrdinalType tNumCols = 4;
    constexpr Plato::OrdinalType tNumCells = 3;
    Plato::ScalarArray3D tIdentity("tIdentity WS", tNumCells, tNumRows, tNumCols);

    // CALL FUNCTION
    Plato::identity_workset<tNumRows, tNumCols>(tNumCells, tIdentity);

    // TEST RESULTS
    constexpr Plato::Scalar tTolerance = 1e-4;
    std::vector<std::vector<Plato::Scalar>> tGold = { {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1} };
    auto tHostIdentity = Kokkos::create_mirror(tIdentity);
    Kokkos::deep_copy(tHostIdentity, tIdentity);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tRowIndex = 0; tRowIndex < tNumRows; tRowIndex++)
        {
            for(Plato::OrdinalType tColIndex = 0; tColIndex < tNumCols; tColIndex++)
            {
                TEST_FLOATING_EQUALITY(tHostIdentity(tCellIndex, tRowIndex, tColIndex), tGold[tRowIndex][tColIndex], tTolerance);
            }
        }
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_InverseMatrixWorkset)
{
    // PREPARE DATA FOR TEST
    constexpr Plato::OrdinalType tNumRows = 2;
    constexpr Plato::OrdinalType tNumCols = 2;
    constexpr Plato::OrdinalType tNumCells = 3; // Number of matrices to invert
    Plato::ScalarArray3D tMatrix("Matrix A", tNumCells, 2, 2);
    auto tHostMatrix = Kokkos::create_mirror(tMatrix);
    for (Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; ++tCellIndex)
    {
        const Plato::Scalar tScaleFactor = 1.0 / (1.0 + tCellIndex);
        tHostMatrix(tCellIndex, 0, 0) = -2.0 * tScaleFactor;
        tHostMatrix(tCellIndex, 1, 0) = 1.0 * tScaleFactor;
        tHostMatrix(tCellIndex, 0, 1) = 1.5 * tScaleFactor;
        tHostMatrix(tCellIndex, 1, 1) = -0.5 * tScaleFactor;
    }
    Kokkos::deep_copy(tMatrix, tHostMatrix);

    // CALL FUNCTION
    Plato::ScalarArray3D tAInverse("A Inverse", tNumCells, 2, 2);
    Plato::inverse_matrix_workset<tNumRows, tNumCols>(tNumCells, tMatrix, tAInverse);

    constexpr Plato::Scalar tTolerance = 1e-6;
    std::vector<std::vector<Plato::Scalar> > tGoldMatrixInverse = { { 1.0, 3.0 }, { 2.0, 4.0 } };
    auto tHostAInverse = Kokkos::create_mirror(tAInverse);
    Kokkos::deep_copy(tHostAInverse, tAInverse);
    for (Plato::OrdinalType tMatrixIndex = 0; tMatrixIndex < tNumCells; tMatrixIndex++)
    {
        for (Plato::OrdinalType tRowIndex = 0; tRowIndex < tNumRows; tRowIndex++)
        {
            for (Plato::OrdinalType tColIndex = 0; tColIndex < tNumCols; tColIndex++)
            {
                //printf("Matrix %d Inverse (%d,%d) = %f\n", n, i, j, tHostAInverse(n, i, j));
                const Plato::Scalar tScaleFactor = (1.0 + tMatrixIndex);
                TEST_FLOATING_EQUALITY(tHostAInverse(tMatrixIndex, tRowIndex, tColIndex), tScaleFactor * tGoldMatrixInverse[tRowIndex][tColIndex], tTolerance);
            }
        }
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_ApplyPenalty)
{
    // PREPARE DATA FOR TEST
    constexpr Plato::OrdinalType tNumRows = 3;
    constexpr Plato::OrdinalType tNumCols = 3;
    Plato::ScalarMultiVector tA("A: 2-D View", tNumRows, tNumCols);
    std::vector<std::vector<Plato::Scalar>> tData = { {10, 20, 30}, {35, 76, 117}, {75, 163, 252} };

    auto tHostA = Kokkos::create_mirror(tA);
    for (Plato::OrdinalType tRowIndex = 0; tRowIndex < tNumRows; ++tRowIndex)
    {
        for (Plato::OrdinalType tColIndex = 0; tColIndex < tNumCols; ++tColIndex)
        {
            tHostA(tRowIndex, tColIndex) = tData[tRowIndex][tColIndex];
        }
    }
    Kokkos::deep_copy(tA, tHostA);

    // CALL FUNCTION
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tNumRows), LAMBDA_EXPRESSION(const Plato::OrdinalType & aRowIndex)
    {
        Plato::apply_penalty<tNumCols>(aRowIndex, 0.5, tA);
    }, "identity workset");

    // TEST RESULTS
    constexpr Plato::Scalar tTolerance = 1e-6;
    tHostA = Kokkos::create_mirror(tA);
    Kokkos::deep_copy(tHostA, tA);
    std::vector<std::vector<Plato::Scalar>> tGold = { {5, 10, 15}, {17.5, 38, 58.5}, {37.5, 81.5, 126} };
    for (Plato::OrdinalType tRowIndex = 0; tRowIndex < tNumRows; tRowIndex++)
    {
        for (Plato::OrdinalType tColIndex = 0; tColIndex < tNumCols; tColIndex++)
        {
            TEST_FLOATING_EQUALITY(tHostA(tRowIndex, tColIndex), tGold[tRowIndex][tColIndex], tTolerance);
        }
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_ComputeShearAndBulkModulus)
{
    const Plato::Scalar tPoisson = 0.3;
    const Plato::Scalar tElasticModulus = 1;
    auto tBulk = Plato::compute_bulk_modulus(tElasticModulus, tPoisson);
    constexpr Plato::Scalar tTolerance = 1e-6;
    TEST_FLOATING_EQUALITY(tBulk, 0.833333333333333, tTolerance);
    auto tShear = Plato::compute_shear_modulus(tElasticModulus, tPoisson);
    TEST_FLOATING_EQUALITY(tShear, 0.384615384615385, tTolerance);
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_StrainDivergence3D)
{
    // PREPARE DATA FOR TEST
    constexpr Plato::OrdinalType tNumCells = 3;
    constexpr Plato::OrdinalType tSpaceDim = 3;
    constexpr Plato::OrdinalType tNumVoigtTerms = 6;
    Plato::ScalarVector tOutput("strain tensor divergence", tNumCells);
    Plato::ScalarMultiVector tStrainTensor("strain tensor", tNumCells, tNumVoigtTerms);
    auto tHostStrainTensor = Kokkos::create_mirror(tStrainTensor);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        tHostStrainTensor(tCellIndex, 0) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.1;
        tHostStrainTensor(tCellIndex, 1) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.2;
        tHostStrainTensor(tCellIndex, 2) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.3;
        tHostStrainTensor(tCellIndex, 3) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.4;
        tHostStrainTensor(tCellIndex, 4) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.5;
        tHostStrainTensor(tCellIndex, 5) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.6;
    }
    Kokkos::deep_copy(tStrainTensor, tHostStrainTensor);

    // CALL FUNCTION
    Plato::StrainDivergence<tSpaceDim> tComputeStrainDivergence;
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tNumCells), LAMBDA_EXPRESSION(const Plato::OrdinalType & aCellIndex)
    {
        tComputeStrainDivergence(aCellIndex, tStrainTensor, tOutput);
    }, "test strain divergence functor");

    // TEST RESULTS
    constexpr Plato::Scalar tTolerance = 1e-6;
    std::vector<Plato::Scalar> tGold = {0.6, 1.2, 1.8};
    auto tHostOutput = Kokkos::create_mirror(tOutput);
    Kokkos::deep_copy(tHostOutput, tOutput);
    for (Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        TEST_FLOATING_EQUALITY(tHostOutput(tCellIndex), tGold[tCellIndex], tTolerance);
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_StrainDivergence2D)
{
    // PREPARE DATA FOR TEST
    constexpr Plato::OrdinalType tNumCells = 3;
    constexpr Plato::OrdinalType tSpaceDim = 2;
    constexpr Plato::OrdinalType tNumVoigtTerms = 3;
    Plato::ScalarVector tOutput("strain tensor divergence", tNumCells);
    Plato::ScalarMultiVector tStrainTensor("strain tensor", tNumCells, tNumVoigtTerms);
    auto tHostStrainTensor = Kokkos::create_mirror(tStrainTensor);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        tHostStrainTensor(tCellIndex, 0) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.1;
        tHostStrainTensor(tCellIndex, 1) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.2;
        tHostStrainTensor(tCellIndex, 2) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.3;
    }
    Kokkos::deep_copy(tStrainTensor, tHostStrainTensor);

    // CALL FUNCTION
    Plato::StrainDivergence<tSpaceDim> tComputeStrainDivergence;
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tNumCells), LAMBDA_EXPRESSION(const Plato::OrdinalType & aCellIndex)
    {
        tComputeStrainDivergence(aCellIndex, tStrainTensor, tOutput);
    }, "test strain divergence functor");

    // TEST RESULTS
    constexpr Plato::Scalar tTolerance = 1e-6;
    std::vector<Plato::Scalar> tGold = {0.3, 0.6, 0.9};
    auto tHostOutput = Kokkos::create_mirror(tOutput);
    Kokkos::deep_copy(tHostOutput, tOutput);
    for (Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        TEST_FLOATING_EQUALITY(tHostOutput(tCellIndex), tGold[tCellIndex], tTolerance);
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_StrainDivergence1D)
{
    // PREPARE DATA FOR TEST
    constexpr Plato::OrdinalType tNumCells = 3;
    constexpr Plato::OrdinalType tSpaceDim = 1;
    constexpr Plato::OrdinalType tNumVoigtTerms = 1;
    Plato::ScalarVector tOutput("strain tensor divergence", tNumCells);
    Plato::ScalarMultiVector tStrainTensor("strain tensor", tNumCells, tNumVoigtTerms);
    auto tHostStrainTensor = Kokkos::create_mirror(tStrainTensor);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        tHostStrainTensor(tCellIndex, 0) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.1;
    }
    Kokkos::deep_copy(tStrainTensor, tHostStrainTensor);

    // CALL FUNCTION
    Plato::StrainDivergence<tSpaceDim> tComputeStrainDivergence;
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tNumCells), LAMBDA_EXPRESSION(const Plato::OrdinalType & aCellIndex)
    {
        tComputeStrainDivergence(aCellIndex, tStrainTensor, tOutput);
    }, "test strain divergence functor");

    // TEST RESULTS
    constexpr Plato::Scalar tTolerance = 1e-6;
    std::vector<Plato::Scalar> tGold = {0.1, 0.2, 0.3};
    auto tHostOutput = Kokkos::create_mirror(tOutput);
    Kokkos::deep_copy(tHostOutput, tOutput);
    for (Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        TEST_FLOATING_EQUALITY(tHostOutput(tCellIndex), tGold[tCellIndex], tTolerance);
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_ComputeStabilization3D)
{
    // PREPARE DATA FOR TEST
    constexpr Plato::OrdinalType tNumCells = 3;
    constexpr Plato::OrdinalType tSpaceDim = 3;

    Plato::ScalarVector tCellVolume("volume", tNumCells);
    auto tHostCellVolume = Kokkos::create_mirror(tCellVolume);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        tHostCellVolume(tCellIndex, 0) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.1;
    }
    Kokkos::deep_copy(tCellVolume, tHostCellVolume);

    Plato::ScalarMultiVector tPressureGrad("pressure gradient", tNumCells, tSpaceDim);
    auto tHostPressureGrad = Kokkos::create_mirror(tPressureGrad);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        tHostPressureGrad(tCellIndex, 0) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.1;
        tHostPressureGrad(tCellIndex, 1) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.2;
        tHostPressureGrad(tCellIndex, 2) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.3;
    }
    Kokkos::deep_copy(tPressureGrad, tHostPressureGrad);

    Plato::ScalarMultiVector tProjectedPressureGrad("projected pressure gradient - gauss pt", tNumCells, tSpaceDim);
    auto tHostProjectedPressureGrad = Kokkos::create_mirror(tProjectedPressureGrad);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        tHostProjectedPressureGrad(tCellIndex, 0) = static_cast<Plato::Scalar>(1+tCellIndex) * 1;
        tHostProjectedPressureGrad(tCellIndex, 1) = static_cast<Plato::Scalar>(1+tCellIndex) * 2;
        tHostProjectedPressureGrad(tCellIndex, 2) = static_cast<Plato::Scalar>(1+tCellIndex) * 3;
    }
    Kokkos::deep_copy(tProjectedPressureGrad, tHostProjectedPressureGrad);

    // CALL FUNCTION
    constexpr Plato::Scalar tScaling = 0.5;
    constexpr Plato::Scalar tShearModulus = 2;
    Plato::ScalarMultiVector tStabilization("cell stabilization", tNumCells, tSpaceDim);
    Plato::ComputeStabilization<tSpaceDim> tComputeStabilization(tScaling, tShearModulus);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tNumCells), LAMBDA_EXPRESSION(const Plato::OrdinalType & aCellIndex)
    {
        tComputeStabilization(aCellIndex, tCellVolume, tPressureGrad, tProjectedPressureGrad, tStabilization);
    }, "test compute stabilization functor");

    // TEST RESULTS
    constexpr Plato::Scalar tTolerance = 1e-6;
    auto tHostStabilization = Kokkos::create_mirror(tStabilization);
    Kokkos::deep_copy(tHostStabilization, tStabilization);
    std::vector<std::vector<Plato::Scalar>> tGold = {{-0.0255839119441290, -0.0511678238882572, -0.0767517358323859},
                                                     {-0.0812238574671431, -0.1624477149342860, -0.2436715724014290},
                                                     {-0.1596500440960990, -0.3193000881921980, -0.4789501322882970}};
    for (Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for (Plato::OrdinalType tDimIndex = 0; tDimIndex < tSpaceDim; tDimIndex++)
        {
            TEST_FLOATING_EQUALITY(tHostStabilization(tCellIndex, tDimIndex), tGold[tCellIndex][tDimIndex], tTolerance);
        }
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_ComputeStabilization2D)
{
    // PREPARE DATA FOR TEST
    constexpr Plato::OrdinalType tNumCells = 3;
    constexpr Plato::OrdinalType tSpaceDim = 2;

    Plato::ScalarVector tCellVolume("volume", tNumCells);
    auto tHostCellVolume = Kokkos::create_mirror(tCellVolume);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        tHostCellVolume(tCellIndex, 0) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.1;
    }
    Kokkos::deep_copy(tCellVolume, tHostCellVolume);

    Plato::ScalarMultiVector tPressureGrad("pressure gradient", tNumCells, tSpaceDim);
    auto tHostPressureGrad = Kokkos::create_mirror(tPressureGrad);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        tHostPressureGrad(tCellIndex, 0) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.1;
        tHostPressureGrad(tCellIndex, 1) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.2;
    }
    Kokkos::deep_copy(tPressureGrad, tHostPressureGrad);

    Plato::ScalarMultiVector tProjectedPressureGrad("projected pressure gradient - gauss pt", tNumCells, tSpaceDim);
    auto tHostProjectedPressureGrad = Kokkos::create_mirror(tProjectedPressureGrad);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        tHostProjectedPressureGrad(tCellIndex, 0) = static_cast<Plato::Scalar>(1+tCellIndex) * 1;
        tHostProjectedPressureGrad(tCellIndex, 1) = static_cast<Plato::Scalar>(1+tCellIndex) * 2;
    }
    Kokkos::deep_copy(tProjectedPressureGrad, tHostProjectedPressureGrad);

    // CALL FUNCTION
    constexpr Plato::Scalar tScaling = 0.5;
    constexpr Plato::Scalar tShearModulus = 2;
    Plato::ScalarMultiVector tStabilization("cell stabilization", tNumCells, tSpaceDim);
    Plato::ComputeStabilization<tSpaceDim> tComputeStabilization(tScaling, tShearModulus);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tNumCells), LAMBDA_EXPRESSION(const Plato::OrdinalType & aCellIndex)
    {
        tComputeStabilization(aCellIndex, tCellVolume, tPressureGrad, tProjectedPressureGrad, tStabilization);
    }, "test compute stabilization functor");

    // TEST RESULTS
    constexpr Plato::Scalar tTolerance = 1e-6;
    auto tHostStabilization = Kokkos::create_mirror(tStabilization);
    Kokkos::deep_copy(tHostStabilization, tStabilization);
    std::vector<std::vector<Plato::Scalar>> tGold = {{-0.0255839119441290, -0.0511678238882572},
                                                     {-0.0812238574671431, -0.1624477149342860},
                                                     {-0.1596500440960990, -0.3193000881921980}};
    for (Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for (Plato::OrdinalType tDimIndex = 0; tDimIndex < tSpaceDim; tDimIndex++)
        {
            TEST_FLOATING_EQUALITY(tHostStabilization(tCellIndex, tDimIndex), tGold[tCellIndex][tDimIndex], tTolerance);
        }
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_ComputeStabilization1D)
{
    // PREPARE DATA FOR TEST
    constexpr Plato::OrdinalType tNumCells = 3;
    constexpr Plato::OrdinalType tSpaceDim = 1;

    Plato::ScalarVector tCellVolume("volume", tNumCells);
    auto tHostCellVolume = Kokkos::create_mirror(tCellVolume);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        tHostCellVolume(tCellIndex, 0) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.1;
    }
    Kokkos::deep_copy(tCellVolume, tHostCellVolume);

    Plato::ScalarMultiVector tPressureGrad("pressure gradient", tNumCells, tSpaceDim);
    auto tHostPressureGrad = Kokkos::create_mirror(tPressureGrad);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        tHostPressureGrad(tCellIndex, 0) = static_cast<Plato::Scalar>(1+tCellIndex) * 0.1;
    }
    Kokkos::deep_copy(tPressureGrad, tHostPressureGrad);

    Plato::ScalarMultiVector tProjectedPressureGrad("projected pressure gradient - gauss pt", tNumCells, tSpaceDim);
    auto tHostProjectedPressureGrad = Kokkos::create_mirror(tProjectedPressureGrad);
    for(Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        tHostProjectedPressureGrad(tCellIndex, 0) = static_cast<Plato::Scalar>(1+tCellIndex) * 1;
    }
    Kokkos::deep_copy(tProjectedPressureGrad, tHostProjectedPressureGrad);

    // CALL FUNCTION
    constexpr Plato::Scalar tScaling = 0.5;
    constexpr Plato::Scalar tShearModulus = 2;
    Plato::ScalarMultiVector tStabilization("cell stabilization", tNumCells, tSpaceDim);
    Plato::ComputeStabilization<tSpaceDim> tComputeStabilization(tScaling, tShearModulus);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tNumCells), LAMBDA_EXPRESSION(const Plato::OrdinalType & aCellIndex)
    {
        tComputeStabilization(aCellIndex, tCellVolume, tPressureGrad, tProjectedPressureGrad, tStabilization);
    }, "test compute stabilization functor");

    // TEST RESULTS
    constexpr Plato::Scalar tTolerance = 1e-6;
    auto tHostStabilization = Kokkos::create_mirror(tStabilization);
    Kokkos::deep_copy(tHostStabilization, tStabilization);
    std::vector<std::vector<Plato::Scalar>> tGold = {{-0.0255839119441290},
                                                     {-0.0812238574671431},
                                                     {-0.1596500440960990}};
    for (Plato::OrdinalType tCellIndex = 0; tCellIndex < tNumCells; tCellIndex++)
    {
        for (Plato::OrdinalType tDimIndex = 0; tDimIndex < tSpaceDim; tDimIndex++)
        {
            TEST_FLOATING_EQUALITY(tHostStabilization(tCellIndex, tDimIndex), tGold[tCellIndex][tDimIndex], tTolerance);
        }
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_Residual3D_Elastic)
{
    // 1. PREPARE PROBLEM INPUS FOR TEST
    Plato::DataMap tDataMap;
    Omega_h::MeshSets tMeshSets;
    constexpr Plato::OrdinalType tSpaceDim = 3;
    constexpr Plato::OrdinalType tMeshWidth = 1;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);

    Teuchos::RCP<Teuchos::ParameterList> tElastoPlasticityInputs =
        Teuchos::getParametersFromXmlString(
        "<ParameterList name='Plato Problem'>                                   \n"
        "  <ParameterList name='Material Model'>                                \n"
        "    <ParameterList name='Isotropic Linear Elastic'>                    \n"
        "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>    \n"
        "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>  \n"
        "    </ParameterList>                                                   \n"
        "  </ParameterList>                                                     \n"
        "  <ParameterList name='Infinite Strain Plasticity'>                    \n"
        "    <ParameterList name='Penalty Function'>                            \n"
        "      <Parameter name='Type' type='string' value='SIMP'/>              \n"
        "      <Parameter name='Exponent' type='double' value='3.0'/>           \n"
        "      <Parameter name='Minimum Value' type='double' value='1.0e-6'/>   \n"
        "    </ParameterList>                                                   \n"
        "  </ParameterList>                                                     \n"
        "</ParameterList>                                                       \n"
      );

    // 2. PREPARE FUNCTION INPUTS FOR TEST
    const Plato::OrdinalType tNumNodes = tMesh->nverts();
    const Plato::OrdinalType tNumCells = tMesh->nelems();
    using PhysicsT = Plato::SimplexPlasticity<tSpaceDim>;
    using EvalType = typename Plato::Evaluation<PhysicsT>::Residual;
    Plato::WorksetBase<PhysicsT> tWorksetBase(*tMesh);

    // 2.1 SET CONFIGURATION
    Plato::ScalarArray3DT<EvalType::ConfigScalarType> tConfiguration("configuration", tNumCells, PhysicsT::mNumNodesPerCell, tSpaceDim);
    tWorksetBase.worksetConfig(tConfiguration);

    // 2.2 SET DESIGN VARIABLES
    Plato::ScalarMultiVectorT<EvalType::ControlScalarType> tDesignVariables("design variables", tNumCells, PhysicsT::mNumNodesPerCell);
    Kokkos::deep_copy(tDesignVariables, 1.0);

    // 2.3 SET GLOBAL STATE
    auto tNumDofsPerNode = PhysicsT::mNumDofsPerNode;
    Plato::ScalarVector tGlobalState("global state", tSpaceDim * tNumNodes);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0,tNumNodes), LAMBDA_EXPRESSION(const Plato::OrdinalType & aNodeOrdinal)
    {
        tGlobalState(aNodeOrdinal*tNumDofsPerNode+0) = (1e-7)*aNodeOrdinal; // disp_x
        tGlobalState(aNodeOrdinal*tNumDofsPerNode+1) = (2e-7)*aNodeOrdinal; // disp_y
        tGlobalState(aNodeOrdinal*tNumDofsPerNode+2) = (3e-7)*aNodeOrdinal; // disp_z
        tGlobalState(aNodeOrdinal*tNumDofsPerNode+3) = (5e-7)*aNodeOrdinal; // press
    }, "set global state");
    Plato::ScalarMultiVectorT<EvalType::StateScalarType> tCurrentGlobalState("current global state", tNumCells, PhysicsT::mNumDofsPerCell);
    tWorksetBase.worksetState(tGlobalState, tCurrentGlobalState);
    Plato::ScalarMultiVectorT<EvalType::PrevStateScalarType> tPrevGlobalState("previous global state", tNumCells, PhysicsT::mNumDofsPerCell);

    // 2.4 SET PROJECTED PRESSURE GRADIENT
    auto tNumNodesPerCell = PhysicsT::mNumNodesPerCell;
    Plato::ScalarMultiVectorT<EvalType::NodeStateScalarType> tProjectedPressureGrad("projected pressure grad", tNumCells, PhysicsT::mNumNodeStatePerCell);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0,tNumCells), LAMBDA_EXPRESSION(const Plato::OrdinalType & aCellOrdinal)
    {
        for(Plato::OrdinalType tNodeIndex=0; tNodeIndex< tNumNodesPerCell; tNodeIndex++)
        {
            for(Plato::OrdinalType tDimIndex=0; tDimIndex< tSpaceDim; tDimIndex++)
            {
                tProjectedPressureGrad(aCellOrdinal, tNodeIndex*tSpaceDim+tDimIndex) = (4e-7)*(tNodeIndex+1)*(tDimIndex+1)*(aCellOrdinal+1);
            }
        }
    }, "set projected pressure grad");

    // 2.5 SET LOCAL STATE
    Plato::ScalarMultiVectorT<EvalType::LocalStateScalarType> tCurrentLocalState("current local state", tNumCells, PhysicsT::mNumLocalDofsPerCell);
    Plato::ScalarMultiVectorT<EvalType::PrevLocalStateScalarType> tPrevLocalState("previous local state", tNumCells, PhysicsT::mNumLocalDofsPerCell);

    // 3. CALL FUNCTION
    Plato::InfinitesimalStrainPlasticityResidual<EvalType, PhysicsT> tComputeElastoPlasticity(*tMesh, tMeshSets, tDataMap, *tElastoPlasticityInputs);
    Plato::ScalarMultiVectorT<EvalType::ResultScalarType> tElastoPlasticityResidual("residual", tNumCells, PhysicsT::mNumDofsPerCell);
    tComputeElastoPlasticity.evaluate(tCurrentGlobalState, tPrevGlobalState, tCurrentLocalState, tPrevLocalState,
                                      tProjectedPressureGrad, tDesignVariables, tConfiguration, tElastoPlasticityResidual);

    // 4. GET GOLD VALUES - COMPARE AGAINST STABILIZED MECHANICS, NO PLASTICITY
    using GoldPhysicsT = Plato::SimplexStabilizedMechanics<tSpaceDim>;
    using GoldEvalType = typename Plato::Evaluation<GoldPhysicsT>::Residual;
    auto tResidualParams = tElastoPlasticityInputs->sublist("Elliptic");
    auto tPenaltyParams = tResidualParams.sublist("Penalty Function");
    Plato::StabilizedElastostaticResidual<GoldEvalType, Plato::MSIMP> tComputeStabilizedMech(*tMesh, tMeshSets, tDataMap, *tElastoPlasticityInputs, tPenaltyParams);
    Plato::ScalarMultiVectorT<GoldEvalType::ResultScalarType> tStabilizedMechResidual("residual", tNumCells, GoldPhysicsT::mNumDofsPerCell);
    tComputeStabilizedMech.evaluate(tCurrentGlobalState, tProjectedPressureGrad, tDesignVariables, tConfiguration, tStabilizedMechResidual);

    // 5. TEST RESULTS
    constexpr Plato::Scalar tTolerance = 1e-6;
    auto tHostGold = Kokkos::create_mirror(tStabilizedMechResidual);
    Kokkos::deep_copy(tHostGold, tStabilizedMechResidual);
    auto tHostElastoPlasticityResidual = Kokkos::create_mirror(tElastoPlasticityResidual);
    Kokkos::deep_copy(tHostElastoPlasticityResidual, tElastoPlasticityResidual);
    for(Plato::OrdinalType tCellIndex=0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tDofIndex=0; tDofIndex< PhysicsT::mNumDofsPerCell; tDofIndex++)
        {
            TEST_FLOATING_EQUALITY(tHostElastoPlasticityResidual(tCellIndex, tDofIndex), tHostGold(tCellIndex, tDofIndex), tTolerance);
        }
    }
}

TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_Residual2D_Elastic)
{
    // 1. PREPARE PROBLEM INPUS FOR TEST
    Plato::DataMap tDataMap;
    Omega_h::MeshSets tMeshSets;
    constexpr Plato::OrdinalType tSpaceDim = 2;
    constexpr Plato::OrdinalType tMeshWidth = 1;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);

    Teuchos::RCP<Teuchos::ParameterList> tElastoPlasticityInputs =
        Teuchos::getParametersFromXmlString(
        "<ParameterList name='Plato Problem'>                                   \n"
        "  <ParameterList name='Material Model'>                                \n"
        "    <ParameterList name='Isotropic Linear Elastic'>                    \n"
        "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>    \n"
        "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>  \n"
        "    </ParameterList>                                                   \n"
        "  </ParameterList>                                                     \n"
        "  <ParameterList name='Infinite Strain Plasticity'>                    \n"
        "    <ParameterList name='Penalty Function'>                            \n"
        "      <Parameter name='Type' type='string' value='SIMP'/>              \n"
        "      <Parameter name='Exponent' type='double' value='3.0'/>           \n"
        "      <Parameter name='Minimum Value' type='double' value='1.0e-6'/>   \n"
        "    </ParameterList>                                                   \n"
        "  </ParameterList>                                                     \n"
        "</ParameterList>                                                       \n"
      );

    // 2. PREPARE FUNCTION INPUTS FOR TEST
    const Plato::OrdinalType tNumNodes = tMesh->nverts();
    const Plato::OrdinalType tNumCells = tMesh->nelems();
    using PhysicsT = Plato::SimplexPlasticity<tSpaceDim>;
    using EvalType = typename Plato::Evaluation<PhysicsT>::Residual;
    Plato::WorksetBase<PhysicsT> tWorksetBase(*tMesh);

    // 2.1 SET CONFIGURATION
    Plato::ScalarArray3DT<EvalType::ConfigScalarType> tConfiguration("configuration", tNumCells, PhysicsT::mNumNodesPerCell, tSpaceDim);
    tWorksetBase.worksetConfig(tConfiguration);

    // 2.2 SET DESIGN VARIABLES
    Plato::ScalarMultiVectorT<EvalType::ControlScalarType> tDesignVariables("design variables", tNumCells, PhysicsT::mNumNodesPerCell);
    Kokkos::deep_copy(tDesignVariables, 1.0);

    // 2.3 SET GLOBAL STATE
    auto tNumDofsPerNode = PhysicsT::mNumDofsPerNode;
    Plato::ScalarVector tGlobalState("global state", tSpaceDim * tNumNodes);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0,tNumNodes), LAMBDA_EXPRESSION(const Plato::OrdinalType & aNodeOrdinal)
    {
        tGlobalState(aNodeOrdinal*tNumDofsPerNode+0) = (1e-7)*aNodeOrdinal; // disp_x
        tGlobalState(aNodeOrdinal*tNumDofsPerNode+1) = (2e-7)*aNodeOrdinal; // disp_y
        tGlobalState(aNodeOrdinal*tNumDofsPerNode+2) = (3e-7)*aNodeOrdinal; // press
    }, "set global state");
    Plato::ScalarMultiVectorT<EvalType::StateScalarType> tCurrentGlobalState("current global state", tNumCells, PhysicsT::mNumDofsPerCell);
    tWorksetBase.worksetState(tGlobalState, tCurrentGlobalState);
    Plato::ScalarMultiVectorT<EvalType::PrevStateScalarType> tPrevGlobalState("previous global state", tNumCells, PhysicsT::mNumDofsPerCell);

    // 2.4 SET PROJECTED PRESSURE GRADIENT
    auto tNumNodesPerCell = PhysicsT::mNumNodesPerCell;
    Plato::ScalarMultiVectorT<EvalType::NodeStateScalarType> tProjectedPressureGrad("projected pressure grad", tNumCells, PhysicsT::mNumNodeStatePerCell);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0,tNumCells), LAMBDA_EXPRESSION(const Plato::OrdinalType & aCellOrdinal)
    {
        for(Plato::OrdinalType tNodeIndex=0; tNodeIndex< tNumNodesPerCell; tNodeIndex++)
        {
            for(Plato::OrdinalType tDimIndex=0; tDimIndex< tSpaceDim; tDimIndex++)
            {
                tProjectedPressureGrad(aCellOrdinal, tNodeIndex*tSpaceDim+tDimIndex) = (4e-7)*(tNodeIndex+1)*(tDimIndex+1)*(aCellOrdinal+1);
            }
        }
    }, "set projected pressure grad");

    // 2.5 SET LOCAL STATE
    Plato::ScalarMultiVectorT<EvalType::LocalStateScalarType> tCurrentLocalState("current local state", tNumCells, PhysicsT::mNumLocalDofsPerCell);
    Plato::ScalarMultiVectorT<EvalType::PrevLocalStateScalarType> tPrevLocalState("previous local state", tNumCells, PhysicsT::mNumLocalDofsPerCell);

    // 3. CALL FUNCTION
    Plato::InfinitesimalStrainPlasticityResidual<EvalType, PhysicsT> tComputeElastoPlasticity(*tMesh, tMeshSets, tDataMap, *tElastoPlasticityInputs);
    Plato::ScalarMultiVectorT<EvalType::ResultScalarType> tElastoPlasticityResidual("residual", tNumCells, PhysicsT::mNumDofsPerCell);
    tComputeElastoPlasticity.evaluate(tCurrentGlobalState, tPrevGlobalState, tCurrentLocalState, tPrevLocalState,
                                      tProjectedPressureGrad, tDesignVariables, tConfiguration, tElastoPlasticityResidual);

    // 5. TEST RESULTS
    constexpr Plato::Scalar tTolerance = 1e-4;
    auto tHostElastoPlasticityResidual = Kokkos::create_mirror(tElastoPlasticityResidual);
    Kokkos::deep_copy(tHostElastoPlasticityResidual, tElastoPlasticityResidual);
    std::vector<std::vector<Plato::Scalar>> tGold =
        {{-0.310897, -0.0961538462, 0.2003656347, 0.214744, -0.0224359, -0.3967844462,  0.0961538462, 0.11859, 0.0297521448},
         {0.125, 0.0576923077, -0.0853066085, -0.0673077, 0.1057692308, 5.45966e-07,  -0.0576923077, -0.1634615385, 0.0853060625}};
    for(Plato::OrdinalType tCellIndex=0; tCellIndex < tNumCells; tCellIndex++)
    {
        for(Plato::OrdinalType tDofIndex=0; tDofIndex< PhysicsT::mNumDofsPerCell; tDofIndex++)
        {
            //printf("residual(%d,%d) = %.10f\n", tCellIndex, tDofIndex, tHostElastoPlasticityResidual(tCellIndex, tDofIndex));
            TEST_FLOATING_EQUALITY(tHostElastoPlasticityResidual(tCellIndex, tDofIndex), tGold[tCellIndex][tDofIndex], tTolerance);
        }
    }
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_TestPartialMaximizePlasticWorkWrtControl_3D)
{
    constexpr Plato::OrdinalType tSpaceDim = 3;
    constexpr Plato::OrdinalType tMeshWidth = 1;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    // ### NOTICE THAT THIS IS ONLY PLASTICITY (NO TEMPERATURE) ###
    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
    "<ParameterList name='Plato Problem'>                                                    \n"
    "  <ParameterList name='Material Model'>                                                 \n"
    "    <ParameterList name='Isotropic Linear Elastic'>                                     \n"
    "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                     \n"
    "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                   \n"
    "    </ParameterList>                                                                    \n"
    "    <ParameterList name='J2 Plasticity'>                                                \n"
    "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>             \n"
    "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>  \n"
    "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/> \n"
    "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/>\n"
    "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/> \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "  <ParameterList name='My Maximize Plastic Work'>                                       \n"
    "    <Parameter name='Type' type='string' value='Scalar Function'/>                      \n"
    "    <Parameter name='Scalar Function Type' type='string' value='Maximize Plastic Work'/>\n"
    "    <ParameterList name='Penalty Function'>                                             \n"
    "      <Parameter name='Type' type='string' value='SIMP'/>                               \n"
    "      <Parameter name='Exponent' type='double' value='3.0'/>                            \n"
    "      <Parameter name='Minimum Value' type='double' value='1.0e-3'/>                    \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "</ParameterList>                                                                        \n"
  );

    // TEST INTERMEDIATE TIME STEP GRADIENT
    printf("\nINTERMEDIATE TIME STEP");
    std::string tFuncName = "My Maximize Plastic Work";
    std::shared_ptr<Plato::LocalScalarFunctionInc> tScalarFunc =
        std::make_shared<Plato::BasicLocalScalarFunctionInc<PhysicsT>>(*tMesh, tMeshSets, tDataMap, *tParamList, tFuncName);
    Plato::test_partial_local_scalar_func_wrt_control<PhysicsT::SimplexT>(tScalarFunc, *tMesh);

    // TEST FINAL TIME STEP GRADIENT
    printf("\nFINAL TIME STEP");
    const Plato::Scalar tTimeStepIndex = 39; // default value is 40 time steps
    Plato::test_partial_local_scalar_func_wrt_control<PhysicsT::SimplexT>(tScalarFunc, *tMesh, tTimeStepIndex);
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_TestPartialMaximizePlasticWorkWrtControl_2D)
{
    constexpr Plato::OrdinalType tSpaceDim = 2;
    constexpr Plato::OrdinalType tMeshWidth = 1;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    // ### NOTICE THAT THIS IS ONLY PLASTICITY (NO TEMPERATURE) ###
    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
    "<ParameterList name='Plato Problem'>                                                    \n"
    "  <ParameterList name='Material Model'>                                                 \n"
    "    <ParameterList name='Isotropic Linear Elastic'>                                     \n"
    "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                     \n"
    "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                   \n"
    "    </ParameterList>                                                                    \n"
    "    <ParameterList name='J2 Plasticity'>                                                \n"
    "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>             \n"
    "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>  \n"
    "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/> \n"
    "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/>\n"
    "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/> \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "  <ParameterList name='My Maximize Plastic Work'>                                       \n"
    "    <Parameter name='Type' type='string' value='Scalar Function'/>                      \n"
    "    <Parameter name='Scalar Function Type' type='string' value='Maximize Plastic Work'/>\n"
    "    <ParameterList name='Penalty Function'>                                             \n"
    "      <Parameter name='Type' type='string' value='SIMP'/>                               \n"
    "      <Parameter name='Exponent' type='double' value='3.0'/>                            \n"
    "      <Parameter name='Minimum Value' type='double' value='1.0e-3'/>                    \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "</ParameterList>                                                                        \n"
  );

    // TEST INTERMEDIATE TIME STEP GRADIENT
    printf("\nINTERMEDIATE TIME STEP");
    std::string tFuncName = "My Maximize Plastic Work";
    std::shared_ptr<Plato::LocalScalarFunctionInc> tScalarFunc =
        std::make_shared<Plato::BasicLocalScalarFunctionInc<PhysicsT>>(*tMesh, tMeshSets, tDataMap, *tParamList, tFuncName);
    Plato::test_partial_local_scalar_func_wrt_control<PhysicsT::SimplexT>(tScalarFunc, *tMesh);

    // TEST FINAL TIME STEP GRADIENT
    printf("\nFINAL TIME STEP");
    const Plato::Scalar tTimeStepIndex = 39; // default value is 40 time steps
    Plato::test_partial_local_scalar_func_wrt_control<PhysicsT::SimplexT>(tScalarFunc, *tMesh, tTimeStepIndex);
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_TestPartialMaximizePlasticWorkWrtCurrentGlobalStates_3D)
{
    constexpr Plato::OrdinalType tSpaceDim = 3;
    constexpr Plato::OrdinalType tMeshWidth = 1;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    // ### NOTICE THAT THIS IS ONLY PLASTICITY (NO TEMPERATURE) ###
    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
    "<ParameterList name='Plato Problem'>                                                    \n"
    "  <ParameterList name='Material Model'>                                                 \n"
    "    <ParameterList name='Isotropic Linear Elastic'>                                     \n"
    "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                     \n"
    "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                   \n"
    "    </ParameterList>                                                                    \n"
    "    <ParameterList name='J2 Plasticity'>                                                \n"
    "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>             \n"
    "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>  \n"
    "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/> \n"
    "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/>\n"
    "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/> \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "  <ParameterList name='My Maximize Plastic Work'>                                       \n"
    "    <Parameter name='Type' type='string' value='Scalar Function'/>                      \n"
    "    <Parameter name='Scalar Function Type' type='string' value='Maximize Plastic Work'/>\n"
    "    <ParameterList name='Penalty Function'>                                             \n"
    "      <Parameter name='Type' type='string' value='SIMP'/>                               \n"
    "      <Parameter name='Exponent' type='double' value='3.0'/>                            \n"
    "      <Parameter name='Minimum Value' type='double' value='1.0e-3'/>                    \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "</ParameterList>                                                                        \n"
  );

    // TEST INTERMEDIATE TIME STEP GRADIENT
    printf("\nINTERMEDIATE TIME STEP");
    std::string tFuncName = "My Maximize Plastic Work";
    std::shared_ptr<Plato::LocalScalarFunctionInc> tScalarFunc =
        std::make_shared<Plato::BasicLocalScalarFunctionInc<PhysicsT>>(*tMesh, tMeshSets, tDataMap, *tParamList, tFuncName);
    Plato::test_partial_local_scalar_func_wrt_current_global_state<PhysicsT::SimplexT>(tScalarFunc, *tMesh);

    // TEST FINAL TIME STEP GRADIENT
    printf("\nFINAL TIME STEP");
    const Plato::Scalar tTimeStepIndex = 39; // default value is 40 time steps
    Plato::test_partial_local_scalar_func_wrt_current_global_state<PhysicsT::SimplexT>(tScalarFunc, *tMesh, tTimeStepIndex);
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_TestPartialMaximizePlasticWorkWrtCurrentGlobalStates_2D)
{
    constexpr Plato::OrdinalType tSpaceDim = 2;
    constexpr Plato::OrdinalType tMeshWidth = 1;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    // ### NOTICE THAT THIS IS ONLY PLASTICITY (NO TEMPERATURE) ###
    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
    "<ParameterList name='Plato Problem'>                                                    \n"
    "  <ParameterList name='Material Model'>                                                 \n"
    "    <ParameterList name='Isotropic Linear Elastic'>                                     \n"
    "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                     \n"
    "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                   \n"
    "    </ParameterList>                                                                    \n"
    "    <ParameterList name='J2 Plasticity'>                                                \n"
    "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>             \n"
    "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>  \n"
    "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/> \n"
    "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/>\n"
    "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/> \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "  <ParameterList name='My Maximize Plastic Work'>                                       \n"
    "    <Parameter name='Type' type='string' value='Scalar Function'/>                      \n"
    "    <Parameter name='Scalar Function Type' type='string' value='Maximize Plastic Work'/>\n"
    "    <ParameterList name='Penalty Function'>                                             \n"
    "      <Parameter name='Type' type='string' value='SIMP'/>                               \n"
    "      <Parameter name='Exponent' type='double' value='3.0'/>                            \n"
    "      <Parameter name='Minimum Value' type='double' value='1.0e-3'/>                    \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "</ParameterList>                                                                        \n"
  );

    // TEST INTERMEDIATE TIME STEP GRADIENT
    printf("\nINTERMEDIATE TIME STEP");
    std::string tFuncName = "My Maximize Plastic Work";
    std::shared_ptr<Plato::LocalScalarFunctionInc> tScalarFunc =
        std::make_shared<Plato::BasicLocalScalarFunctionInc<PhysicsT>>(*tMesh, tMeshSets, tDataMap, *tParamList, tFuncName);
    Plato::test_partial_local_scalar_func_wrt_current_global_state<PhysicsT::SimplexT>(tScalarFunc, *tMesh);

    // TEST FINAL TIME STEP GRADIENT
    printf("\nFINAL TIME STEP");
    const Plato::Scalar tTimeStepIndex = 39; // default value is 40 time steps
    Plato::test_partial_local_scalar_func_wrt_current_global_state<PhysicsT::SimplexT>(tScalarFunc, *tMesh, tTimeStepIndex);
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_TestPartialMaximizePlasticWorkWrtCurrentLocalStates_3D)
{
    constexpr Plato::OrdinalType tSpaceDim = 3;
    constexpr Plato::OrdinalType tMeshWidth = 1;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    // ### NOTICE THAT THIS IS ONLY PLASTICITY (NO TEMPERATURE) ###
    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
    "<ParameterList name='Plato Problem'>                                                    \n"
    "  <ParameterList name='Material Model'>                                                 \n"
    "    <ParameterList name='Isotropic Linear Elastic'>                                     \n"
    "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                     \n"
    "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                   \n"
    "    </ParameterList>                                                                    \n"
    "    <ParameterList name='J2 Plasticity'>                                                \n"
    "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>             \n"
    "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>  \n"
    "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/> \n"
    "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/>\n"
    "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/> \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "  <ParameterList name='My Maximize Plastic Work'>                                       \n"
    "    <Parameter name='Type' type='string' value='Scalar Function'/>                      \n"
    "    <Parameter name='Scalar Function Type' type='string' value='Maximize Plastic Work'/>\n"
    "    <ParameterList name='Penalty Function'>                                             \n"
    "      <Parameter name='Type' type='string' value='SIMP'/>                               \n"
    "      <Parameter name='Exponent' type='double' value='3.0'/>                            \n"
    "      <Parameter name='Minimum Value' type='double' value='1.0e-3'/>                    \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "</ParameterList>                                                                        \n"
  );

    // TEST INTERMEDIATE TIME STEP GRADIENT
    printf("\nINTERMEDIATE TIME STEP");
    std::string tFuncName = "My Maximize Plastic Work";
    std::shared_ptr<Plato::LocalScalarFunctionInc> tScalarFunc =
        std::make_shared<Plato::BasicLocalScalarFunctionInc<PhysicsT>>(*tMesh, tMeshSets, tDataMap, *tParamList, tFuncName);
    Plato::test_partial_local_scalar_func_wrt_current_local_state<PhysicsT::SimplexT>(tScalarFunc, *tMesh);

    // TEST FINAL TIME STEP GRADIENT
    printf("\nFINAL TIME STEP");
    const Plato::Scalar tTimeStepIndex = 39; // default value is 40 time steps
    Plato::test_partial_local_scalar_func_wrt_current_local_state<PhysicsT::SimplexT>(tScalarFunc, *tMesh, tTimeStepIndex);
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_TestPartialMaximizePlasticWorkWrtCurrentLocalStates_2D)
{
    constexpr Plato::OrdinalType tSpaceDim = 2;
    constexpr Plato::OrdinalType tMeshWidth = 1;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    // ### NOTICE THAT THIS IS ONLY PLASTICITY (NO TEMPERATURE) ###
    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
    "<ParameterList name='Plato Problem'>                                                    \n"
    "  <ParameterList name='Material Model'>                                                 \n"
    "    <ParameterList name='Isotropic Linear Elastic'>                                     \n"
    "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                     \n"
    "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                   \n"
    "    </ParameterList>                                                                    \n"
    "    <ParameterList name='J2 Plasticity'>                                                \n"
    "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>             \n"
    "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>  \n"
    "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/> \n"
    "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/>\n"
    "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/> \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "  <ParameterList name='My Maximize Plastic Work'>                                       \n"
    "    <Parameter name='Type' type='string' value='Scalar Function'/>                      \n"
    "    <Parameter name='Scalar Function Type' type='string' value='Maximize Plastic Work'/>\n"
    "    <ParameterList name='Penalty Function'>                                             \n"
    "      <Parameter name='Type' type='string' value='SIMP'/>                               \n"
    "      <Parameter name='Exponent' type='double' value='3.0'/>                            \n"
    "      <Parameter name='Minimum Value' type='double' value='1.0e-3'/>                    \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "</ParameterList>                                                                        \n"
  );

    // TEST INTERMEDIATE TIME STEP GRADIENT
    printf("\nINTERMEDIATE TIME STEP");
    std::string tFuncName = "My Maximize Plastic Work";
    std::shared_ptr<Plato::LocalScalarFunctionInc> tScalarFunc =
        std::make_shared<Plato::BasicLocalScalarFunctionInc<PhysicsT>>(*tMesh, tMeshSets, tDataMap, *tParamList, tFuncName);
    Plato::test_partial_local_scalar_func_wrt_current_local_state<PhysicsT::SimplexT>(tScalarFunc, *tMesh);

    // TEST FINAL TIME STEP GRADIENT
    printf("\nFINAL TIME STEP");
    const Plato::Scalar tTimeStepIndex = 39; // default value is 40 time steps
    Plato::test_partial_local_scalar_func_wrt_current_local_state<PhysicsT::SimplexT>(tScalarFunc, *tMesh, tTimeStepIndex);
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_TestPartialResidualWrtPreviousLocalStates_3D)
{
    constexpr Plato::OrdinalType tSpaceDim = 3;
    constexpr Plato::OrdinalType tMeshWidth = 1;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    // ### NOTICE THAT THIS IS ONLY PLASTICITY (NO TEMPERATURE) ###
    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
    "<ParameterList name='Plato Problem'>                                                    \n"
    "  <ParameterList name='Material Model'>                                                 \n"
    "    <ParameterList name='Isotropic Linear Elastic'>                                     \n"
    "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                     \n"
    "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                   \n"
    "    </ParameterList>                                                                    \n"
    "    <ParameterList name='J2 Plasticity'>                                                \n"
    "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>             \n"
    "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>  \n"
    "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/> \n"
    "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/>\n"
    "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/> \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "  <ParameterList name='Infinite Strain Plasticity'>                                     \n"
    "    <ParameterList name='Penalty Function'>                                             \n"
    "      <Parameter name='Type' type='string' value='SIMP'/>                               \n"
    "      <Parameter name='Exponent' type='double' value='3.0'/>                            \n"
    "      <Parameter name='Minimum Value' type='double' value='1.0e-6'/>                    \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "</ParameterList>                                                                        \n"
  );

    std::string tFuncName = "Infinite Strain Plasticity";
    std::shared_ptr<Plato::GlobalVectorFunctionInc<PhysicsT>> tVectorFunc =
        std::make_shared<Plato::GlobalVectorFunctionInc<PhysicsT>>(*tMesh, tMeshSets, tDataMap, *tParamList, tFuncName);
    Plato::test_partial_global_jacobian_wrt_previous_local_states<PhysicsT::SimplexT>(tVectorFunc, *tMesh);
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_TestPartialResidualWrtPreviousLocalStates_2D)
{
    constexpr Plato::OrdinalType tSpaceDim = 2;
    constexpr Plato::OrdinalType tMeshWidth = 1;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    // ### NOTICE THAT THIS IS ONLY PLASTICITY (NO TEMPERATURE) ###
    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
    "<ParameterList name='Plato Problem'>                                                    \n"
    "  <ParameterList name='Material Model'>                                                 \n"
    "    <ParameterList name='Isotropic Linear Elastic'>                                     \n"
    "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                     \n"
    "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                   \n"
    "    </ParameterList>                                                                    \n"
    "    <ParameterList name='J2 Plasticity'>                                                \n"
    "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>             \n"
    "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>  \n"
    "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/> \n"
    "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/>\n"
    "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/> \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "  <ParameterList name='Infinite Strain Plasticity'>                                     \n"
    "    <ParameterList name='Penalty Function'>                                             \n"
    "      <Parameter name='Type' type='string' value='SIMP'/>                               \n"
    "      <Parameter name='Exponent' type='double' value='3.0'/>                            \n"
    "      <Parameter name='Minimum Value' type='double' value='1.0e-6'/>                    \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "</ParameterList>                                                                        \n"
  );

    std::string tFuncName = "Infinite Strain Plasticity";
    std::shared_ptr<Plato::GlobalVectorFunctionInc<PhysicsT>> tVectorFunc =
        std::make_shared<Plato::GlobalVectorFunctionInc<PhysicsT>>(*tMesh, tMeshSets, tDataMap, *tParamList, tFuncName);
    Plato::test_partial_global_jacobian_wrt_previous_local_states<PhysicsT::SimplexT>(tVectorFunc, *tMesh);
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_TestPartialResidualWrtCurrentLocalStates_2D)
{
    constexpr Plato::OrdinalType tSpaceDim = 3;
    constexpr Plato::OrdinalType tMeshWidth = 1;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    // ### NOTICE THAT THIS IS ONLY PLASTICITY (NO TEMPERATURE) ###
    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
    "<ParameterList name='Plato Problem'>                                                    \n"
    "  <ParameterList name='Material Model'>                                                 \n"
    "    <ParameterList name='Isotropic Linear Elastic'>                                     \n"
    "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                     \n"
    "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                   \n"
    "    </ParameterList>                                                                    \n"
    "    <ParameterList name='J2 Plasticity'>                                                \n"
    "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>      \n"
    "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>             \n"
    "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>  \n"
    "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/> \n"
    "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/>\n"
    "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/> \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "  <ParameterList name='Infinite Strain Plasticity'>                                     \n"
    "    <ParameterList name='Penalty Function'>                                             \n"
    "      <Parameter name='Type' type='string' value='SIMP'/>                               \n"
    "      <Parameter name='Exponent' type='double' value='3.0'/>                            \n"
    "      <Parameter name='Minimum Value' type='double' value='1.0e-6'/>                    \n"
    "    </ParameterList>                                                                    \n"
    "  </ParameterList>                                                                      \n"
    "</ParameterList>                                                                        \n"
  );

    std::string tFuncName = "Infinite Strain Plasticity";
    std::shared_ptr<Plato::GlobalVectorFunctionInc<PhysicsT>> tVectorFunc =
        std::make_shared<Plato::GlobalVectorFunctionInc<PhysicsT>>(*tMesh, tMeshSets, tDataMap, *tParamList, tFuncName);
    Plato::test_partial_global_jacobian_wrt_current_local_states<PhysicsT::SimplexT>(tVectorFunc, *tMesh);
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_TestPlasticityProblem_2D)
{
    // 1. DEFINE PROBLEM
    constexpr Plato::OrdinalType tSpaceDim = 2;
    constexpr Plato::OrdinalType tMeshWidth = 2;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
      "<ParameterList name='Plato Problem'>                                                     \n"
      "  <Parameter name='Physics'          type='string'  value='Mechanical'/>                 \n"
      "  <Parameter name='PDE Constraint'   type='string'  value='Infinite Strain Plasticity'/> \n"
      "  <ParameterList name='Material Model'>                                                  \n"
      "    <ParameterList name='Isotropic Linear Elastic'>                                      \n"
      "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                      \n"
      "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                    \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Plasticity Model'>                                                \n"
      "    <ParameterList name='J2 Plasticity'>                                                 \n"
      "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>              \n"
      "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>   \n"
      "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/>  \n"
      "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/> \n"
      "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/>  \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Infinite Strain Plasticity'>                                      \n"
      "    <ParameterList name='Penalty Function'>                                              \n"
      "      <Parameter name='Type' type='string' value='SIMP'/>                                \n"
      "      <Parameter name='Exponent' type='double' value='3.0'/>                             \n"
      "      <Parameter name='Minimum Value' type='double' value='1.0e-6'/>                     \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Time Stepping'>                                                   \n"
      "    <Parameter name='Initial Num. Pseudo Time Steps' type='int' value='1'/>              \n"
      "    <Parameter name='Maximum Num. Pseudo Time Steps' type='int' value='1'/>              \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Newton-Raphson'>                                                  \n"
      "    <Parameter name='Maximum Number Iterations' type='int' value='10'/>                  \n"
      "  </ParameterList>                                                                       \n"
      "</ParameterList>                                                                         \n"
    );

    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;
    Plato::PlasticityProblem<PhysicsT> tPlasticityProblem(*tMesh, tMeshSets, *tParamList);

    // 2. Get Dirichlet Boundary Conditions
    Plato::OrdinalType tDispDofX = 0;
    Plato::OrdinalType tDispDofY = 1;
    constexpr Plato::OrdinalType tNumDofsPerNode = PhysicsT::mNumDofsPerNode;
    auto tDirichletIndicesBoundaryX0 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "x0", tNumDofsPerNode, tDispDofX);
    auto tDirichletIndicesBoundaryY0 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "y0", tNumDofsPerNode, tDispDofY);
    auto tDirichletIndicesBoundaryX1 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "x1", tNumDofsPerNode, tDispDofX);

    // 3. Set Dirichlet Boundary Conditions
    Plato::Scalar tValueToSet = 0;
    auto tNumDirichletDofs = tDirichletIndicesBoundaryX0.size() + tDirichletIndicesBoundaryY0.size() + tDirichletIndicesBoundaryX1.size();
    TEST_EQUALITY(9, tNumDirichletDofs);
    Plato::ScalarVector tDirichletValues("Dirichlet Values", tNumDirichletDofs);
    Plato::LocalOrdinalVector tDirichletDofs("Dirichlet Dofs", tNumDirichletDofs);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX0.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        tDirichletValues(aIndex) = tValueToSet;
        tDirichletDofs(aIndex) = tDirichletIndicesBoundaryX0(aIndex);
    }, "set dirichlet values and indices");

    auto tOffset = tDirichletIndicesBoundaryX0.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryY0.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryY0(aIndex);
    }, "set dirichlet values and indices");

    tValueToSet = 1e-5;
    tOffset += tDirichletIndicesBoundaryY0.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX1.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryX1(aIndex);
    }, "set dirichlet values/indices");
    tPlasticityProblem.setEssentialBoundaryConditions(tDirichletDofs, tDirichletValues);

    // 4. Solve Problem
    auto tNumVertices = tMesh->nverts();
    Plato::ScalarVector tControls("Controls", tNumVertices);
    Plato::fill(1.0, tControls);
    auto tSolution = tPlasticityProblem.solution(tControls);

    // 5. Test solution
    const Plato::Scalar tTolerance = 1e-5;
    auto tHostSolution = Kokkos::create_mirror(tSolution);
    Kokkos::deep_copy(tHostSolution, tSolution);
    std::vector<std::vector<Plato::Scalar>> tGold = 
        {{0.0, 0.0, 1.1428571429e-05, 0.0, -4.2857142857e-06, 1.1428571429e-05, 0.0, -8.5714285714e-06, 1.1428571429e-05, 
          1e-5, -4.2857142857e-06, 1.1428571429e-05, 1e-5, -8.5714285714e-06, 1.1428571429e-05, 1e-5, -8.5714285714e-06, 1.1428571429e-05,
          1e-5, -4.2857142857e-06, 1.1428571429e-05, 1e-5, 0.0, 1.1428571429e-05, 1e-5, 0.0, 1.1428571429e-05}};
    for(Plato::OrdinalType tTimeIndex = 0; tTimeIndex < tSolution.extent(0); tTimeIndex++)
    {
        for(Plato::OrdinalType tDofIndex=0; tDofIndex < tSolution.extent(1); tDofIndex++)
        {
            //printf("solution(%d,%d) = %.10e\n", tTimeIndex, tDofIndex, tHostSolution(tTimeIndex, tDofIndex));
            TEST_FLOATING_EQUALITY(tHostSolution(tTimeIndex,tDofIndex), tGold[tTimeIndex][tDofIndex], tTolerance);
        }
    }
    std::system("rm -f plato_analyze_newton_raphson_diagnostics.txt");
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_TestPlasticityProblem_3D)
{
    // 1. DEFINE PROBLEM
    constexpr Plato::OrdinalType tSpaceDim = 3;
    constexpr Plato::OrdinalType tMeshWidth = 2;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
      "<ParameterList name='Plato Problem'>                                                     \n"
      "  <Parameter name='Physics'          type='string'  value='Mechanical'/>                 \n"
      "  <Parameter name='PDE Constraint'   type='string'  value='Infinite Strain Plasticity'/> \n"
      "  <ParameterList name='Material Model'>                                                  \n"
      "    <ParameterList name='Isotropic Linear Elastic'>                                      \n"
      "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                      \n"
      "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                    \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Plasticity Model'>                                                \n"
      "    <ParameterList name='J2 Plasticity'>                                                 \n"
      "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>              \n"
      "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>   \n"
      "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/>  \n"
      "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/> \n"
      "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/>  \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Infinite Strain Plasticity'>                                      \n"
      "    <ParameterList name='Penalty Function'>                                              \n"
      "      <Parameter name='Type' type='string' value='SIMP'/>                                \n"
      "      <Parameter name='Exponent' type='double' value='3.0'/>                             \n"
      "      <Parameter name='Minimum Value' type='double' value='1.0e-6'/>                     \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Time Stepping'>                                                   \n"
      "    <Parameter name='Initial Num. Pseudo Time Steps' type='int' value='1'/>              \n"
      "    <Parameter name='Maximum Num. Pseudo Time Steps' type='int' value='1'/>              \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Newton-Raphson'>                                                  \n"
      "    <Parameter name='Maximum Number Iterations' type='int' value='5'/>                   \n"
      "    <Parameter name='Stopping Tolerance' type='double' value='1e-8'/>                    \n"
      "  </ParameterList>                                                                       \n"
      "</ParameterList>                                                                         \n"
    );

    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;
    Plato::PlasticityProblem<PhysicsT> tPlasticityProblem(*tMesh, tMeshSets, *tParamList);

    // 2. Get Dirichlet Boundary Conditions
    Plato::OrdinalType tDispDofX = 0;
    Plato::OrdinalType tDispDofY = 1;
    constexpr Plato::OrdinalType tNumDofsPerNode = PhysicsT::mNumDofsPerNode;
    auto tDirichletIndicesBoundaryX0 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_3D(*tMesh, "x0", tNumDofsPerNode, tDispDofX);
    auto tDirichletIndicesBoundaryY0 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_3D(*tMesh, "y0", tNumDofsPerNode, tDispDofY);
    auto tDirichletIndicesBoundaryX1 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_3D(*tMesh, "x1", tNumDofsPerNode, tDispDofX);

    // 3. Set Dirichlet Boundary Conditions
    Plato::Scalar tValueToSet = 0;
    auto tNumDirichletDofs = tDirichletIndicesBoundaryX0.size() + tDirichletIndicesBoundaryY0.size() + tDirichletIndicesBoundaryX1.size();
    Plato::ScalarVector tDirichletValues("Dirichlet Values", tNumDirichletDofs);
    Plato::LocalOrdinalVector tDirichletDofs("Dirichlet Dofs", tNumDirichletDofs);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX0.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        tDirichletValues(aIndex) = tValueToSet;
        tDirichletDofs(aIndex) = tDirichletIndicesBoundaryX0(aIndex);
    }, "set dirichlet values/indices");

    auto tOffset = tDirichletIndicesBoundaryX0.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryY0.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryY0(aIndex);
    }, "set dirichlet values/indices");

    tValueToSet = 1e-5;
    tOffset += tDirichletIndicesBoundaryY0.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX1.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryX1(aIndex);
    }, "set dirichlet values/indices");
    tPlasticityProblem.setEssentialBoundaryConditions(tDirichletDofs, tDirichletValues);

    // 4. Solve Problem
    auto tNumVertices = tMesh->nverts();
    Plato::ScalarVector tControls("Controls", tNumVertices);
    Plato::fill(1.0, tControls);
    auto tSolution = tPlasticityProblem.solution(tControls);

    // 5. Test solution
    const Plato::Scalar tTolerance = 1e-5;
    auto tHostSolution = Kokkos::create_mirror(tSolution);
    Kokkos::deep_copy(tHostSolution, tSolution);
    std::vector<std::vector<Plato::Scalar>> tGold =
        {{0.0, 0.0, -2.8703524698e-06, 7.6000681146e-06, 3.7606218586e-06, 5.6258234412e-07, -3.4820159683e-06, 9.8896072581e-07, 1.3454392146e-06, 
          6.3829859669e-07, -3.6908417918e-06, -5.0242434384e-07, 4.8905381924e-06, -1.9797741763e-07, -3.2272119988e-06, 5.3825694399e-07, 2.2337985260e-06, 3.9352136992e-07, 
          -3.6613122996e-06, -1.0461558025e-06, 3.0646653970e-06, 3.0968953539e-07, -4.4330643613e-06, -1.6912127906e-06, 6.1353831347e-06, -8.3902524735e-07, -3.3287864385e-06,
          4.0051893738e-08, 0.0, -2.2087136143e-06, -3.1628566405e-06, 6.1437621526e-06, 0.0, -1.0328856755e-06, -2.7512486241e-06, 6.9631226734e-06,
          1.0118897471e-05, -1.4299979375e-06, -1.2847922538e-06, 7.0218871139e-06, 1e-5, -1.8949456964e-06, 8.0356747338e-06, 6.8737058133e-06, 1e-5, 
          -3.0514115429e-06, 6.0535265173e-06, 6.7526416932e-06, 1.0074968203e-05, -2.6445649347e-06, -2.2338003015e-06, 6.8880398610e-06, 5.4686654491e-06, -8.1418956010e-07,
          -1.9473174343e-06, 1.5380491366e-06, 1.7105070600e-06, -3.5158863076e-07, -1.5867390106e-06, -7.0845855271e-07, 2.7166329792e-06, -4.5479626163e-07, -2.3738866154e-06,
          -1.1610587760e-06, 6.2674597144e-06, -1.5727044615e-06, -2.9130041304e-06, 1.7042280112e-06, 6.3898614489e-06, -2.0803547953e-06, 3.5793858021e-06, 1.2902101000e-06,
          1.5486938011e-06, -1.0190937703e-06, 2.4665715419e-06, -2.4051998441e-06, 7.7831799636e-07, -9.1402627105e-07, 3.4729592621e-06, -2.0479500951e-06, 5.6564630070e-06,
          -1.2685500632e-06, 4.6874204507e-06, 9.6156949391e-07, 4.8147086071e-06, -1.5671149076e-08, -1.4982846502e-06, 1.3081703145e-06, 7.8714797564e-07, -1.2672735500e-07,
          -1.3823809188e-06, -2.7770927830e-07, 2.0372736008e-07, -6.8441754377e-07, 3.7440527656e-06, -1.4403634964e-06, 5.0900067907e-06, -3.1022541575e-07, 5.0526908215e-06,
          8.8415060576e-07, 1.0267855942e-05, 0.0, -8.1598232281e-07, 6.8148877602e-06, 1.0e-5, 0.0, 9.5515963354e-06, 6.9168938314e-06}}; 
    for(Plato::OrdinalType tTimeIndex = 0; tTimeIndex < tSolution.extent(0); tTimeIndex++)
    {
        for(Plato::OrdinalType tDofIndex=0; tDofIndex < tSolution.extent(1); tDofIndex++)
        {
            //printf("solution(%d,%d) = %.10e\n", tTimeIndex, tDofIndex, tHostSolution(tTimeIndex, tDofIndex));
            TEST_FLOATING_EQUALITY(tHostSolution(tTimeIndex,tDofIndex), tGold[tTimeIndex][tDofIndex], tTolerance);
        }
    }
    std::system("rm -f plato_analyze_newton_raphson_diagnostics.txt");
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_ConstraintValue_2D)
{
    // 1. DEFINE PROBLEM
    constexpr Plato::OrdinalType tSpaceDim = 2;
    constexpr Plato::OrdinalType tMeshWidth = 3;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
      "<ParameterList name='Plato Problem'>                                                     \n"
      "  <Parameter name='Physics'          type='string'  value='Mechanical'/>                 \n"
      "  <Parameter name='PDE Constraint'   type='string'  value='Infinite Strain Plasticity'/> \n"
      "  <Parameter name='Constraint'       type='string'  value='My Maximize Plastic Work'/>   \n"
      "  <ParameterList name='Material Model'>                                                  \n"
      "    <ParameterList name='Isotropic Linear Elastic'>                                      \n"
      "      <Parameter  name='Density' type='double' value='1000'/>                            \n"
      "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                      \n"
      "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                    \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Plasticity Model'>                                                \n"
      "    <ParameterList name='J2 Plasticity'>                                                 \n"
      "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>              \n"
      "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>   \n"
      "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/>  \n"
      "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/> \n"
      "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/>  \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Infinite Strain Plasticity'>                                      \n"
      "    <ParameterList name='Penalty Function'>                                              \n"
      "      <Parameter name='Type' type='string' value='SIMP'/>                                \n"
      "      <Parameter name='Exponent' type='double' value='3.0'/>                             \n"
      "      <Parameter name='Minimum Value' type='double' value='1.0e-6'/>                     \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='My Maximize Plastic Work'>                                        \n"
      "    <Parameter name='Type'                 type='string' value='Scalar Function'/>       \n"
      "    <Parameter name='Scalar Function Type' type='string' value='Maximize Plastic Work'/> \n"
      "    <Parameter name='Exponent'             type='double' value='3.0'/>                   \n"
      "    <Parameter name='Minimum Value'        type='double' value='1.0e-9'/>                \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Time Stepping'>                                                   \n"
      "    <Parameter name='Initial Num. Pseudo Time Steps' type='int' value='20'/>             \n"
      "    <Parameter name='Maximum Num. Pseudo Time Steps' type='int' value='40'/>             \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Newton-Raphson'>                                                  \n"
      "    <Parameter name='Stop Measure' type='string' value='residual'/>                      \n"
      "    <Parameter name='Maximum Number Iterations' type='int' value='50'/>                \n"
      "  </ParameterList>                                                                       \n"
      "</ParameterList>                                                                         \n"
    );

    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;
    Plato::PlasticityProblem<PhysicsT> tPlasticityProblem(*tMesh, tMeshSets, *tParamList);

    // 2. Get Dirichlet Boundary Conditions
    Plato::OrdinalType tDispDofX = 0;
    Plato::OrdinalType tDispDofY = 1;
    constexpr Plato::OrdinalType tNumDofsPerNode = PhysicsT::mNumDofsPerNode;
    auto tDirichletIndicesBoundaryX0 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "x0", tNumDofsPerNode, tDispDofX);
    auto tDirichletIndicesBoundaryY0 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "y0", tNumDofsPerNode, tDispDofY);
    auto tDirichletIndicesBoundaryX1 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "x1", tNumDofsPerNode, tDispDofX);

    // 3. Set Dirichlet Boundary Conditions
    Plato::Scalar tValueToSet = 0;
    auto tNumDirichletDofs = tDirichletIndicesBoundaryX0.size() + tDirichletIndicesBoundaryY0.size() + tDirichletIndicesBoundaryX1.size();
    Plato::ScalarVector tDirichletValues("Dirichlet Values", tNumDirichletDofs);
    Plato::LocalOrdinalVector tDirichletDofs("Dirichlet Dofs", tNumDirichletDofs);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX0.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        tDirichletValues(aIndex) = tValueToSet;
        tDirichletDofs(aIndex) = tDirichletIndicesBoundaryX0(aIndex);
    }, "set dirichlet values and indices");

    auto tOffset = tDirichletIndicesBoundaryX0.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryY0.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryY0(aIndex);
    }, "set dirichlet values and indices");

    tValueToSet = 6e-4;
    tOffset += tDirichletIndicesBoundaryY0.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX1.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryX1(aIndex);
    }, "set dirichlet values and indices");
    tPlasticityProblem.setEssentialBoundaryConditions(tDirichletDofs, tDirichletValues);

    // 4. Evaluate Objective Function
    auto tNumVertices = tMesh->nverts();
    Plato::ScalarVector tControls("Controls", tNumVertices);
    Plato::fill(1.0, tControls);
    auto tSolution = tPlasticityProblem.solution(tControls);
    auto tConstraintValue = tPlasticityProblem.constraintValue(tControls, tSolution);

    // 5. Test Results
    constexpr Plato::Scalar tTolerance = 1e-4;
    TEST_FLOATING_EQUALITY(tConstraintValue, -0.16819, tTolerance);
    std::system("rm -f plato_analyze_newton_raphson_diagnostics.txt");
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_ConstraintValue_3D)
{
    // 1. DEFINE PROBLEM
    constexpr Plato::OrdinalType tSpaceDim = 3;
    constexpr Plato::OrdinalType tMeshWidth = 3;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
      "<ParameterList name='Plato Problem'>                                                     \n"
      "  <Parameter name='Physics'          type='string'  value='Mechanical'/>                 \n"
      "  <Parameter name='PDE Constraint'   type='string'  value='Infinite Strain Plasticity'/> \n"
      "  <Parameter name='Constraint'       type='string'  value='My Maximize Plastic Work'/>   \n"
      "  <ParameterList name='Material Model'>                                                  \n"
      "    <ParameterList name='Isotropic Linear Elastic'>                                      \n"
      "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                      \n"
      "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                    \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Plasticity Model'>                                                \n"
      "    <ParameterList name='J2 Plasticity'>                                                 \n"
      "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>              \n"
      "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>   \n"
      "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/>  \n"
      "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/> \n"
      "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/>  \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Infinite Strain Plasticity'>                                      \n"
      "    <ParameterList name='Penalty Function'>                                              \n"
      "      <Parameter name='Type' type='string' value='SIMP'/>                                \n"
      "      <Parameter name='Exponent' type='double' value='3.0'/>                             \n"
      "      <Parameter name='Minimum Value' type='double' value='1.0e-6'/>                     \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='My Maximize Plastic Work'>                                        \n"
      "    <Parameter name='Type'                 type='string' value='Scalar Function'/>       \n"
      "    <Parameter name='Scalar Function Type' type='string' value='Maximize Plastic Work'/> \n"
      "    <Parameter name='Exponent'             type='double' value='3.0'/>                   \n"
      "    <Parameter name='Minimum Value'        type='double' value='1.0e-9'/>                \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Time Stepping'>                                                   \n"
      "    <Parameter name='Initial Num. Pseudo Time Steps' type='int' value='25'/>             \n"
      "    <Parameter name='Maximum Num. Pseudo Time Steps' type='int' value='25'/>             \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Newton-Raphson'>                                                  \n"
      "    <Parameter name='Stop Measure' type='string' value='residual'/>                      \n"
      "  </ParameterList>                                                                       \n"
      "</ParameterList>                                                                         \n"
    );

    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;
    Plato::PlasticityProblem<PhysicsT> tPlasticityProblem(*tMesh, tMeshSets, *tParamList);

    // 2. Get Dirichlet Boundary Conditions
    Plato::OrdinalType tDispDofX = 0;
    Plato::OrdinalType tDispDofY = 1;
    constexpr Plato::OrdinalType tNumDofsPerNode = PhysicsT::mNumDofsPerNode;
    auto tDirichletIndicesBoundaryX0 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "x0", tNumDofsPerNode, tDispDofX);
    auto tDirichletIndicesBoundaryY0 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "y0", tNumDofsPerNode, tDispDofY);
    auto tDirichletIndicesBoundaryX1 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "x1", tNumDofsPerNode, tDispDofX);

    // 3. Set Dirichlet Boundary Conditions
    Plato::Scalar tValueToSet = 0;
    auto tNumDirichletDofs = tDirichletIndicesBoundaryX0.size() + tDirichletIndicesBoundaryY0.size() + tDirichletIndicesBoundaryX1.size();
    Plato::ScalarVector tDirichletValues("Dirichlet Values", tNumDirichletDofs);
    Plato::LocalOrdinalVector tDirichletDofs("Dirichlet Dofs", tNumDirichletDofs);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX0.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        tDirichletValues(aIndex) = tValueToSet;
        tDirichletDofs(aIndex) = tDirichletIndicesBoundaryX0(aIndex);
    }, "set dirichlet values/indices");

    auto tOffset = tDirichletIndicesBoundaryX0.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryY0.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryY0(aIndex);
    }, "set dirichlet values/indices");

    tValueToSet = 6e-4;
    tOffset += tDirichletIndicesBoundaryY0.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX1.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryX1(aIndex);
    }, "set dirichlet values/indices");
    tPlasticityProblem.setEssentialBoundaryConditions(tDirichletDofs, tDirichletValues);

    // 4. Evaluate Objective Function
    auto tNumVertices = tMesh->nverts();
    Plato::ScalarVector tControls("Controls", tNumVertices);
    Plato::fill(1.0, tControls);
    auto tSolution = tPlasticityProblem.solution(tControls);
    auto tConstraintValue = tPlasticityProblem.constraintValue(tControls, tSolution);

    // 5. Test Results
    constexpr Plato::Scalar tTolerance = 1e-4;
    TEST_FLOATING_EQUALITY(tConstraintValue, -0.00518257, tTolerance);
    std::system("rm -f plato_analyze_newton_raphson_diagnostics.txt");
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_ObjectiveTest_2D)
{
    // 1. DEFINE PROBLEM
    constexpr Plato::OrdinalType tSpaceDim = 2;
    constexpr Plato::OrdinalType tMeshWidth = 1;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
      "<ParameterList name='Plato Problem'>                                                     \n"
      "  <Parameter name='Physics'          type='string'  value='Mechanical'/>                 \n"
      "  <Parameter name='PDE Constraint'   type='string'  value='Infinite Strain Plasticity'/> \n"
      "  <Parameter name='Objective'         type='string'  value='My Maximize Plastic Work'/>  \n"
      "  <ParameterList name='Material Model'>                                                  \n"
      "    <ParameterList name='Isotropic Linear Elastic'>                                      \n"
      "      <Parameter  name='Density' type='double' value='1000'/>                            \n"
      "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                      \n"
      "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                    \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Plasticity Model'>                                                \n"
      "    <ParameterList name='J2 Plasticity'>                                                 \n"
      "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>              \n"
      "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>   \n"
      "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/>  \n"
      "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/> \n"
      "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/>  \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Infinite Strain Plasticity'>                                      \n"
      "    <ParameterList name='Penalty Function'>                                              \n"
      "      <Parameter name='Type' type='string' value='SIMP'/>                                \n"
      "      <Parameter name='Exponent' type='double' value='3.0'/>                             \n"
      "      <Parameter name='Minimum Value' type='double' value='1.0e-6'/>                     \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='My Maximize Plastic Work'>                                        \n"
      "    <Parameter name='Type'                 type='string' value='Scalar Function'/>       \n"
      "    <Parameter name='Scalar Function Type' type='string' value='Maximize Plastic Work'/> \n"
      "    <Parameter name='Exponent'             type='double' value='3.0'/>                   \n"
      "    <Parameter name='Minimum Value'        type='double' value='1.0e-9'/>                \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Time Stepping'>                                                   \n"
      "    <Parameter name='Initial Num. Pseudo Time Steps' type='int' value='4'/>              \n"
      "    <Parameter name='Maximum Num. Pseudo Time Steps' type='int' value='4'/>              \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Newton-Raphson'>                                                  \n"
      "    <Parameter name='Stop Measure' type='string' value='residual'/>                      \n"
      "    <Parameter name='Maximum Number Iterations' type='int' value='20'/>                  \n"
      "  </ParameterList>                                                                       \n"
      "</ParameterList>                                                                         \n"
    );

    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;
    Plato::PlasticityProblem<PhysicsT> tPlasticityProblem(*tMesh, tMeshSets, *tParamList);

    // 2. Get Dirichlet Boundary Conditions
    Plato::OrdinalType tDispDofX = 0;
    Plato::OrdinalType tDispDofY = 1;
    constexpr Plato::OrdinalType tNumDofsPerNode = PhysicsT::mNumDofsPerNode;
    auto tDirichletIndicesBoundaryX0 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "x0", tNumDofsPerNode, tDispDofX);
    auto tDirichletIndicesBoundaryY0 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "y0", tNumDofsPerNode, tDispDofY);
    auto tDirichletIndicesBoundaryX1 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "x1", tNumDofsPerNode, tDispDofX);

    // 3. Set Dirichlet Boundary Conditions
    Plato::Scalar tValueToSet = 0;
    auto tNumDirichletDofs = tDirichletIndicesBoundaryX0.size() + tDirichletIndicesBoundaryY0.size() + tDirichletIndicesBoundaryX1.size();
    Plato::ScalarVector tDirichletValues("Dirichlet Values", tNumDirichletDofs);
    Plato::LocalOrdinalVector tDirichletDofs("Dirichlet Dofs", tNumDirichletDofs);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX0.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        tDirichletValues(aIndex) = tValueToSet;
        tDirichletDofs(aIndex) = tDirichletIndicesBoundaryX0(aIndex);
    }, "set dirichlet values and indices");

    auto tOffset = tDirichletIndicesBoundaryX0.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryY0.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryY0(aIndex);
    }, "set dirichlet values and indices");

    tValueToSet = 2e-3;
    tOffset += tDirichletIndicesBoundaryY0.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX1.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryX1(aIndex);
    }, "set dirichlet values and indices");
    tPlasticityProblem.setEssentialBoundaryConditions(tDirichletDofs, tDirichletValues);

    // 4. Evaluate Objective Function
    auto tNumVertices = tMesh->nverts();
    Plato::ScalarVector tControls("Controls", tNumVertices);
    Plato::fill(1.0, tControls);

    constexpr Plato::Scalar tTolerance = 1e-4;
    auto tSolution = tPlasticityProblem.solution(tControls);
    auto tObjValue = tPlasticityProblem.objectiveValue(tControls, tSolution);
    TEST_FLOATING_EQUALITY(tObjValue, 0.0, tTolerance);

    auto tObjGrad = tPlasticityProblem.objectiveGradient(tControls, tSolution);
    Plato::print(tObjGrad, "tObjGrad");
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_TestObjectiveGradientZ_2D)
{
    // 1. DEFINE PROBLEM
    constexpr Plato::OrdinalType tSpaceDim = 2;
    constexpr Plato::OrdinalType tMeshWidth = 6;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
      "<ParameterList name='Plato Problem'>                                                     \n"
      "  <Parameter name='Physics'          type='string'  value='Mechanical'/>                 \n"
      "  <Parameter name='PDE Constraint'   type='string'  value='Infinite Strain Plasticity'/> \n"
      "  <Parameter name='Objective'         type='string'  value='My Maximize Plastic Work'/>  \n"
      "  <ParameterList name='Material Model'>                                                  \n"
      "    <ParameterList name='Isotropic Linear Elastic'>                                      \n"
      "      <Parameter  name='Density' type='double' value='1000'/>                            \n"
      "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                      \n"
      "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                    \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Plasticity Model'>                                                \n"
      "    <ParameterList name='J2 Plasticity'>                                                 \n"
      "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>              \n"
      "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>   \n"
      "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/>  \n"
      "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/> \n"
      "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/>  \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Infinite Strain Plasticity'>                                      \n"
      "    <ParameterList name='Penalty Function'>                                              \n"
      "      <Parameter name='Type' type='string' value='SIMP'/>                                \n"
      "      <Parameter name='Exponent' type='double' value='3.0'/>                             \n"
      "      <Parameter name='Minimum Value' type='double' value='1.0e-6'/>                     \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='My Maximize Plastic Work'>                                        \n"
      "    <Parameter name='Type'                 type='string' value='Scalar Function'/>       \n"
      "    <Parameter name='Scalar Function Type' type='string' value='Maximize Plastic Work'/> \n"
      "    <Parameter name='Exponent'             type='double' value='3.0'/>                   \n"
      "    <Parameter name='Minimum Value'        type='double' value='1.0e-9'/>                \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Time Stepping'>                                                   \n"
      "    <Parameter name='Initial Num. Pseudo Time Steps' type='int' value='4'/>              \n"
      "    <Parameter name='Maximum Num. Pseudo Time Steps' type='int' value='4'/>              \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Newton-Raphson'>                                                  \n"
      "    <Parameter name='Stop Measure' type='string' value='residual'/>                      \n"
      "    <Parameter name='Maximum Number Iterations' type='int' value='20'/>                  \n"
      "  </ParameterList>                                                                       \n"
      "</ParameterList>                                                                         \n"
    );

    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;
    Plato::PlasticityProblem<PhysicsT> tPlasticityProblem(*tMesh, tMeshSets, *tParamList);

    // 2. Get Dirichlet Boundary Conditions
    Plato::OrdinalType tDispDofX = 0;
    Plato::OrdinalType tDispDofY = 1;
    constexpr Plato::OrdinalType tNumDofsPerNode = PhysicsT::mNumDofsPerNode;
    auto tDirichletIndicesBoundaryX0 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "x0", tNumDofsPerNode, tDispDofX);
    auto tDirichletIndicesBoundaryY0 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "y0", tNumDofsPerNode, tDispDofY);
    auto tDirichletIndicesBoundaryX1 = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_2D(*tMesh, "x1", tNumDofsPerNode, tDispDofX);

    // 3. Set Dirichlet Boundary Conditions
    Plato::Scalar tValueToSet = 0;
    auto tNumDirichletDofs = tDirichletIndicesBoundaryX0.size() + tDirichletIndicesBoundaryY0.size() + tDirichletIndicesBoundaryX1.size();
    Plato::ScalarVector tDirichletValues("Dirichlet Values", tNumDirichletDofs);
    Plato::LocalOrdinalVector tDirichletDofs("Dirichlet Dofs", tNumDirichletDofs);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX0.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        tDirichletValues(aIndex) = tValueToSet;
        tDirichletDofs(aIndex) = tDirichletIndicesBoundaryX0(aIndex);
    }, "set dirichlet values and indices");

    auto tOffset = tDirichletIndicesBoundaryX0.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryY0.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryY0(aIndex);
    }, "set dirichlet values and indices");

    tValueToSet = 2e-3;
    tOffset += tDirichletIndicesBoundaryY0.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX1.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryX1(aIndex);
    }, "set dirichlet values and indices");
    tPlasticityProblem.setEssentialBoundaryConditions(tDirichletDofs, tDirichletValues);

    // 4. TEST PARTIAL DERIVATIVE
    auto tApproxError = Plato::test_objective_grad_wrt_control(tPlasticityProblem, *tMesh);
    const Plato::Scalar tUpperBound = 1e-6;
    TEST_ASSERT(tApproxError < tUpperBound);
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_ObjectiveTest_3D)
{
    // 1. DEFINE PROBLEM
    constexpr Plato::OrdinalType tSpaceDim = 3;
    constexpr Plato::OrdinalType tMeshWidth = 2;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
      "<ParameterList name='Plato Problem'>                                                     \n"
      "  <Parameter name='Physics'          type='string'  value='Mechanical'/>                 \n"
      "  <Parameter name='PDE Constraint'   type='string'  value='Infinite Strain Plasticity'/> \n"
      "  <Parameter name='Objective'         type='string'  value='My Maximize Plastic Work'/>  \n"
      "  <ParameterList name='Material Model'>                                                  \n"
      "    <ParameterList name='Isotropic Linear Elastic'>                                      \n"
      "      <Parameter  name='Density' type='double' value='1000'/>                            \n"
      "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                      \n"
      "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                    \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Plasticity Model'>                                                \n"
      "    <ParameterList name='J2 Plasticity'>                                                 \n"
      "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>              \n"
      "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>   \n"
      "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/>  \n"
      "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/> \n"
      "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/>  \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Infinite Strain Plasticity'>                                      \n"
      "    <ParameterList name='Penalty Function'>                                              \n"
      "      <Parameter name='Type' type='string' value='SIMP'/>                                \n"
      "      <Parameter name='Exponent' type='double' value='3.0'/>                             \n"
      "      <Parameter name='Minimum Value' type='double' value='1.0e-6'/>                     \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='My Maximize Plastic Work'>                                        \n"
      "    <Parameter name='Type'                 type='string' value='Scalar Function'/>       \n"
      "    <Parameter name='Scalar Function Type' type='string' value='Maximize Plastic Work'/> \n"
      "    <Parameter name='Exponent'             type='double' value='3.0'/>                   \n"
      "    <Parameter name='Minimum Value'        type='double' value='1.0e-9'/>                \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Time Stepping'>                                                   \n"
      "    <Parameter name='Initial Num. Pseudo Time Steps' type='int' value='4'/>              \n"
      "    <Parameter name='Maximum Num. Pseudo Time Steps' type='int' value='4'/>              \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Newton-Raphson'>                                                  \n"
      "    <Parameter name='Stop Measure' type='string' value='residual'/>                      \n"
      "    <Parameter name='Stopping Tolerance' type='double' value='1e-10'/>                   \n"
      "    <Parameter name='Maximum Number Iterations' type='int' value='20'/>                  \n"
      "  </ParameterList>                                                                       \n"
      "</ParameterList>                                                                         \n"
    );

    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;
    Plato::PlasticityProblem<PhysicsT> tPlasticityProblem(*tMesh, tMeshSets, *tParamList);

    // 2. Get Dirichlet Boundary Conditions
    Plato::OrdinalType tDispDofX = 0;
    Plato::OrdinalType tDispDofY = 1;
    Plato::OrdinalType tDispDofZ = 2;
    constexpr Plato::OrdinalType tNumDofsPerNode = PhysicsT::mNumDofsPerNode;
    auto tDirichletIndicesBoundaryX0_Xdof = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_3D(*tMesh, "x0", tNumDofsPerNode, tDispDofX);
    auto tDirichletIndicesBoundaryX1_Xdof = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_3D(*tMesh, "x1", tNumDofsPerNode, tDispDofX);
    auto tDirichletIndicesBoundaryY0_Ydof = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_3D(*tMesh, "y0", tNumDofsPerNode, tDispDofY);
    auto tDirichletIndicesBoundaryY1_Ydof = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_3D(*tMesh, "y1", tNumDofsPerNode, tDispDofY);
    auto tDirichletIndicesBoundaryZ0_Zdof = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_3D(*tMesh, "z0", tNumDofsPerNode, tDispDofZ);

    // 3. Set Dirichlet Boundary Conditions
    Plato::Scalar tValueToSet = 0;
    auto tNumDirichletDofs = tDirichletIndicesBoundaryX0_Xdof.size() + tDirichletIndicesBoundaryX1_Xdof.size()
        + tDirichletIndicesBoundaryY0_Ydof.size() + tDirichletIndicesBoundaryY1_Ydof.size() + tDirichletIndicesBoundaryZ0_Zdof.size();
    Plato::ScalarVector tDirichletValues("Dirichlet Values", tNumDirichletDofs);
    Plato::LocalOrdinalVector tDirichletDofs("Dirichlet Dofs", tNumDirichletDofs);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX0_Xdof.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        tDirichletValues(aIndex) = tValueToSet;
        tDirichletDofs(aIndex) = tDirichletIndicesBoundaryX0_Xdof(aIndex);
    }, "set dirichlet values and indices");

    auto tOffset = tDirichletIndicesBoundaryX0_Xdof.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryY0_Ydof.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryY0_Ydof(aIndex);
    }, "set dirichlet values and indices");

    tOffset += tDirichletIndicesBoundaryY0_Ydof.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryY1_Ydof.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryY1_Ydof(aIndex);
    }, "set dirichlet values and indices");

    tOffset += tDirichletIndicesBoundaryY1_Ydof.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryZ0_Zdof.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryZ0_Zdof(aIndex);
    }, "set dirichlet values and indices");

    tValueToSet = 2e-3;
    tOffset += tDirichletIndicesBoundaryZ0_Zdof.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX1_Xdof.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryX1_Xdof(aIndex);
    }, "set dirichlet values and indices");
    tPlasticityProblem.setEssentialBoundaryConditions(tDirichletDofs, tDirichletValues);

    // 4. Evaluate Objective Function
    auto tNumVertices = tMesh->nverts();
    Plato::ScalarVector tControls("Controls", tNumVertices);
    Plato::fill(1.0, tControls);

    constexpr Plato::Scalar tTolerance = 1e-4;
    auto tSolution = tPlasticityProblem.solution(tControls);
    auto tObjValue = tPlasticityProblem.objectiveValue(tControls, tSolution);
    TEST_FLOATING_EQUALITY(tObjValue, -5.394823e-01, tTolerance);

    auto tObjGrad = tPlasticityProblem.objectiveGradient(tControls, tSolution);
    std::vector<Plato::Scalar> tGold = 
        {-8.694180e-02, -1.159224e-01, -2.898060e-02, -1.738836e-01, -5.796120e-02, -2.898060e-02, -5.796120e-02, -2.898060e-02, -1.159224e-01,
         -1.738836e-01, -5.796120e-02, -2.898060e-02, -5.796120e-02, -3.477672e-01, -1.738836e-01, -1.159224e-01, -1.738836e-01, -1.159224e-01,
         -8.694180e-02, -1.159224e-01, -1.738836e-01, -1.738836e-01, -5.796120e-02, -2.898060e-02, -5.796120e-02, -1.159224e-01, -2.898060e-02  };
    auto tHostGrad = Kokkos::create_mirror(tObjGrad);
    Kokkos::deep_copy(tHostGrad, tObjGrad);
    TEST_ASSERT( tHostGrad.size() == static_cast<Plato::OrdinalType>(tGold.size() ));
    for(Plato::OrdinalType tIndex = 0; tIndex < tHostGrad.size(); tIndex++)
    {
        TEST_FLOATING_EQUALITY(tHostGrad(tIndex), tGold[tIndex], tTolerance);
    }
}


TEUCHOS_UNIT_TEST(PlatoAnalyzeUnitTests, ElastoPlasticity_TestObjectiveGradientZ_3D)
{
    // 1. DEFINE PROBLEM
    constexpr Plato::OrdinalType tSpaceDim = 3;
    constexpr Plato::OrdinalType tMeshWidth = 6;
    auto tMesh = PlatoUtestHelpers::getBoxMesh(tSpaceDim, tMeshWidth);
    Plato::DataMap    tDataMap;
    Omega_h::MeshSets tMeshSets;

    Teuchos::RCP<Teuchos::ParameterList> tParamList =
    Teuchos::getParametersFromXmlString(
      "<ParameterList name='Plato Problem'>                                                     \n"
      "  <Parameter name='Physics'          type='string'  value='Mechanical'/>                 \n"
      "  <Parameter name='PDE Constraint'   type='string'  value='Infinite Strain Plasticity'/> \n"
      "  <Parameter name='Objective'         type='string'  value='My Maximize Plastic Work'/>  \n"
      "  <ParameterList name='Material Model'>                                                  \n"
      "    <ParameterList name='Isotropic Linear Elastic'>                                      \n"
      "      <Parameter  name='Density' type='double' value='1000'/>                            \n"
      "      <Parameter  name='Poissons Ratio' type='double' value='0.3'/>                      \n"
      "      <Parameter  name='Youngs Modulus' type='double' value='1.0e6'/>                    \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Plasticity Model'>                                                \n"
      "    <ParameterList name='J2 Plasticity'>                                                 \n"
      "      <Parameter  name='Hardening Modulus Isotropic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Hardening Modulus Kinematic' type='double' value='1.0e3'/>       \n"
      "      <Parameter  name='Initial Yield Stress' type='double' value='1.0e3'/>              \n"
      "      <Parameter  name='Elastic Properties Penalty Exponent' type='double' value='3'/>   \n"
      "      <Parameter  name='Elastic Properties Minimum Ersatz' type='double' value='1e-6'/>  \n"
      "      <Parameter  name='Plastic Properties Penalty Exponent' type='double' value='2.5'/> \n"
      "      <Parameter  name='Plastic Properties Minimum Ersatz' type='double' value='1e-9'/>  \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Infinite Strain Plasticity'>                                      \n"
      "    <ParameterList name='Penalty Function'>                                              \n"
      "      <Parameter name='Type' type='string' value='SIMP'/>                                \n"
      "      <Parameter name='Exponent' type='double' value='3.0'/>                             \n"
      "      <Parameter name='Minimum Value' type='double' value='1.0e-6'/>                     \n"
      "    </ParameterList>                                                                     \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='My Maximize Plastic Work'>                                        \n"
      "    <Parameter name='Type'                 type='string' value='Scalar Function'/>       \n"
      "    <Parameter name='Scalar Function Type' type='string' value='Maximize Plastic Work'/> \n"
      "    <Parameter name='Exponent'             type='double' value='3.0'/>                   \n"
      "    <Parameter name='Minimum Value'        type='double' value='1.0e-9'/>                \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Time Stepping'>                                                   \n"
      "    <Parameter name='Initial Num. Pseudo Time Steps' type='int' value='4'/>              \n"
      "    <Parameter name='Maximum Num. Pseudo Time Steps' type='int' value='4'/>              \n"
      "  </ParameterList>                                                                       \n"
      "  <ParameterList name='Newton-Raphson'>                                                  \n"
      "    <Parameter name='Stop Measure' type='string' value='residual'/>                      \n"
      "    <Parameter name='Stopping Tolerance' type='double' value='1e-10'/>                   \n"
      "    <Parameter name='Maximum Number Iterations' type='int' value='20'/>                  \n"
      "  </ParameterList>                                                                       \n"
      "</ParameterList>                                                                         \n"
    );

    using PhysicsT = Plato::InfinitesimalStrainPlasticity<tSpaceDim>;
    Plato::PlasticityProblem<PhysicsT> tPlasticityProblem(*tMesh, tMeshSets, *tParamList);

    // 2. Get Dirichlet Boundary Conditions
    Plato::OrdinalType tDispDofX = 0;
    Plato::OrdinalType tDispDofY = 1;
    Plato::OrdinalType tDispDofZ = 2;
    constexpr Plato::OrdinalType tNumDofsPerNode = PhysicsT::mNumDofsPerNode;
    auto tDirichletIndicesBoundaryX0_Xdof = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_3D(*tMesh, "x0", tNumDofsPerNode, tDispDofX);
    auto tDirichletIndicesBoundaryX1_Xdof = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_3D(*tMesh, "x1", tNumDofsPerNode, tDispDofX);
    auto tDirichletIndicesBoundaryY0_Ydof = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_3D(*tMesh, "y0", tNumDofsPerNode, tDispDofY);
    auto tDirichletIndicesBoundaryY1_Ydof = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_3D(*tMesh, "y1", tNumDofsPerNode, tDispDofY);
    auto tDirichletIndicesBoundaryZ0_Zdof = PlatoUtestHelpers::get_dirichlet_indices_on_boundary_3D(*tMesh, "z0", tNumDofsPerNode, tDispDofZ);

    // 3. Set Dirichlet Boundary Conditions
    Plato::Scalar tValueToSet = 0;
    auto tNumDirichletDofs = tDirichletIndicesBoundaryX0_Xdof.size() + tDirichletIndicesBoundaryX1_Xdof.size() 
        + tDirichletIndicesBoundaryY0_Ydof.size() + tDirichletIndicesBoundaryY1_Ydof.size() + tDirichletIndicesBoundaryZ0_Zdof.size();
    Plato::ScalarVector tDirichletValues("Dirichlet Values", tNumDirichletDofs);
    Plato::LocalOrdinalVector tDirichletDofs("Dirichlet Dofs", tNumDirichletDofs);
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX0_Xdof.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        tDirichletValues(aIndex) = tValueToSet;
        tDirichletDofs(aIndex) = tDirichletIndicesBoundaryX0_Xdof(aIndex);
    }, "set dirichlet values and indices");

    auto tOffset = tDirichletIndicesBoundaryX0_Xdof.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryY0_Ydof.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryY0_Ydof(aIndex);
    }, "set dirichlet values and indices");

    tOffset += tDirichletIndicesBoundaryY0_Ydof.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryY1_Ydof.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryY1_Ydof(aIndex);
    }, "set dirichlet values and indices");

    tOffset += tDirichletIndicesBoundaryY1_Ydof.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryZ0_Zdof.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryZ0_Zdof(aIndex);
    }, "set dirichlet values and indices");

    tValueToSet = 2e-3;
    tOffset += tDirichletIndicesBoundaryZ0_Zdof.size();
    Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tDirichletIndicesBoundaryX1_Xdof.size()), LAMBDA_EXPRESSION(const Plato::OrdinalType & aIndex)
    {
        auto tIndex = tOffset + aIndex;
        tDirichletValues(tIndex) = tValueToSet;
        tDirichletDofs(tIndex) = tDirichletIndicesBoundaryX1_Xdof(aIndex);
    }, "set dirichlet values and indices");
    tPlasticityProblem.setEssentialBoundaryConditions(tDirichletDofs, tDirichletValues);

    // 4. TEST PARTIAL DERIVATIVE
    auto tApproxError = Plato::test_objective_grad_wrt_control(tPlasticityProblem, *tMesh);
    constexpr Plato::Scalar tUpperBound = 1e-6;
    TEST_ASSERT(tApproxError < tUpperBound);
}


}
