#pragma once

#include <memory>
#include <string>
#include <vector>

#include <Omega_h_mesh.hpp>
#include <Omega_h_assoc.hpp>

#include <Teuchos_ParameterList.hpp>

#include "WorksetBase.hpp"
#include "AbstractLocalVectorFunctionInc.hpp"
#include "SimplexFadTypes.hpp"

namespace Plato
{

/******************************************************************************/
/*! local vector function class

   This class takes as a template argument a vector function in the form:

   H = H(U^k, U^{k-1}, C^k, C^{k-1}, X)

   and manages the evaluation of the function and derivatives wrt global state, U^k; 
   previous global state, U^{k-1}; local state, C^k; 
   previous local state, C^{k-1}; and control, X.
*/
/******************************************************************************/
template<typename PhysicsT>
class LocalVectorFunctionInc
{
private:
    static constexpr auto mNumGlobalDofsPerCell = PhysicsT::mNumDofsPerCell;
    static constexpr auto mNumLocalDofsPerCell = PhysicsT::mNumLocalDofsPerCell;
    static constexpr auto mNumNodesPerCell = PhysicsT::mNumNodesPerCell;
    static constexpr auto mNumDofsPerNode = PhysicsT::mNumDofsPerNode;
    static constexpr auto mNumSpatialDims = PhysicsT::mNumSpatialDims;
    static constexpr auto mNumControl = PhysicsT::mNumControl;

    const Plato::OrdinalType mNumNodes;
    const Plato::OrdinalType mNumCells;

    using Residual        = typename Plato::Evaluation<PhysicsT>::Residual;
    using GlobalJacobian  = typename Plato::Evaluation<PhysicsT>::Jacobian;
    using GlobalJacobianP = typename Plato::Evaluation<PhysicsT>::JacobianP;
    using LocalJacobian   = typename Plato::Evaluation<PhysicsT>::LocalJacobian;
    using LocalJacobianP  = typename Plato::Evaluation<PhysicsT>::LocalJacobianP;
    using GradientX       = typename Plato::Evaluation<PhysicsT>::GradientX;
    using GradientZ       = typename Plato::Evaluation<PhysicsT>::GradientZ;

    static constexpr auto mNumConfigDofsPerCell = mNumSpatialDims * mNumNodesPerCell;

    Plato::WorksetBase<PhysicsT> mWorksetBase;

    std::shared_ptr<Plato::AbstractLocalVectorFunctionInc<Residual>>        mLocalVectorFunctionResidual;
    std::shared_ptr<Plato::AbstractLocalVectorFunctionInc<GlobalJacobian>>  mLocalVectorFunctionJacobianU;
    std::shared_ptr<Plato::AbstractLocalVectorFunctionInc<GlobalJacobianP>> mLocalVectorFunctionJacobianUP;
    std::shared_ptr<Plato::AbstractLocalVectorFunctionInc<LocalJacobian>>   mLocalVectorFunctionJacobianC;
    std::shared_ptr<Plato::AbstractLocalVectorFunctionInc<LocalJacobianP>>  mLocalVectorFunctionJacobianCP;
    std::shared_ptr<Plato::AbstractLocalVectorFunctionInc<GradientX>>       mLocalVectorFunctionJacobianX;
    std::shared_ptr<Plato::AbstractLocalVectorFunctionInc<GradientZ>>       mLocalVectorFunctionJacobianZ;

    Plato::DataMap& mDataMap;

public:

    /**************************************************************************//**
    *
    * \brief Constructor
    * \param [in] aMesh mesh data base
    * \param [in] aMeshSets mesh sets data base
    * \param [in] aDataMap problem-specific data map
    * \param [in] aParamList Teuchos parameter list with input data
    *
    ******************************************************************************/
    LocalVectorFunctionInc(Omega_h::Mesh& aMesh,
                           Omega_h::MeshSets& aMeshSets,
                           Plato::DataMap& aDataMap,
                           Teuchos::ParameterList& aParamList) :
                           mWorksetBase(aMesh),
                           mNumCells(aMesh.nelems()),
                           mNumNodes(aMesh.nverts()),
                           mDataMap(aDataMap)
    {
      typename PhysicsT::FunctionFactory tFunctionFactory;

      mLocalVectorFunctionResidual  = tFunctionFactory.template createLocalVectorFunctionInc<Residual>
                                                                (aMesh, aMeshSets, aDataMap, aParamList);

      mLocalVectorFunctionJacobianU = tFunctionFactory.template createLocalVectorFunctionInc<GlobalJacobian>
                                                                (aMesh, aMeshSets, aDataMap, aParamList);

      mLocalVectorFunctionJacobianUP = tFunctionFactory.template createLocalVectorFunctionInc<GlobalJacobianP>
                                                                (aMesh, aMeshSets, aDataMap, aParamList);

      mLocalVectorFunctionJacobianC = tFunctionFactory.template createLocalVectorFunctionInc<LocalJacobian>
                                                                (aMesh, aMeshSets, aDataMap, aParamList);

      mLocalVectorFunctionJacobianCP = tFunctionFactory.template createLocalVectorFunctionInc<LocalJacobianP>
                                                                (aMesh, aMeshSets, aDataMap, aParamList);

      mLocalVectorFunctionJacobianZ = tFunctionFactory.template createLocalVectorFunctionInc<GradientZ>
                                                                (aMesh, aMeshSets, aDataMap, aParamList);

      mLocalVectorFunctionJacobianX = tFunctionFactory.template createLocalVectorFunctionInc<GradientX>
                                                                (aMesh, aMeshSets, aDataMap, aParamList);
    }

    /**************************************************************************//**
    *
    * \brief Constructor
    * \param [in] aMesh mesh data base
    * \param [in] aDataMap problem-specific data map
    *
    ******************************************************************************/
    LocalVectorFunctionInc(Omega_h::Mesh& aMesh, Plato::DataMap& aDataMap) :
                           mWorksetBase(aMesh),
                           mNumCells(aMesh.nelems()),
                           mNumNodes(aMesh.nverts()),
                           mLocalVectorFunctionResidual(),
                           mLocalVectorFunctionJacobianU(),
                           mLocalVectorFunctionJacobianUP(),
                           mLocalVectorFunctionJacobianC(),
                           mLocalVectorFunctionJacobianCP(),
                           mLocalVectorFunctionJacobianX(),
                           mLocalVectorFunctionJacobianZ(),
                           mDataMap(aDataMap)
    {
    }

    /**************************************************************************//**
    *
    * \brief Return total number of local degrees of freedom
    *
    ******************************************************************************/
    Plato::OrdinalType size() const
    {
      return mNumCells * mNumLocalDofsPerCell;
    }

    /**************************************************************************//**
     *
     * \brief Return total number of nodes
     * \return total number of nodes
     *
     ******************************************************************************/
    decltype(mNumNodes) numNodes() const
    {
        return mNumNodes;
    }

    /**************************************************************************//**
     *
     * \brief Return total number of cells
     * \return total number of cells
     *
     ******************************************************************************/
    decltype(mNumCells) numCells() const
    {
        return mNumCells;
    }

    /***********************************************************************//**
     * \brief Return number of spatial dimensions.
     * \return number of spatial dimensions
    ***************************************************************************/
    decltype(mNumSpatialDims) numSpatialDims() const
    {
        return mNumSpatialDims;
    }

    /***********************************************************************//**
     * \brief Return number of nodes per cell.
     * \return number of nodes per cell
    ***************************************************************************/
    decltype(mNumNodesPerCell) numNodesPerCell() const
    {
        return mNumNodesPerCell;
    }

    /***********************************************************************//**
     * \brief Return number of global degrees of freedom per node.
     * \return number of global degrees of freedom per node
    ***************************************************************************/
    decltype(mNumDofsPerNode) numGlobalDofsPerNode() const
    {
        return mNumDofsPerNode;
    }

    /***********************************************************************//**
     * \brief Return number of global degrees of freedom per cell.
     * \return number of global degrees of freedom per cell
    ***************************************************************************/
    decltype(mNumGlobalDofsPerCell) numGlobalDofsPerCell() const
    {
        return mNumGlobalDofsPerCell;
    }

    /***********************************************************************//**
     * \brief Return number of local degrees of freedom per cell.
     * \return number of local degrees of freedom per cell
    ***************************************************************************/
    decltype(mNumLocalDofsPerCell) numLocalDofsPerCell() const
    {
        return mNumLocalDofsPerCell;
    }

    /**************************************************************************//**
    *
    * \brief Return state names
    *
    ******************************************************************************/
    std::vector<std::string> getDofNames() const
    {
      return mLocalVectorFunctionResidual->getDofNames();
    }

    /**************************************************************************//**
    *
    * \brief Allocate residual evaluator
    * \param [in] aResidual residual evaluator
    *
    ******************************************************************************/
    void allocateResidual(const std::shared_ptr<Plato::AbstractLocalVectorFunctionInc<Residual>>& aResidual)
    {
        mLocalVectorFunctionResidual = aResidual;
    }

    /**************************************************************************//**
    *
    * \brief Allocate global jacobian evaluator
    * \param [in] aJacobian global jacobian evaluator
    *
    ******************************************************************************/
    void allocateJacobianU(const std::shared_ptr<Plato::AbstractLocalVectorFunctionInc<GlobalJacobian>>& aJacobian)
    {
        mLocalVectorFunctionJacobianU = aJacobian;
    }

    /**************************************************************************//**
    *
    * \brief Allocate previous global jacobian evaluator
    * \param [in] aJacobian previous global jacobian evaluator
    *
    ******************************************************************************/
    void allocateJacobianUP(const std::shared_ptr<Plato::AbstractLocalVectorFunctionInc<GlobalJacobianP>>& aJacobian)
    {
        mLocalVectorFunctionJacobianUP = aJacobian;
    }

    /**************************************************************************//**
    *
    * \brief Allocate local jacobian evaluator
    * \param [in] aJacobian local jacobian evaluator
    *
    ******************************************************************************/
    void allocateJacobianC(const std::shared_ptr<Plato::AbstractLocalVectorFunctionInc<LocalJacobian>>& aJacobian)
    {
        mLocalVectorFunctionJacobianC = aJacobian;
    }

    /**************************************************************************//**
    *
    * \brief Allocate previous local jacobian evaluator
    * \param [in] aJacobian previous local jacobian evaluator
    *
    ******************************************************************************/
    void allocateJacobianCP(const std::shared_ptr<Plato::AbstractLocalVectorFunctionInc<LocalJacobianP>>& aJacobian)
    {
        mLocalVectorFunctionJacobianCP = aJacobian;
    }

    /**************************************************************************//**
    *
    * \brief Allocate partial derivative with respect to control evaluator
    * \param [in] aGradientZ partial derivative with respect to control evaluator
    *
    ******************************************************************************/
    void allocateJacobianZ(const std::shared_ptr<Plato::AbstractLocalVectorFunctionInc<GradientZ>>& aGradientZ)
    {
        mLocalVectorFunctionJacobianZ = aGradientZ; 
    }

    /**************************************************************************//**
    *
    * \brief Allocate partial derivative with respect to configuration evaluator
    * \param [in] GradientX partial derivative with respect to configuration evaluator
    *
    ******************************************************************************/
    void allocateJacobianX(const std::shared_ptr<Plato::AbstractLocalVectorFunctionInc<GradientX>>& aGradientX)
    {
        mLocalVectorFunctionJacobianX = aGradientX; 
    }

    /**************************************************************************//**
    * \brief Update the local state variables
    * \param [in]  aGlobalState global state at current time step
    * \param [in]  aPrevGlobalState global state at previous time step
    * \param [out] aLocalState local state at current time step
    * \param [in]  aPrevLocalState local state at previous time step
    * \param [in]  aControl control parameters
    * \param [in]  aTimeStep time step
    ******************************************************************************/
    void
    updateLocalState(const Plato::ScalarVector & aGlobalState,
                     const Plato::ScalarVector & aPrevGlobalState,
                     const Plato::ScalarVector & aLocalState,
                     const Plato::ScalarVector & aPrevLocalState,
                     const Plato::ScalarVector & aControl,
                     Plato::Scalar aTimeStep = 0.0) const
    {
      using ConfigScalar         = typename Residual::ConfigScalarType;
      using StateScalar          = typename Residual::StateScalarType;
      using PrevStateScalar      = typename Residual::PrevStateScalarType;
      using LocalStateScalar     = typename Residual::LocalStateScalarType;
      using PrevLocalStateScalar = typename Residual::PrevLocalStateScalarType;
      using ControlScalar        = typename Residual::ControlScalarType;
      using ResultScalar         = typename Residual::ResultScalarType;

      // Workset global state
      //
      Plato::ScalarMultiVectorT<StateScalar> tGlobalStateWS("Global State Workset", mNumCells, mNumGlobalDofsPerCell);
      mWorksetBase.worksetState(aGlobalState, tGlobalStateWS);

      // Workset prev global state
      //
      Plato::ScalarMultiVectorT<PrevStateScalar> tPrevGlobalStateWS("Prev Global State Workset", mNumCells, mNumGlobalDofsPerCell);
      mWorksetBase.worksetState(aPrevGlobalState, tPrevGlobalStateWS);

      // Workset local state
      //
      Plato::ScalarMultiVectorT<LocalStateScalar> tLocalStateWS("Local State Workset", mNumCells, mNumLocalDofsPerCell);
      mWorksetBase.worksetLocalState(aLocalState, tLocalStateWS);

      // Workset prev local state
      //
      Plato::ScalarMultiVectorT<PrevLocalStateScalar> tPrevLocalStateWS("Prev Local State Workset", mNumCells, mNumLocalDofsPerCell);
      mWorksetBase.worksetLocalState(aPrevLocalState, tPrevLocalStateWS);

      // Workset control
      //
      Plato::ScalarMultiVectorT<ControlScalar> tControlWS("Control Workset", mNumCells, mNumNodesPerCell);
      mWorksetBase.worksetControl(aControl, tControlWS);

      // Workset config
      // 
      Plato::ScalarArray3DT<ConfigScalar> tConfigWS("Config Workset", mNumCells, mNumNodesPerCell, mNumSpatialDims);
      mWorksetBase.worksetConfig(tConfigWS);

      // update the local state variables
      //
      mLocalVectorFunctionResidual->updateLocalState( tGlobalStateWS, tPrevGlobalStateWS, 
                                                      tLocalStateWS , tPrevLocalStateWS,
                                                      tControlWS    , tConfigWS, 
                                                      aTimeStep );

      auto tNumCells = mNumCells;
      Plato::flatten_vector_workset<mNumLocalDofsPerCell>(tNumCells, tLocalStateWS, aLocalState);
    }

    /**************************************************************************//**
    * \brief Compute the local residual vector
    * \param [in]  aGlobalState global state at current time step
    * \param [in]  aPrevGlobalState global state at previous time step
    * \param [in]  aLocalState local state at current time step
    * \param [in]  aPrevLocalState local state at previous time step
    * \param [in]  aControl control parameters
    * \param [in]  aTimeStep time step
    * \return local residual vector
    ******************************************************************************/
    Plato::ScalarVectorT<typename Residual::ResultScalarType>
    value(const Plato::ScalarVector & aGlobalState,
          const Plato::ScalarVector & aPrevGlobalState,
          const Plato::ScalarVector & aLocalState,
          const Plato::ScalarVector & aPrevLocalState,
          const Plato::ScalarVector & aControl,
          Plato::Scalar aTimeStep = 0.0) const
    {
        using ConfigScalar         = typename Residual::ConfigScalarType;
        using StateScalar          = typename Residual::StateScalarType;
        using PrevStateScalar      = typename Residual::PrevStateScalarType;
        using LocalStateScalar     = typename Residual::LocalStateScalarType;
        using PrevLocalStateScalar = typename Residual::PrevLocalStateScalarType;
        using ControlScalar        = typename Residual::ControlScalarType;
        using ResultScalar         = typename Residual::ResultScalarType;

        // Workset config
        //
        Plato::ScalarArray3DT<ConfigScalar>
            tConfigWS("Config Workset", mNumCells, mNumNodesPerCell, mNumSpatialDims);
        mWorksetBase.worksetConfig(tConfigWS);

        // Workset global state
        //
        Plato::ScalarMultiVectorT<StateScalar> tGlobalStateWS("State Workset", mNumCells, mNumGlobalDofsPerCell);
        mWorksetBase.worksetState(aGlobalState, tGlobalStateWS);

        // Workset prev global state
        //
        Plato::ScalarMultiVectorT<PrevStateScalar> tPrevGlobalStateWS("Prev State Workset", mNumCells, mNumGlobalDofsPerCell);
        mWorksetBase.worksetState(aPrevGlobalState, tPrevGlobalStateWS);

        // Workset local state
        //
        Plato::ScalarMultiVectorT<LocalStateScalar>
                                 tLocalStateWS("Local State Workset", mNumCells, mNumLocalDofsPerCell);
        mWorksetBase.worksetLocalState(aLocalState, tLocalStateWS);

        // Workset prev local state
        //
        Plato::ScalarMultiVectorT<PrevLocalStateScalar>
                                 tPrevLocalStateWS("Prev Local State Workset", mNumCells, mNumLocalDofsPerCell);
        mWorksetBase.worksetLocalState(aPrevLocalState, tPrevLocalStateWS);

        // Workset control
        //
        Plato::ScalarMultiVectorT<ControlScalar> tControlWS("Control Workset", mNumCells, mNumNodesPerCell);
        mWorksetBase.worksetControl(aControl, tControlWS);

        // create return view
        //
        Plato::ScalarMultiVectorT<ResultScalar> tResidualWS("Residual", mNumCells, mNumLocalDofsPerCell);

        // evaluate function
        //
        mLocalVectorFunctionResidual->evaluate(tGlobalStateWS, tPrevGlobalStateWS,
                                               tLocalStateWS,  tPrevLocalStateWS,
                                               tControlWS, tConfigWS, tResidualWS, aTimeStep);

        const auto tNumCells = this->numCells();
        const auto tTotalNumLocalDofs = mNumCells * mNumLocalDofsPerCell;
        Plato::ScalarVector tResidualVector("Residual Vector", tTotalNumLocalDofs);
        Plato::flatten_vector_workset<mNumLocalDofsPerCell>(tNumCells, tResidualWS, tResidualVector);

        return tResidualVector;
    }

    /**************************************************************************//**
    * \brief Compute the local residual workset
    * \param [in]  aGlobalState global state at current time step
    * \param [in]  aPrevGlobalState global state at previous time step
    * \param [in]  aLocalState local state at current time step
    * \param [in]  aPrevLocalState local state at previous time step
    * \param [in]  aControl control parameters
    * \param [in]  aTimeStep time step
    * \return local residual workset
    ******************************************************************************/
    Plato::ScalarMultiVectorT<typename Residual::ResultScalarType>
    valueWorkSet(const Plato::ScalarVector & aGlobalState,
                 const Plato::ScalarVector & aPrevGlobalState,
                 const Plato::ScalarVector & aLocalState,
                 const Plato::ScalarVector & aPrevLocalState,
                 const Plato::ScalarVector & aControl,
                 Plato::Scalar aTimeStep = 0.0) const
    {
        using ConfigScalar         = typename Residual::ConfigScalarType;
        using StateScalar          = typename Residual::StateScalarType;
        using PrevStateScalar      = typename Residual::PrevStateScalarType;
        using LocalStateScalar     = typename Residual::LocalStateScalarType;
        using PrevLocalStateScalar = typename Residual::PrevLocalStateScalarType;
        using ControlScalar        = typename Residual::ControlScalarType;
        using ResultScalar         = typename Residual::ResultScalarType;

        // Workset config
        //
        Plato::ScalarArray3DT<ConfigScalar>
            tConfigWS("Config Workset", mNumCells, mNumNodesPerCell, mNumSpatialDims);
        mWorksetBase.worksetConfig(tConfigWS);

        // Workset global state
        //
        Plato::ScalarMultiVectorT<StateScalar> tGlobalStateWS("State Workset", mNumCells, mNumGlobalDofsPerCell);
        mWorksetBase.worksetState(aGlobalState, tGlobalStateWS);

        // Workset prev global state
        //
        Plato::ScalarMultiVectorT<PrevStateScalar> tPrevGlobalStateWS("Prev State Workset", mNumCells, mNumGlobalDofsPerCell);
        mWorksetBase.worksetState(aPrevGlobalState, tPrevGlobalStateWS);

        // Workset local state
        //
        Plato::ScalarMultiVectorT<LocalStateScalar>
                                 tLocalStateWS("Local State Workset", mNumCells, mNumLocalDofsPerCell);
        mWorksetBase.worksetLocalState(aLocalState, tLocalStateWS);

        // Workset prev local state
        //
        Plato::ScalarMultiVectorT<PrevLocalStateScalar>
                                 tPrevLocalStateWS("Prev Local State Workset", mNumCells, mNumLocalDofsPerCell);
        mWorksetBase.worksetLocalState(aPrevLocalState, tPrevLocalStateWS);

        // Workset control
        //
        Plato::ScalarMultiVectorT<ControlScalar> tControlWS("Control Workset", mNumCells, mNumNodesPerCell);
        mWorksetBase.worksetControl(aControl, tControlWS);

        // create return view
        //
        Plato::ScalarMultiVectorT<ResultScalar> tResidualWS("Residual", mNumCells, mNumLocalDofsPerCell);

        // evaluate function
        //
        mLocalVectorFunctionResidual->evaluate(tGlobalStateWS, tPrevGlobalStateWS,
                                               tLocalStateWS,  tPrevLocalStateWS,
                                               tControlWS, tConfigWS, tResidualWS, aTimeStep);

        return tResidualWS;
    }

    /**************************************************************************//**
    * \brief Compute the gradient wrt configuration of the local residual vector
    * \param [in]  aGlobalState global state at current time step
    * \param [in]  aPrevGlobalState global state at previous time step
    * \param [in]  aLocalState local state at current time step
    * \param [in]  aPrevLocalState local state at previous time step
    * \param [in]  aControl control parameters
    * \param [in]  aTimeStep time step
    * \return gradient wrt configuration of the local residual vector
    ******************************************************************************/
    Plato::ScalarArray3D
    gradient_x(const Plato::ScalarVector & aGlobalState,
               const Plato::ScalarVector & aPrevGlobalState,
               const Plato::ScalarVector & aLocalState,
               const Plato::ScalarVector & aPrevLocalState,
               const Plato::ScalarVector & aControl,
               Plato::Scalar aTimeStep = 0.0) const
    {
        using ConfigScalar         = typename GradientX::ConfigScalarType;
        using StateScalar          = typename GradientX::StateScalarType;
        using PrevStateScalar      = typename GradientX::PrevStateScalarType;
        using LocalStateScalar     = typename GradientX::LocalStateScalarType;
        using PrevLocalStateScalar = typename GradientX::PrevLocalStateScalarType;
        using ControlScalar        = typename GradientX::ControlScalarType;
        using ResultScalar         = typename GradientX::ResultScalarType;

        // Workset config
        //
        Plato::ScalarArray3DT<ConfigScalar>
            tConfigWS("Config Workset", mNumCells, mNumNodesPerCell, mNumSpatialDims);
        mWorksetBase.worksetConfig(tConfigWS);

        // Workset global state
        //
        Plato::ScalarMultiVectorT<StateScalar> tGlobalStateWS("State Workset", mNumCells, mNumGlobalDofsPerCell);
        mWorksetBase.worksetState(aGlobalState, tGlobalStateWS);

        // Workset prev global state
        //
        Plato::ScalarMultiVectorT<PrevStateScalar> tPrevGlobalStateWS("Prev State Workset", mNumCells, mNumGlobalDofsPerCell);
        mWorksetBase.worksetState(aPrevGlobalState, tPrevGlobalStateWS);

        // Workset local state
        //
        Plato::ScalarMultiVectorT<LocalStateScalar> 
                                 tLocalStateWS("Local State Workset", mNumCells, mNumLocalDofsPerCell);
        mWorksetBase.worksetLocalState(aLocalState, tLocalStateWS);

        // Workset prev local state
        //
        Plato::ScalarMultiVectorT<PrevLocalStateScalar> 
                                 tPrevLocalStateWS("Prev Local State Workset", mNumCells, mNumLocalDofsPerCell);
        mWorksetBase.worksetLocalState(aPrevLocalState, tPrevLocalStateWS);

        // Workset control
        //
        Plato::ScalarMultiVectorT<ControlScalar> tControlWS("Control Workset", mNumCells, mNumNodesPerCell);
        mWorksetBase.worksetControl(aControl, tControlWS);

        // create return view
        //
        Plato::ScalarMultiVectorT<ResultScalar> tJacobianWS("Jacobian Configuration Workset", mNumCells, mNumLocalDofsPerCell);

        // evaluate function
        //
        mLocalVectorFunctionJacobianX->evaluate(tGlobalStateWS, tPrevGlobalStateWS, 
                                                tLocalStateWS,  tPrevLocalStateWS, 
                                                tControlWS, tConfigWS, tJacobianWS, aTimeStep);

        constexpr auto tNumConfigDofsPerCell = mNumNodesPerCell * mNumSpatialDims;
        Plato::ScalarArray3D tOutputJacobian("Output Jacobian Configuration", mNumCells, mNumLocalDofsPerCell, tNumConfigDofsPerCell);
        Plato::transform_ad_type_to_pod_3Dview<mNumLocalDofsPerCell, tNumConfigDofsPerCell>(mNumCells, tJacobianWS, tOutputJacobian);

        return tOutputJacobian;
    }

    /**************************************************************************//**
    * \brief Compute the gradient wrt global state of the local residual vector
    * \param [in]  aGlobalState global state at current time step
    * \param [in]  aPrevGlobalState global state at previous time step
    * \param [in]  aLocalState local state at current time step
    * \param [in]  aPrevLocalState local state at previous time step
    * \param [in]  aControl control parameters
    * \param [in]  aTimeStep time step
    * \return gradient wrt global state of the local residual vector
    ******************************************************************************/
    ScalarArray3D
    gradient_u(const Plato::ScalarVector & aGlobalState,
               const Plato::ScalarVector & aPrevGlobalState,
               const Plato::ScalarVector & aLocalState,
               const Plato::ScalarVector & aPrevLocalState,
               const Plato::ScalarVector & aControl,
               Plato::Scalar aTimeStep = 0.0) const
    {
      using ConfigScalar         = typename GlobalJacobian::ConfigScalarType;
      using StateScalar          = typename GlobalJacobian::StateScalarType;
      using PrevStateScalar      = typename GlobalJacobian::PrevStateScalarType;
      using LocalStateScalar     = typename GlobalJacobian::LocalStateScalarType;
      using PrevLocalStateScalar = typename GlobalJacobian::PrevLocalStateScalarType;
      using ControlScalar        = typename GlobalJacobian::ControlScalarType;
      using ResultScalar         = typename GlobalJacobian::ResultScalarType;

      // Workset config
      //
      Plato::ScalarArray3DT<ConfigScalar>
          tConfigWS("Config Workset", mNumCells, mNumNodesPerCell, mNumSpatialDims);
      mWorksetBase.worksetConfig(tConfigWS);

      // Workset global state
      //
      Plato::ScalarMultiVectorT<StateScalar> tGlobalStateWS("State Workset", mNumCells, mNumGlobalDofsPerCell);
      mWorksetBase.worksetState(aGlobalState, tGlobalStateWS);

      // Workset prev global state
      //
      Plato::ScalarMultiVectorT<PrevStateScalar> tPrevGlobalStateWS("Prev State Workset", mNumCells, mNumGlobalDofsPerCell);
      mWorksetBase.worksetState(aPrevGlobalState, tPrevGlobalStateWS);

      // Workset local state
      //
      Plato::ScalarMultiVectorT<LocalStateScalar> 
                               tLocalStateWS("Local State Workset", mNumCells, mNumLocalDofsPerCell);
      mWorksetBase.worksetLocalState(aLocalState, tLocalStateWS);

      // Workset prev local state
      //
      Plato::ScalarMultiVectorT<PrevLocalStateScalar> 
                               tPrevLocalStateWS("Prev Local State Workset", mNumCells, mNumLocalDofsPerCell);
      mWorksetBase.worksetLocalState(aPrevLocalState, tPrevLocalStateWS);

      // Workset control
      //
      Plato::ScalarMultiVectorT<ControlScalar> tControlWS("Control Workset", mNumCells, mNumNodesPerCell);
      mWorksetBase.worksetControl(aControl, tControlWS);

      // create return view
      //
      Plato::ScalarMultiVectorT<ResultScalar> tJacobianWS("Jacobian Current Global State Workset",mNumCells,mNumLocalDofsPerCell);

      // evaluate function
      //
      mLocalVectorFunctionJacobianU->evaluate(tGlobalStateWS, tPrevGlobalStateWS, 
                                              tLocalStateWS,  tPrevLocalStateWS, 
                                              tControlWS, tConfigWS, tJacobianWS, aTimeStep);

      Plato::ScalarArray3D tOutputJacobian("Output Jacobian Current Global State", mNumCells, mNumLocalDofsPerCell, mNumGlobalDofsPerCell);
      Plato::transform_ad_type_to_pod_3Dview<mNumLocalDofsPerCell, mNumGlobalDofsPerCell>(mNumCells, tJacobianWS, tOutputJacobian);

      return tOutputJacobian;
    }

    /**************************************************************************//**
    * \brief Compute the gradient wrt previous global state of the local residual vector
    * \param [in]  aGlobalState global state at current time step
    * \param [in]  aPrevGlobalState global state at previous time step
    * \param [in]  aLocalState local state at current time step
    * \param [in]  aPrevLocalState local state at previous time step
    * \param [in]  aControl control parameters
    * \param [in]  aTimeStep time step
    * \return gradient wrt previous global state of the local residual vector
    ******************************************************************************/
    Plato::ScalarArray3D
    gradient_up(const Plato::ScalarVector & aGlobalState,
               const Plato::ScalarVector & aPrevGlobalState,
               const Plato::ScalarVector & aLocalState,
               const Plato::ScalarVector & aPrevLocalState,
               const Plato::ScalarVector & aControl,
               Plato::Scalar aTimeStep = 0.0) const
    {
      using ConfigScalar         = typename GlobalJacobianP::ConfigScalarType;
      using StateScalar          = typename GlobalJacobianP::StateScalarType;
      using PrevStateScalar      = typename GlobalJacobianP::PrevStateScalarType;
      using LocalStateScalar     = typename GlobalJacobianP::LocalStateScalarType;
      using PrevLocalStateScalar = typename GlobalJacobianP::PrevLocalStateScalarType;
      using ControlScalar        = typename GlobalJacobianP::ControlScalarType;
      using ResultScalar         = typename GlobalJacobianP::ResultScalarType;

      // Workset config
      //
      Plato::ScalarArray3DT<ConfigScalar>
          tConfigWS("Config Workset", mNumCells, mNumNodesPerCell, mNumSpatialDims);
      mWorksetBase.worksetConfig(tConfigWS);

      // Workset global state
      //
      Plato::ScalarMultiVectorT<StateScalar> tGlobalStateWS("State Workset", mNumCells, mNumGlobalDofsPerCell);
      mWorksetBase.worksetState(aGlobalState, tGlobalStateWS);

      // Workset prev global state
      //
      Plato::ScalarMultiVectorT<PrevStateScalar> tPrevGlobalStateWS("Prev State Workset", mNumCells, mNumGlobalDofsPerCell);
      mWorksetBase.worksetState(aPrevGlobalState, tPrevGlobalStateWS);

      // Workset local state
      //
      Plato::ScalarMultiVectorT<LocalStateScalar> 
                               tLocalStateWS("Local State Workset", mNumCells, mNumLocalDofsPerCell);
      mWorksetBase.worksetLocalState(aLocalState, tLocalStateWS);

      // Workset prev local state
      //
      Plato::ScalarMultiVectorT<PrevLocalStateScalar> 
                               tPrevLocalStateWS("Prev Local State Workset", mNumCells, mNumLocalDofsPerCell);
      mWorksetBase.worksetLocalState(aPrevLocalState, tPrevLocalStateWS);

      // Workset control
      //
      Plato::ScalarMultiVectorT<ControlScalar> tControlWS("Control Workset", mNumCells, mNumNodesPerCell);
      mWorksetBase.worksetControl(aControl, tControlWS);

      // create return view
      //
      Plato::ScalarMultiVectorT<ResultScalar> tJacobianWS("Jacobian Previous Global State Workset",mNumCells,mNumLocalDofsPerCell);

      // evaluate function
      //
      mLocalVectorFunctionJacobianUP->evaluate(tGlobalStateWS, tPrevGlobalStateWS, 
                                              tLocalStateWS,  tPrevLocalStateWS, 
                                              tControlWS, tConfigWS, tJacobianWS, aTimeStep);

      Plato::ScalarArray3D tOutputJacobian("Output Jacobian Previous Global State", mNumCells, mNumLocalDofsPerCell, mNumGlobalDofsPerCell);
      Plato::transform_ad_type_to_pod_3Dview<mNumLocalDofsPerCell, mNumGlobalDofsPerCell>(mNumCells, tJacobianWS, tOutputJacobian);

      return tOutputJacobian;
    }

    /**************************************************************************//**
    * \brief Compute the gradient wrt local state of the local residual vector
    * \param [in]  aGlobalState global state at current time step
    * \param [in]  aPrevGlobalState global state at previous time step
    * \param [in]  aLocalState local state at current time step
    * \param [in]  aPrevLocalState local state at previous time step
    * \param [in]  aControl control parameters
    * \param [in]  aTimeStep time step
    * \return gradient wrt local state of the local residual vector
    ******************************************************************************/
    Plato::ScalarArray3D
    gradient_c(const Plato::ScalarVector & aGlobalState,
               const Plato::ScalarVector & aPrevGlobalState,
               const Plato::ScalarVector & aLocalState,
               const Plato::ScalarVector & aPrevLocalState,
               const Plato::ScalarVector & aControl,
               Plato::Scalar aTimeStep = 0.0) const
    {
      using ConfigScalar         = typename LocalJacobian::ConfigScalarType;
      using StateScalar          = typename LocalJacobian::StateScalarType;
      using PrevStateScalar      = typename LocalJacobian::PrevStateScalarType;
      using LocalStateScalar     = typename LocalJacobian::LocalStateScalarType;
      using PrevLocalStateScalar = typename LocalJacobian::PrevLocalStateScalarType;
      using ControlScalar        = typename LocalJacobian::ControlScalarType;
      using ResultScalar         = typename LocalJacobian::ResultScalarType;

      // Workset config
      //
      Plato::ScalarArray3DT<ConfigScalar>
          tConfigWS("Config Workset", mNumCells, mNumNodesPerCell, mNumSpatialDims);
      mWorksetBase.worksetConfig(tConfigWS);

      // Workset global state
      //
      Plato::ScalarMultiVectorT<StateScalar> tGlobalStateWS("State Workset", mNumCells, mNumGlobalDofsPerCell);
      mWorksetBase.worksetState(aGlobalState, tGlobalStateWS);

      // Workset prev global state
      //
      Plato::ScalarMultiVectorT<PrevStateScalar> tPrevGlobalStateWS("Prev State Workset", mNumCells, mNumGlobalDofsPerCell);
      mWorksetBase.worksetState(aPrevGlobalState, tPrevGlobalStateWS);

      // Workset local state
      //
      Plato::ScalarMultiVectorT<LocalStateScalar> 
                               tLocalStateWS("Local State Workset", mNumCells, mNumLocalDofsPerCell);
      mWorksetBase.worksetLocalState(aLocalState, tLocalStateWS);

      // Workset prev local state
      //
      Plato::ScalarMultiVectorT<PrevLocalStateScalar> 
                               tPrevLocalStateWS("Prev Local State Workset", mNumCells, mNumLocalDofsPerCell);
      mWorksetBase.worksetLocalState(aPrevLocalState, tPrevLocalStateWS);

      // Workset control
      //
      Plato::ScalarMultiVectorT<ControlScalar> tControlWS("Control Workset", mNumCells, mNumNodesPerCell);
      mWorksetBase.worksetControl(aControl, tControlWS);

      // create return view
      //
      Plato::ScalarMultiVectorT<ResultScalar> tJacobianWS("Jacobian Current Local State Workset",mNumCells,mNumLocalDofsPerCell);

      // evaluate function
      //
      mLocalVectorFunctionJacobianC->evaluate(tGlobalStateWS, tPrevGlobalStateWS, 
                                              tLocalStateWS,  tPrevLocalStateWS, 
                                              tControlWS, tConfigWS, tJacobianWS, aTimeStep);

      Plato::ScalarArray3D tOutputJacobian("Output Jacobian Current Local State", mNumCells, mNumLocalDofsPerCell, mNumLocalDofsPerCell);
      Plato::transform_ad_type_to_pod_3Dview<mNumLocalDofsPerCell, mNumLocalDofsPerCell>(mNumCells, tJacobianWS, tOutputJacobian);

      return tOutputJacobian;
    }

    /**************************************************************************//**
    * \brief Compute the gradient wrt previous local state of the local residual vector
    * \param [in]  aGlobalState global state at current time step
    * \param [in]  aPrevGlobalState global state at previous time step
    * \param [in]  aLocalState local state at current time step
    * \param [in]  aPrevLocalState local state at previous time step
    * \param [in]  aControl control parameters
    * \param [in]  aTimeStep time step
    * \return gradient wrt previous local state of the local residual vector
    ******************************************************************************/
    Plato::ScalarArray3D
    gradient_cp(const Plato::ScalarVector & aGlobalState,
               const Plato::ScalarVector & aPrevGlobalState,
               const Plato::ScalarVector & aLocalState,
               const Plato::ScalarVector & aPrevLocalState,
               const Plato::ScalarVector & aControl,
               Plato::Scalar aTimeStep = 0.0) const
    {
      using ConfigScalar         = typename LocalJacobianP::ConfigScalarType;
      using StateScalar          = typename LocalJacobianP::StateScalarType;
      using PrevStateScalar      = typename LocalJacobianP::PrevStateScalarType;
      using LocalStateScalar     = typename LocalJacobianP::LocalStateScalarType;
      using PrevLocalStateScalar = typename LocalJacobianP::PrevLocalStateScalarType;
      using ControlScalar        = typename LocalJacobianP::ControlScalarType;
      using ResultScalar         = typename LocalJacobianP::ResultScalarType;

      // Workset config
      //
      Plato::ScalarArray3DT<ConfigScalar>
          tConfigWS("Config Workset", mNumCells, mNumNodesPerCell, mNumSpatialDims);
      mWorksetBase.worksetConfig(tConfigWS);

      // Workset global state
      //
      Plato::ScalarMultiVectorT<StateScalar> tGlobalStateWS("State Workset", mNumCells, mNumGlobalDofsPerCell);
      mWorksetBase.worksetState(aGlobalState, tGlobalStateWS);

      // Workset prev global state
      //
      Plato::ScalarMultiVectorT<PrevStateScalar> tPrevGlobalStateWS("Prev State Workset", mNumCells, mNumGlobalDofsPerCell);
      mWorksetBase.worksetState(aPrevGlobalState, tPrevGlobalStateWS);

      // Workset local state
      //
      Plato::ScalarMultiVectorT<LocalStateScalar> 
                               tLocalStateWS("Local State Workset", mNumCells, mNumLocalDofsPerCell);
      mWorksetBase.worksetLocalState(aLocalState, tLocalStateWS);

      // Workset prev local state
      //
      Plato::ScalarMultiVectorT<PrevLocalStateScalar> 
                               tPrevLocalStateWS("Prev Local State Workset", mNumCells, mNumLocalDofsPerCell);
      mWorksetBase.worksetLocalState(aPrevLocalState, tPrevLocalStateWS);

      // Workset control
      //
      Plato::ScalarMultiVectorT<ControlScalar> tControlWS("Control Workset", mNumCells, mNumNodesPerCell);
      mWorksetBase.worksetControl(aControl, tControlWS);

      // create return view
      //
      Plato::ScalarMultiVectorT<ResultScalar> tJacobianWS("Jacobian Previous Local State Workset",mNumCells,mNumLocalDofsPerCell);

      // evaluate function
      //
      mLocalVectorFunctionJacobianCP->evaluate(tGlobalStateWS, tPrevGlobalStateWS, 
                                               tLocalStateWS,  tPrevLocalStateWS,
                                               tControlWS, tConfigWS, tJacobianWS, aTimeStep);

      Plato::ScalarArray3D tOutputJacobian("Output Jacobian Previous Local State", mNumCells, mNumLocalDofsPerCell, mNumLocalDofsPerCell);
      Plato::transform_ad_type_to_pod_3Dview<mNumLocalDofsPerCell, mNumLocalDofsPerCell>(mNumCells, tJacobianWS, tOutputJacobian);

      return tOutputJacobian;
    }

    /**************************************************************************//**
    * \brief Compute the gradient wrt control of the local residual vector
    * \param [in]  aGlobalState global state at current time step
    * \param [in]  aPrevGlobalState global state at previous time step
    * \param [in]  aLocalState local state at current time step
    * \param [in]  aPrevLocalState local state at previous time step
    * \param [in]  aControl control parameters
    * \param [in]  aTimeStep time step
    * \return gradient wrt control of the local residual vector
    ******************************************************************************/
    Plato::ScalarArray3D
    gradient_z(const Plato::ScalarVector & aGlobalState,
               const Plato::ScalarVector & aPrevGlobalState,
               const Plato::ScalarVector & aLocalState,
               const Plato::ScalarVector & aPrevLocalState,
               const Plato::ScalarVector & aControl,
               Plato::Scalar aTimeStep = 0.0) const
    {
      using ConfigScalar         = typename GradientZ::ConfigScalarType;
      using StateScalar          = typename GradientZ::StateScalarType;
      using PrevStateScalar      = typename GradientZ::PrevStateScalarType;
      using LocalStateScalar     = typename GradientZ::LocalStateScalarType;
      using PrevLocalStateScalar = typename GradientZ::PrevLocalStateScalarType;
      using ControlScalar        = typename GradientZ::ControlScalarType;
      using ResultScalar         = typename GradientZ::ResultScalarType;

      // Workset config
      //
      Plato::ScalarArray3DT<ConfigScalar>
          tConfigWS("Config Workset", mNumCells, mNumNodesPerCell, mNumSpatialDims);
      mWorksetBase.worksetConfig(tConfigWS);

      // Workset global state
      //
      Plato::ScalarMultiVectorT<StateScalar> tGlobalStateWS("State Workset", mNumCells, mNumGlobalDofsPerCell);
      mWorksetBase.worksetState(aGlobalState, tGlobalStateWS);

      // Workset prev global state
      //
      Plato::ScalarMultiVectorT<PrevStateScalar> tPrevGlobalStateWS("Prev State Workset", mNumCells, mNumGlobalDofsPerCell);
      mWorksetBase.worksetState(aPrevGlobalState, tPrevGlobalStateWS);

      // Workset local state
      //
      Plato::ScalarMultiVectorT<LocalStateScalar> 
                               tLocalStateWS("Local State Workset", mNumCells, mNumLocalDofsPerCell);
      mWorksetBase.worksetLocalState(aLocalState, tLocalStateWS);

      // Workset prev local state
      //
      Plato::ScalarMultiVectorT<PrevLocalStateScalar> 
                               tPrevLocalStateWS("Prev Local State Workset", mNumCells, mNumLocalDofsPerCell);
      mWorksetBase.worksetLocalState(aPrevLocalState, tPrevLocalStateWS);

      // Workset control
      //
      Plato::ScalarMultiVectorT<ControlScalar> tControlWS("Control Workset", mNumCells, mNumNodesPerCell);
      mWorksetBase.worksetControl(aControl, tControlWS);

      // create result 
      //
      Plato::ScalarMultiVectorT<ResultScalar> tJacobianWS("Jacobian Control Workset",mNumCells,mNumLocalDofsPerCell);

      // evaluate function 
      //
      mLocalVectorFunctionJacobianZ->evaluate(tGlobalStateWS, tPrevGlobalStateWS, 
                                              tLocalStateWS,  tPrevLocalStateWS, 
                                              tControlWS, tConfigWS, tJacobianWS, aTimeStep);

      Plato::ScalarArray3D tOutputJacobian("Output Jacobian Control", mNumCells, mNumLocalDofsPerCell, mNumNodesPerCell);
      Plato::transform_ad_type_to_pod_3Dview<mNumLocalDofsPerCell, mNumNodesPerCell>(mNumCells, tJacobianWS, tOutputJacobian);

      return tOutputJacobian;
    }
};
// class LocalVectorFunctionInc

} // namespace Plato

#include "Plasticity.hpp"
#include "ThermoPlasticity.hpp"

#ifdef PLATOANALYZE_2D
extern template class Plato::LocalVectorFunctionInc<Plato::Plasticity<2>>;
extern template class Plato::LocalVectorFunctionInc<Plato::ThermoPlasticity<2>>;
#endif
#ifdef PLATOANALYZE_3D
extern template class Plato::LocalVectorFunctionInc<Plato::Plasticity<3>>;
extern template class Plato::LocalVectorFunctionInc<Plato::ThermoPlasticity<3>>;
#endif