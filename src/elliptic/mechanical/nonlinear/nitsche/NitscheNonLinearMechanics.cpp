/*
 * NitscheNonLinearMechanics.cpp
 *
 *  Created on: July 14, 2023
 */

#include "elliptic/mechanical/nonlinear/nitsche/NitscheNonLinearMechanics_decl.hpp"

#ifdef PLATOANALYZE_USE_EXPLICIT_INSTANTIATION

#include "elliptic/mechanical/nonlinear/nitsche/NitscheNonLinearMechanics_def.hpp"

#include "ThermalElement.hpp"
#include "MechanicsElement.hpp"
#include "element/ThermoElasticElement.hpp"
#include "elliptic/ExpInstMacros.hpp"

PLATO_ELLIPTIC_EXP_INST_2(Plato::Elliptic::NitscheNonLinearMechanics, Plato::ThermalElement)
PLATO_ELLIPTIC_EXP_INST_2(Plato::Elliptic::NitscheNonLinearMechanics, Plato::MechanicsElement)
PLATO_ELLIPTIC_EXP_INST_2(Plato::Elliptic::NitscheNonLinearMechanics, Plato::ThermoElasticElement)

#endif