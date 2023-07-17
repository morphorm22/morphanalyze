/*
 * MaterialNeoHookean.cpp
 *
 *  Created on: May 31, 2023
 */

#include "materials/mechanical/MaterialNeoHookean_decl.hpp"

#ifdef PLATOANALYZE_USE_EXPLICIT_INSTANTIATION

#include "materials/mechanical/MaterialNeoHookean_def.hpp"

#include "ThermalElement.hpp"
#include "MechanicsElement.hpp"
#include "element/ThermoElasticElement.hpp"
#include "elliptic/ExpInstMacros.hpp"

PLATO_ELLIPTIC_EXP_INST_2(Plato::MaterialNeoHookean,Plato::ThermalElement)
PLATO_ELLIPTIC_EXP_INST_2(Plato::MaterialNeoHookean,Plato::MechanicsElement)
PLATO_ELLIPTIC_EXP_INST_2(Plato::MaterialNeoHookean,Plato::ThermoElasticElement)

#endif