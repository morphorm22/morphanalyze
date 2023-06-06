/*
 * StressEvaluatorNeoHookean.cpp
 *
 *  Created on: May 31, 2023
 */

#include "elliptic/mechanical/nonlinear/StressEvaluatorNeoHookean_decl.hpp"

#ifdef PLATOANALYZE_USE_EXPLICIT_INSTANTIATION

#include "elliptic/mechanical/nonlinear/StressEvaluatorNeoHookean_def.hpp"

#include "MechanicsElement.hpp"
#include "elliptic/ExpInstMacros.hpp"

PLATO_ELLIPTIC_EXP_INST_2(Plato::StressEvaluatorNeoHookean,Plato::MechanicsElement)

#endif