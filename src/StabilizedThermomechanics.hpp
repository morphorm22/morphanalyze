#ifndef PLATO_STABILIZED_THERMOMECHANICS_HPP
#define PLATO_STABILIZED_THERMOMECHANICS_HPP

#include <memory>

#include <Omega_h_mesh.hpp>
#include <Omega_h_assoc.hpp>

#include "Simplex.hpp"
#include "SimplexProjection.hpp"
#include "AbstractVectorFunctionVMS.hpp"
#include "AbstractScalarFunction.hpp"
#include "AbstractScalarFunctionInc.hpp"
#include "Projection.hpp"
#include "StabilizedThermoelastostaticResidual.hpp"
#include "PressureGradientProjectionResidual.hpp"
#include "ThermoPlasticity.hpp"
#include "AnalyzeMacros.hpp"

#include "Simp.hpp"
#include "Ramp.hpp"
#include "Heaviside.hpp"
#include "NoPenalty.hpp"

namespace Plato
{

namespace StabilizedThermomechanicsFactory
{

/******************************************************************************/
struct FunctionFactory
{
    /******************************************************************************/
    template<typename EvaluationType>
    std::shared_ptr<Plato::AbstractVectorFunctionVMS<EvaluationType>>
    createVectorFunctionVMS(Omega_h::Mesh& aMesh,
                         Omega_h::MeshSets& aMeshSets,
                         Plato::DataMap& aDataMap,
                         Teuchos::ParameterList& aParamList,
                         std::string aStrVectorFunctionType)
    /******************************************************************************/
    {

        if(aStrVectorFunctionType == "Elliptic")
        {
            auto tPenaltyParams = aParamList.sublist(aStrVectorFunctionType).sublist("Penalty Function");
            std::string tPenaltyType = tPenaltyParams.get<std::string>("Type", "SIMP");
            if(tPenaltyType == "SIMP")
            {
                return std::make_shared<Plato::StabilizedThermoelastostaticResidual<EvaluationType, Plato::MSIMP>>
                         (aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else
            if(tPenaltyType == "RAMP")
            {
                return std::make_shared<Plato::StabilizedThermoelastostaticResidual<EvaluationType, Plato::RAMP>>
                         (aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else
            if(tPenaltyType == "Heaviside")
            {
                return std::make_shared<Plato::StabilizedThermoelastostaticResidual<EvaluationType, Plato::Heaviside>>
                         (aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else
            if(tPenaltyType == "NoPenalty")
            {
                return std::make_shared<Plato::StabilizedThermoelastostaticResidual<EvaluationType, Plato::NoPenalty>>
                         (aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else
            {
                THROWERR("Unknown 'Type' specified in 'Penalty Function' ParameterList");
            }
        }
        else
        {
            THROWERR("Unknown 'PDE Constraint' specified in 'Plato Problem' ParameterList");
        }
    }

    /******************************************************************************/
    template <typename EvaluationType>
    std::shared_ptr<AbstractVectorFunctionInc<EvaluationType>>
    createVectorFunctionInc(Omega_h::Mesh& aMesh,
                            Omega_h::MeshSets& aMeshSets, 
                            Plato::DataMap& aDataMap,
                            Teuchos::ParameterList& aParamList,
                            std::string strVectorFunctionType )
    /******************************************************************************/
    {
        {
            THROWERR("Unknown 'PDE Constraint' specified in 'Plato Problem' ParameterList");
        }
    }
    /******************************************************************************/
    template<typename EvaluationType>
    std::shared_ptr<Plato::AbstractScalarFunction<EvaluationType>>
    createScalarFunction(Omega_h::Mesh& aMesh,
                         Omega_h::MeshSets& aMeshSets,
                         Plato::DataMap& aDataMap, 
                         Teuchos::ParameterList & aParamList, 
                         std::string aStrScalarFunctionType,
                         std::string aStrScalarFunctionName)
    /******************************************************************************/
    {
        {
            THROWERR("Unknown 'Objective' specified in 'Plato Problem' ParameterList");
        }
    }
    /******************************************************************************/
    template <typename EvaluationType>
    std::shared_ptr<AbstractScalarFunctionInc<EvaluationType>>
    createScalarFunctionInc(Omega_h::Mesh& aMesh,
                            Omega_h::MeshSets& aMeshSets,
                            Plato::DataMap& aDataMap,
                            Teuchos::ParameterList& aParamList,
                            std::string strScalarFunctionType,
                            std::string aStrScalarFunctionName )
    /******************************************************************************/
    {
        {
            THROWERR("Unknown 'PDE Constraint' specified in 'Plato Problem' ParameterList");
        }
    }
}; // struct FunctionFactory

} // namespace StabilizedThermomechanicsFactory


/****************************************************************************//**
 * @brief Concrete class for use as the SimplexPhysics template argument in
 *        EllipticVMSProblem
 *******************************************************************************/
template<Plato::OrdinalType SpaceDimParam>
class StabilizedThermomechanics: public Plato::SimplexStabilizedThermomechanics<SpaceDimParam>
{
public:
    typedef Plato::StabilizedThermomechanicsFactory::FunctionFactory FunctionFactory;
    using SimplexT        = SimplexStabilizedThermomechanics<SpaceDimParam>;

    using LocalStateT   = typename Plato::ThermoPlasticity<SpaceDimParam>;

    using ProjectorT = typename Plato::Projection<SpaceDimParam,
                                                  SimplexT::mNumDofsPerNode,
                                                  SimplexT::mPressureDofOffset,
                                                  /* numProjectionDofs=*/ 1>;


    static constexpr Plato::OrdinalType SpaceDim = SpaceDimParam;
};

} // namespace Plato

#endif
