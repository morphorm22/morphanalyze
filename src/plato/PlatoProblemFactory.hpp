/*
 * PlatoProblemFactory.hpp
 *
 *  Created on: Apr 19, 2018
 */

#ifndef PLATOPROBLEMFACTORY_HPP_
#define PLATOPROBLEMFACTORY_HPP_

#include <memory>

#include <Omega_h_mesh.hpp>
#include <Omega_h_assoc.hpp>

#include <Teuchos_ParameterList.hpp>

#include "PlatoProblem.hpp"
#include "plato/HeatEquationProblem.hpp"

#include "plato/Mechanics.hpp"
#include "plato/Thermomechanics.hpp"
//#include "plato/StructuralDynamics.hpp"
//#include "plato/StructuralDynamicsProblem.hpp"

namespace Plato
{

/**********************************************************************************/
template<int SpatialDim>
class ProblemFactory
{
/**********************************************************************************/
public:
    std::shared_ptr<Plato::AbstractProblem> 
    create(Omega_h::Mesh& aMesh, Omega_h::MeshSets& aMeshSets, Teuchos::ParameterList& aInputParams)
    {

        auto tProblemSpecs = aInputParams.sublist("Plato Problem");
        auto tProblemPhysics = tProblemSpecs.get<std::string>("Physics");
        auto tProblemPDE = tProblemSpecs.get<std::string>("PDE Constraint");

        if(tProblemPhysics == "Mechanical")
        {
            return std::make_shared<Problem<::Plato::Mechanics<SpatialDim>>>(aMesh, aMeshSets, tProblemSpecs);
        }
        else if(tProblemPhysics == "Thermal")
        {
            if(tProblemPDE == "Heat Equation") {
              return std::make_shared<HeatEquationProblem<::Plato::Thermal<SpatialDim>>>(aMesh, aMeshSets, tProblemSpecs);
            } else {
              return std::make_shared<Problem<::Plato::Thermal<SpatialDim>>>(aMesh, aMeshSets, tProblemSpecs);
            }
        }
        else if(tProblemPhysics == "StructuralDynamics")
        {
//            return std::make_shared<Plato::StructuralDynamicsProblem<Plato::StructuralDynamics<SpatialDim>>>(aMesh, aMeshSets, tProblemSpecs);
        }
        else if(tProblemPhysics == "Thermomechanical")
        {
            return std::make_shared<Problem<::Plato::Thermomechanics<SpatialDim>>>(aMesh, aMeshSets, tProblemSpecs);
        }
        return nullptr;
    }
};
// class ProblemFactory

} // namespace Plato

#endif /* PLATOPROBLEMFACTORY_HPP_ */
