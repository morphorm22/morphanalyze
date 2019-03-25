#ifndef PLATO_ELECTROMECHANICS_HPP
#define PLATO_ELECTROMECHANICS_HPP

#include <memory>

#include <Omega_h_mesh.hpp>
#include <Omega_h_assoc.hpp>

#include "plato/Simplex.hpp"
#include "plato/SimplexElectromechanics.hpp"
#include "plato/AbstractVectorFunction.hpp"
#include "plato/ElectroelastostaticResidual.hpp"
#include "plato/InternalElectroelasticEnergy.hpp"
#include "plato/Volume.hpp"
#include "plato/EMStressPNorm.hpp"
#include "plato/Simp.hpp"
#include "plato/Ramp.hpp"
#include "plato/Heaviside.hpp"

namespace Plato
{

namespace ElectromechanicsFactory
{

/******************************************************************************/
struct FunctionFactory
{
    /******************************************************************************/
    template<typename EvaluationType>
    std::shared_ptr<AbstractVectorFunction<EvaluationType>>
    createVectorFunction(Omega_h::Mesh& aMesh, 
                         Omega_h::MeshSets& aMeshSets,
                         Plato::DataMap& aDataMap, 
                         Teuchos::ParameterList& aParamList, 
                         std::string aStrVectorFunctionType)
    /******************************************************************************/
    {

        if(aStrVectorFunctionType == "Electroelastostatics")
        {
            auto tPenaltyParams = aParamList.sublist("Electroelastostatics").sublist("Penalty Function");
            std::string tPenaltyType = tPenaltyParams.get<std::string>("Type", "SIMP");
            if(tPenaltyType == "SIMP")
            {
                return std::make_shared<ElectroelastostaticResidual<EvaluationType, ::SIMP>>(aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else 
            if(tPenaltyType == "RAMP")
            {
                return std::make_shared<ElectroelastostaticResidual<EvaluationType, ::RAMP>>(aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else 
            if(tPenaltyType == "Heaviside")
            {
                return std::make_shared<ElectroelastostaticResidual<EvaluationType, ::Heaviside>>(aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else
            {
                throw std::runtime_error("Unknown 'Type' specified in 'Penalty Function' ParameterList");
            }
        }
        else
        {
            throw std::runtime_error("Unknown 'PDE Constraint' specified in 'Plato Problem' ParameterList");
        }
    }
    /******************************************************************************/
    template<typename EvaluationType>
    std::shared_ptr<AbstractScalarFunction<EvaluationType>>
    createScalarFunction(Omega_h::Mesh& aMesh,
                         Omega_h::MeshSets& aMeshSets,
                         Plato::DataMap& aDataMap, 
                         Teuchos::ParameterList & aParamList, 
                         std::string aStrScalarFunctionType)
    /******************************************************************************/
    {

        if(aStrScalarFunctionType == "Internal Electroelastic Energy")
        {
            auto tPenaltyParams = aParamList.sublist(aStrScalarFunctionType).sublist("Penalty Function");
            std::string tPenaltyType = tPenaltyParams.get<std::string>("Type", "SIMP");
            if(tPenaltyType == "SIMP")
            {
                return std::make_shared<InternalElectroelasticEnergy<EvaluationType, ::SIMP>>(aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else 
            if(tPenaltyType == "RAMP")
            {
                return std::make_shared<InternalElectroelasticEnergy<EvaluationType, ::RAMP>>(aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else 
            if(tPenaltyType == "Heaviside")
            {
                return std::make_shared<InternalElectroelasticEnergy<EvaluationType, ::Heaviside>>(aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else
            {
                throw std::runtime_error("Unknown 'Type' specified in 'Penalty Function' ParameterList");
            }
        }
        else if(aStrScalarFunctionType == "Stress P-Norm")
        {
            auto tPenaltyParams = aParamList.sublist(aStrScalarFunctionType).sublist("Penalty Function");
            std::string tPenaltyType = tPenaltyParams.get<std::string>("Type", "SIMP");
            if(tPenaltyType == "SIMP")
            {
                return std::make_shared<EMStressPNorm<EvaluationType, ::SIMP>>(aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else 
            if(tPenaltyType == "RAMP")
            {
                return std::make_shared<EMStressPNorm<EvaluationType, ::RAMP>>(aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else 
            if(tPenaltyType == "Heaviside")
            {
                return std::make_shared<EMStressPNorm<EvaluationType, ::Heaviside>>(aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else
            {
                throw std::runtime_error("Unknown 'Type' specified in 'Penalty Function' ParameterList");
            }
        }
        else if(aStrScalarFunctionType == "Volume")
        {
            auto tPenaltyParams = aParamList.sublist(aStrScalarFunctionType).sublist("Penalty Function");
            std::string tPenaltyType = tPenaltyParams.get<std::string>("Type", "SIMP");
            if(tPenaltyType == "SIMP")
            {
                return std::make_shared<Volume<EvaluationType, ::SIMP>>(aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else 
            if(tPenaltyType == "RAMP")
            {
                return std::make_shared<Volume<EvaluationType, ::RAMP>>(aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else 
            if(tPenaltyType == "Heaviside")
            {
                return std::make_shared<Volume<EvaluationType, ::Heaviside>>(aMesh, aMeshSets, aDataMap, aParamList, tPenaltyParams);
            }
            else
            {
                throw std::runtime_error("Unknown 'Type' specified in 'Penalty Function' ParameterList");
            }
        }
        else
        {
            throw std::runtime_error("Unknown 'Objective' specified in 'Plato Problem' ParameterList");
        }
    }
}; // struct FunctionFactory

} // namespace ElectromechanicsFactory

template<Plato::OrdinalType SpaceDimParam>
class Electromechanics: public Plato::SimplexElectromechanics<SpaceDimParam>
{
public:
    using FunctionFactory = typename Plato::ElectromechanicsFactory::FunctionFactory;
    static constexpr int SpaceDim = SpaceDimParam;
};

} // namespace Plato

#endif