/*
 * SourceWeightedSum.cpp
 *
 *  Created on: May 24, 2023
 */

#include "elliptic/electrical/SourceWeightedSum_decl.hpp"

#ifdef PLATOANALYZE_USE_EXPLICIT_INSTANTIATION

#include "elliptic/electrical/SourceWeightedSum_def.hpp"

#include "elliptic/electrical/ElectricalElement.hpp"
#include "elliptic/ExpInstMacros.hpp"

PLATO_ELLIPTIC_EXP_INST_2(Plato::SourceWeightedSum, Plato::ElectricalElement)

#endif