/*
 * NitscheBoundaryCondition.cpp
 *
 *  Created on: July 14, 2023
 */

#include "elliptic/evaluators/nitsche/NitscheBoundaryCondition_decl.hpp"

#ifdef PLATOANALYZE_USE_EXPLICIT_INSTANTIATION

#include "elliptic/evaluators/nitsche/NitscheBoundaryCondition_def.hpp"

#include "ThermalElement.hpp"
#include "MechanicsElement.hpp"
#include "element/ThermoElasticElement.hpp"
#include "elliptic/ExpInstMacros.hpp"

PLATO_ELLIPTIC_EXP_INST_2(Plato::Elliptic::NitscheBoundaryCondition, Plato::ThermalElement)
PLATO_ELLIPTIC_EXP_INST_2(Plato::Elliptic::NitscheBoundaryCondition, Plato::MechanicsElement)
PLATO_ELLIPTIC_EXP_INST_2(Plato::Elliptic::NitscheBoundaryCondition, Plato::ThermoElasticElement)

#endif