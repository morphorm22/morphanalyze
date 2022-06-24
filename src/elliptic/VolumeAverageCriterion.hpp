#pragma once

#include <memory>
#include <cassert>
#include <vector>

#include "alg/Cubature.hpp"
#include "AnalyzeMacros.hpp"
#include "PlatoMeshExpr.hpp"
#include "PlatoStaticsTypes.hpp"
#include "WorksetBase.hpp"
#include "elliptic/ScalarFunctionBaseFactory.hpp"
#include "elliptic/PhysicsScalarFunction.hpp"
#include "elliptic/DivisionFunction.hpp"

#include <Teuchos_ParameterList.hpp>

namespace Plato
{

namespace Elliptic
{

/******************************************************************************//**
 * \brief Volume average criterion class
 **********************************************************************************/
template<typename PhysicsType>
class VolumeAverageCriterion :
    public Plato::Elliptic::ScalarFunctionBase,
    public Plato::WorksetBase<typename PhysicsType::ElementType>
{
private:
    using ElementType = typename PhysicsType::ElementType;

    using Residual  = typename Plato::Elliptic::Evaluation<ElementType>::Residual;
    using GradientU = typename Plato::Elliptic::Evaluation<ElementType>::Jacobian;
    using GradientX = typename Plato::Elliptic::Evaluation<ElementType>::GradientX;
    using GradientZ = typename Plato::Elliptic::Evaluation<ElementType>::GradientZ;

    std::shared_ptr<Plato::Elliptic::DivisionFunction<PhysicsType>> mDivisionFunction;

    const Plato::SpatialModel & mSpatialModel;

    Plato::DataMap& mDataMap; /*!< PLATO Engine and Analyze data map */

    std::string mFunctionName; /*!< User defined function name */

    std::string mSpatialWeightingFunctionString = "1.0"; /*!< Spatial weighting function string of x, y, z coordinates  */

    /******************************************************************************//**
     * \brief Initialization of Volume Average Criterion
     * \param [in] aInputParams input parameters database
    **********************************************************************************/
    void
    initialize(
        Teuchos::ParameterList & aInputParams
    )
    {
        auto params = aInputParams.sublist("Criteria").get<Teuchos::ParameterList>(mFunctionName);
        if (params.isType<std::string>("Function"))
            mSpatialWeightingFunctionString = params.get<std::string>("Function");

        createDivisionFunction(mSpatialModel, aInputParams);
    }


    /******************************************************************************//**
     * \brief Create the volume function only
     * \param [in] aSpatialModel Plato Analyze spatial model
     * \param [in] aInputParams parameter list
     * \return physics scalar function
    **********************************************************************************/
    std::shared_ptr<Plato::Elliptic::PhysicsScalarFunction<PhysicsType>>
    getVolumeFunction(
        const Plato::SpatialModel & aSpatialModel,
        Teuchos::ParameterList & aInputParams
    )
    {
        std::shared_ptr<Plato::Elliptic::PhysicsScalarFunction<PhysicsType>> tVolumeFunction =
             std::make_shared<Plato::Elliptic::PhysicsScalarFunction<PhysicsType>>(aSpatialModel, mDataMap);
        tVolumeFunction->setFunctionName("Volume Function");

        typename PhysicsType::FunctionFactory tFactory;
        std::string tFunctionType = "volume average criterion denominator";

        for(const auto& tDomain : mSpatialModel.Domains)
        {
            auto tName = tDomain.getDomainName();

            std::shared_ptr<Plato::Elliptic::AbstractScalarFunction<Residual>> tValue = 
                 tFactory.template createScalarFunction<Residual>(tDomain, mDataMap, aInputParams, tFunctionType, mFunctionName);
            tValue->setSpatialWeightFunction(mSpatialWeightingFunctionString);
            tVolumeFunction->setEvaluator(tValue, tName);

            std::shared_ptr<Plato::Elliptic::AbstractScalarFunction<GradientU>> tGradientU = 
                 tFactory.template createScalarFunction<GradientU>(tDomain, mDataMap, aInputParams, tFunctionType, mFunctionName);
            tGradientU->setSpatialWeightFunction(mSpatialWeightingFunctionString);
            tVolumeFunction->setEvaluator(tGradientU, tName);

            std::shared_ptr<Plato::Elliptic::AbstractScalarFunction<GradientZ>> tGradientZ = 
                 tFactory.template createScalarFunction<GradientZ>(tDomain, mDataMap, aInputParams, tFunctionType, mFunctionName);
            tGradientZ->setSpatialWeightFunction(mSpatialWeightingFunctionString);
            tVolumeFunction->setEvaluator(tGradientZ, tName);

            std::shared_ptr<Plato::Elliptic::AbstractScalarFunction<GradientX>> tGradientX = 
                 tFactory.template createScalarFunction<GradientX>(tDomain, mDataMap, aInputParams, tFunctionType, mFunctionName);
            tGradientX->setSpatialWeightFunction(mSpatialWeightingFunctionString);
            tVolumeFunction->setEvaluator(tGradientX, tName);
        }
        return tVolumeFunction;
    }

    /******************************************************************************//**
     * \brief Create the division function
     * \param [in] aSpatialModel Plato Analyze spatial model
     * \param [in] aInputParams parameter list
    **********************************************************************************/
    void
    createDivisionFunction(
        const Plato::SpatialModel & aSpatialModel,
        Teuchos::ParameterList & aInputParams
    )
    {
        const std::string tNumeratorName = "Volume Average Criterion Numerator";
        std::shared_ptr<Plato::Elliptic::PhysicsScalarFunction<PhysicsType>> tNumerator =
             std::make_shared<Plato::Elliptic::PhysicsScalarFunction<PhysicsType>>(aSpatialModel, mDataMap);
        tNumerator->setFunctionName(tNumeratorName);

        typename PhysicsType::FunctionFactory tFactory;
        std::string tFunctionType = "volume average criterion numerator";

        for(const auto& tDomain : mSpatialModel.Domains)
        {
            auto tName = tDomain.getDomainName();

            std::shared_ptr<Plato::Elliptic::AbstractScalarFunction<Residual>> tNumeratorValue = 
                 tFactory.template createScalarFunction<Residual>(tDomain, mDataMap, aInputParams, tFunctionType, mFunctionName);
            tNumeratorValue->setSpatialWeightFunction(mSpatialWeightingFunctionString);
            tNumerator->setEvaluator(tNumeratorValue, tName);

            std::shared_ptr<Plato::Elliptic::AbstractScalarFunction<GradientU>> tNumeratorGradientU = 
                 tFactory.template createScalarFunction<GradientU>(tDomain, mDataMap, aInputParams, tFunctionType, mFunctionName);
            tNumeratorGradientU->setSpatialWeightFunction(mSpatialWeightingFunctionString);
            tNumerator->setEvaluator(tNumeratorGradientU, tName);

            std::shared_ptr<Plato::Elliptic::AbstractScalarFunction<GradientZ>> tNumeratorGradientZ = 
                 tFactory.template createScalarFunction<GradientZ>(tDomain, mDataMap, aInputParams, tFunctionType, mFunctionName);
            tNumeratorGradientZ->setSpatialWeightFunction(mSpatialWeightingFunctionString);
            tNumerator->setEvaluator(tNumeratorGradientZ, tName);

            std::shared_ptr<Plato::Elliptic::AbstractScalarFunction<GradientX>> tNumeratorGradientX = 
                 tFactory.template createScalarFunction<GradientX>(tDomain, mDataMap, aInputParams, tFunctionType, mFunctionName);
            tNumeratorGradientX->setSpatialWeightFunction(mSpatialWeightingFunctionString);
            tNumerator->setEvaluator(tNumeratorGradientX, tName);
        }

        const std::string tDenominatorName = "Volume Function";
        std::shared_ptr<Plato::Elliptic::PhysicsScalarFunction<PhysicsType>> tDenominator = 
             getVolumeFunction(aSpatialModel, aInputParams);
        tDenominator->setFunctionName(tDenominatorName);

        mDivisionFunction =
             std::make_shared<Plato::Elliptic::DivisionFunction<PhysicsType>>(aSpatialModel, mDataMap);
        mDivisionFunction->allocateNumeratorFunction(tNumerator);
        mDivisionFunction->allocateDenominatorFunction(tDenominator);
        mDivisionFunction->setFunctionName("Volume Average Criterion Division Function");
    }


public:
    /******************************************************************************//**
     * \brief Primary volume average criterion constructor
     * \param [in] aSpatialModel Plato Analyze spatial model
     * \param [in] aDataMap PLATO Engine and Analyze data map
     * \param [in] aInputParams input parameters database
     * \param [in] aName user defined function name
    **********************************************************************************/
    VolumeAverageCriterion(
        const Plato::SpatialModel    & aSpatialModel,
              Plato::DataMap         & aDataMap,
              Teuchos::ParameterList & aInputParams,
              std::string            & aName
    ) :
        Plato::WorksetBase<ElementType>(aSpatialModel.Mesh),
        mSpatialModel (aSpatialModel),
        mDataMap      (aDataMap),
        mFunctionName (aName)
    {
        initialize(aInputParams);
    }


    /******************************************************************************//**
     * \brief Update physics-based parameters within optimization iterations
     * \param [in] aState 1D view of state variables
     * \param [in] aControl 1D view of control variables
     **********************************************************************************/
    void
    updateProblem(
        const Plato::ScalarVector & aState,
        const Plato::ScalarVector & aControl
    ) const override
    {
        mDivisionFunction->updateProblem(aState, aControl);
    }

    /******************************************************************************//**
     * \brief Evaluate Volume Average Criterion
     * \param [in] aSolution solution database
     * \param [in] aControl 1D view of control variables
     * \param [in] aTimeStep time step (default = 0.0)
     * \return scalar function evaluation
    **********************************************************************************/
    Plato::Scalar
    value(
        const Plato::Solutions    & aSolution,
        const Plato::ScalarVector & aControl,
              Plato::Scalar         aTimeStep = 0.0
    ) const override
    {
        Plato::Scalar tFunctionValue = mDivisionFunction->value(aSolution, aControl, aTimeStep);
        return tFunctionValue;
    }

    /******************************************************************************//**
     * \brief Evaluate gradient of the Volume Average Criterion with respect to (wrt) the state variables
     * \param [in] aSolution solution database
     * \param [in] aControl 1D view of control variables
     * \param [in] aTimeStep time step (default = 0.0)
     * \return 1D view with the gradient of the scalar function wrt the state variables
    **********************************************************************************/
    Plato::ScalarVector
    gradient_u(
        const Plato::Solutions    & aSolution,
        const Plato::ScalarVector & aControl,
              Plato::OrdinalType    aStepIndex,
              Plato::Scalar         aTimeStep = 0.0
    ) const override
    {
        Plato::ScalarVector tGradientU = mDivisionFunction->gradient_u(aSolution, aControl, aStepIndex, aTimeStep);
        return tGradientU;
    }

    /******************************************************************************//**
     * \brief Evaluate gradient of the Volume Average Criterion with respect to (wrt) the configuration
     * \param [in] aSolution solution database
     * \param [in] aControl 1D view of control variables
     * \param [in] aTimeStep time step (default = 0.0)
     * \return 1D view with the gradient of the scalar function wrt the state variables
    **********************************************************************************/
    Plato::ScalarVector
    gradient_x(
        const Plato::Solutions    & aSolution,
        const Plato::ScalarVector & aControl,
              Plato::Scalar         aTimeStep = 0.0
    ) const override
    {
        Plato::ScalarVector tGradientX = mDivisionFunction->gradient_x(aSolution, aControl, aTimeStep);
        return tGradientX;
    }

    /******************************************************************************//**
     * \brief Evaluate gradient of the Volume Average Criterion with respect to (wrt) the control
     * \param [in] aSolution solution database
     * \param [in] aControl 1D view of control variables
     * \param [in] aTimeStep time step (default = 0.0)
     * \return 1D view with the gradient of the scalar function wrt the state variables
    **********************************************************************************/
    Plato::ScalarVector
    gradient_z(
        const Plato::Solutions    & aSolution,
        const Plato::ScalarVector & aControl,
              Plato::Scalar         aTimeStep = 0.0
    ) const override
    {
        Plato::ScalarVector tGradientZ = mDivisionFunction->gradient_z(aSolution, aControl, aTimeStep);
        return tGradientZ;
    }


    /******************************************************************************//**
     * \brief Return user defined function name
     * \return User defined function name
    **********************************************************************************/
    std::string name() const
    {
        return mFunctionName;
    }
};
// class VolumeAverageCriterion

} // namespace Elliptic

} // namespace Plato

#include "Thermal.hpp"
#include "Mechanics.hpp"
#include "Electromechanics.hpp"
#include "Thermomechanics.hpp"
#include "ExpInstMacros.hpp"

PLATO_ELEMENT_DEC(Plato::Elliptic::VolumeAverageCriterion, Plato::Thermal)
PLATO_ELEMENT_DEC(Plato::Elliptic::VolumeAverageCriterion, Plato::Mechanics)
PLATO_ELEMENT_DEC(Plato::Elliptic::VolumeAverageCriterion, Plato::Thermomechanics)
PLATO_ELEMENT_DEC(Plato::Elliptic::VolumeAverageCriterion, Plato::Electromechanics)

#ifdef PLATO_STABILIZED
#include "StabilizedMechanics.hpp"
#include "StabilizedThermomechanics.hpp"

PLATO_ELEMENT_DEC(Plato::Elliptic::VolumeAverageCriterion, Plato::StabilizedMechanics)
PLATO_ELEMENT_DEC(Plato::Elliptic::VolumeAverageCriterion, Plato::StabilizedThermomechanics)
#endif
