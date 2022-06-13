#ifndef PLATO_THERMOMECHANICS_HPP
#define PLATO_THERMOMECHANICS_HPP

#include <memory>

//TODO #include "parabolic/TransientThermomechResidual.hpp"

#include "parabolic/AbstractScalarFunction.hpp"
#include "elliptic/AbstractVectorFunction.hpp"
#include "elliptic/ThermoelastostaticResidual.hpp"
#include "elliptic/InternalThermoelasticEnergy.hpp"
#include "elliptic/TMStressPNorm.hpp"

//TODO #include "parabolic/InternalThermoelasticEnergy.hpp"
//TODO #include "elliptic/Volume.hpp"

//TODO #include "AbstractLocalMeasure.hpp"

#include "AnalyzeMacros.hpp"

//TODO #include "Plato_AugLagStressCriterionQuadratic.hpp"
//TODO #include "ThermalVonMisesLocalMeasure.hpp"

namespace Plato
{

namespace ThermomechanicsFactory
{
#ifdef NOPE
    /******************************************************************************//**
    * \brief Create a local measure for use in augmented lagrangian quadratic
    * \param [in] aProblemParams input parameters
    * \param [in] aFuncName scalar function name
    **********************************************************************************/
    template <typename EvaluationType>
    inline std::shared_ptr<Plato::AbstractLocalMeasure<EvaluationType,Plato::SimplexThermomechanics<EvaluationType::SpatialDim>>> 
    create_local_measure(
      const Plato::SpatialDomain   & aSpatialDomain,
            Teuchos::ParameterList & aProblemParams,
      const std::string            & aFuncName
    )
    {
        auto tFunctionSpecs = aProblemParams.sublist("Criteria").sublist(aFuncName);
        auto tLocalMeasure = tFunctionSpecs.get<std::string>("Local Measure", "VonMises");

        if(tLocalMeasure == "VonMises")
        {
          return std::make_shared<ThermalVonMisesLocalMeasure<EvaluationType, Plato::SimplexThermomechanics<EvaluationType::SpatialDim>>>
              (aSpatialDomain, aProblemParams, "VonMises");
        }
        else
        {
          ANALYZE_THROWERR("Unknown 'Local Measure' specified in 'Plato Problem' ParameterList")
        }
    }

    /******************************************************************************//**
     * \brief Create augmented Lagrangian local constraint criterion with quadratic constraint formulation
     * \param [in] aMesh mesh database
     * \param [in] aDataMap Plato Analyze physics-based database
     * \param [in] aInputParams input parameters
    **********************************************************************************/
    template<typename EvaluationType>
    inline std::shared_ptr<Plato::Elliptic::AbstractScalarFunction<EvaluationType>>
    stress_constraint_quadratic(
        const Plato::SpatialDomain   & aSpatialDomain,
              Plato::DataMap         & aDataMap,
              Teuchos::ParameterList & aInputParams,
        const std::string            & aFuncName)
    {
        auto EvalMeasure = Plato::ThermomechanicsFactory::create_local_measure<EvaluationType>(aSpatialDomain, aInputParams, aFuncName);
        using Residual = typename Plato::ResidualTypes<Plato::SimplexThermomechanics<EvaluationType::SpatialDim>>;
        auto PODMeasure = Plato::ThermomechanicsFactory::create_local_measure<Residual>(aSpatialDomain, aInputParams, aFuncName);

        using SimplexT = Plato::SimplexThermomechanics<EvaluationType::SpatialDim>;
        std::shared_ptr<Plato::AugLagStressCriterionQuadratic<EvaluationType,SimplexT>> tOutput;
        tOutput = std::make_shared< Plato::AugLagStressCriterionQuadratic<EvaluationType,SimplexT> >
                    (aSpatialDomain, aDataMap, aInputParams, aFuncName);
        //ANALYZE_THROWERR("Not finished implementing this for thermomechanics... need local measure that is compatible.")
        tOutput->setLocalMeasure(EvalMeasure, PODMeasure);
        return (tOutput);
    }
#endif

/******************************************************************************/
struct FunctionFactory
{
    /******************************************************************************/
    template<typename EvaluationType>
    std::shared_ptr<Plato::Elliptic::AbstractVectorFunction<EvaluationType>>
    createVectorFunction(
        const Plato::SpatialDomain   & aSpatialDomain,
              Plato::DataMap         & aDataMap,
              Teuchos::ParameterList & aParamList,
              std::string              aFuncType
    )
    /******************************************************************************/
    {

        auto tLowerFuncType = Plato::tolower(aFuncType);
        if(tLowerFuncType == "elliptic")
        {
            return Plato::Elliptic::makeVectorFunction<EvaluationType, Plato::Elliptic::ThermoelastostaticResidual>
                     (aSpatialDomain, aDataMap, aParamList, aFuncType);
        }
        else
        {
            ANALYZE_THROWERR("Unknown 'PDE Constraint' specified in 'Plato Problem' ParameterList");
        }
    }

#ifdef NOPE
    /******************************************************************************/
    template <typename EvaluationType>
    std::shared_ptr<Plato::Parabolic::AbstractVectorFunction<EvaluationType>>
    createVectorFunctionParabolic(
        const Plato::SpatialDomain   & aSpatialDomain,
              Plato::DataMap         & aDataMap,
              Teuchos::ParameterList & aParamList,
              std::string              strVectorFunctionType
    )
    /******************************************************************************/
    {
        if( strVectorFunctionType == "Parabolic" )
        {
            auto tPenaltyParams = aParamList.sublist(strVectorFunctionType).sublist("Penalty Function");
            std::string tPenaltyType = tPenaltyParams.get<std::string>("Type");
            if( tPenaltyType == "SIMP" )
            {
                return std::make_shared<Plato::Parabolic::TransientThermomechResidual<EvaluationType, Plato::MSIMP>>
                         (aSpatialDomain, aDataMap, aParamList, tPenaltyParams);
            } else
            if( tPenaltyType == "RAMP" )
            {
                return std::make_shared<Plato::Parabolic::TransientThermomechResidual<EvaluationType, Plato::RAMP>>
                         (aSpatialDomain, aDataMap, aParamList, tPenaltyParams);
            } else
            if( tPenaltyType == "Heaviside" )
            {
                return std::make_shared<Plato::Parabolic::TransientThermomechResidual<EvaluationType, Plato::Heaviside>>
                         (aSpatialDomain, aDataMap, aParamList, tPenaltyParams);
            } else {
                ANALYZE_THROWERR("Unknown 'Type' specified in 'Penalty Function' ParameterList");
            }
        }
        else
        {
            ANALYZE_THROWERR("Unknown 'PDE Constraint' specified in 'Plato Problem' ParameterList");
        }
    }
#endif

    /******************************************************************************/
    template<typename EvaluationType>
    std::shared_ptr<Plato::Elliptic::AbstractScalarFunction<EvaluationType>>
    createScalarFunction(
        const Plato::SpatialDomain   & aSpatialDomain,
              Plato::DataMap         & aDataMap, 
              Teuchos::ParameterList & aProblemParams, 
              std::string              aFuncType,
              std::string              aFuncName
    )
    /******************************************************************************/
    {

        auto tLowerFuncType = Plato::tolower(aFuncType);
        if(tLowerFuncType == "internal thermoelastic energy")
        {
            return Plato::Elliptic::makeScalarFunction<EvaluationType, Plato::Elliptic::InternalThermoelasticEnergy>
                (aSpatialDomain, aDataMap, aProblemParams, aFuncName);
        }
        else 
        if(tLowerFuncType == "stress p-norm")
        {
            return Plato::Elliptic::makeScalarFunction<EvaluationType, Plato::Elliptic::TMStressPNorm>
                (aSpatialDomain, aDataMap, aProblemParams, aFuncName);
        }
# ifdef NOPE
        else
        if(aStrScalarFunctionType == "Stress Constraint Quadratic")
        {
            return (Plato::ThermomechanicsFactory::stress_constraint_quadratic<EvaluationType>
                   (aSpatialDomain, aDataMap, aParamList, aStrScalarFunctionName));
        }
        else
        if(tLowerFuncType == "volume" )
        {
            return Plato::Elliptic::makeScalarFunction<EvaluationType, Plato::Elliptic::Volume>
                (aSpatialDomain, aDataMap, aProblemParams, aFuncName);
        }
#endif
        else
        {
            ANALYZE_THROWERR("Unknown 'Objective' specified in 'Plato Problem' ParameterList");
        }
    }

#ifdef NOPE
    /******************************************************************************/
    template <typename EvaluationType>
    std::shared_ptr<Plato::Parabolic::AbstractScalarFunction<EvaluationType>>
    createScalarFunctionParabolic(
        const Plato::SpatialDomain   & aSpatialDomain,
              Plato::DataMap         & aDataMap,
              Teuchos::ParameterList & aParamList,
              std::string              aStrScalarFunctionType,
              std::string              aStrScalarFunctionName
    )
    /******************************************************************************/
    {
        auto tPenaltyParams = aParamList.sublist("Criteria").sublist(aStrScalarFunctionName).sublist("Penalty Function");
        std::string tPenaltyType = tPenaltyParams.get<std::string>("Type");
        if( aStrScalarFunctionType == "Internal Thermoelastic Energy" )
        {
            if( tPenaltyType == "SIMP" )
            {
                return std::make_shared<Plato::Parabolic::InternalThermoelasticEnergy<EvaluationType, Plato::MSIMP>>
                     (aSpatialDomain, aDataMap, aParamList, tPenaltyParams, aStrScalarFunctionName);
            }
            else
            if( tPenaltyType == "RAMP" )
            {
                return std::make_shared<Plato::Parabolic::InternalThermoelasticEnergy<EvaluationType, Plato::RAMP>>
                     (aSpatialDomain, aDataMap, aParamList, tPenaltyParams, aStrScalarFunctionName);
            }
            else
            if( tPenaltyType == "Heaviside" )
            {
                return std::make_shared<Plato::Parabolic::InternalThermoelasticEnergy<EvaluationType, Plato::Heaviside>>
                     (aSpatialDomain, aDataMap, aParamList, tPenaltyParams, aStrScalarFunctionName);
            }
            else
            {
                ANALYZE_THROWERR("Unknown 'Type' specified in 'Penalty Function' ParameterList");
            }
        }
        else
        {
            ANALYZE_THROWERR("Unknown 'PDE Constraint' specified in 'Plato Problem' ParameterList");
        }
    }
#endif

}; // struct FunctionFactory

} // namespace ThermomechanicsFactory

} // namespace Plato

#include "ThermomechanicsElement.hpp"

namespace Plato
{
/****************************************************************************//**
 * \brief Concrete class for use as the SimplexPhysics template argument in
 *        Plato::Elliptic::Problem and Plato::Parabolic::Problem
 *******************************************************************************/
template<typename TopoElementType>
class Thermomechanics
{
public:
    typedef Plato::ThermomechanicsFactory::FunctionFactory FunctionFactory;
    using ElementType = ThermomechanicsElement<TopoElementType>;
};
} // namespace Plato

#endif
