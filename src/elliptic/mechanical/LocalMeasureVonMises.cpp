/*
 * LocalMeasureVonMises.cpp
 *
 */

#include "elliptic/mechanical/LocalMeasureVonMises_decl.hpp"

#ifdef PLATOANALYZE_USE_EXPLICIT_INSTANTIATION

#include "elliptic/mechanical/LocalMeasureVonMises_def.hpp"

#include "MechanicsElement.hpp"
#include "elliptic/ExpInstMacros.hpp"

PLATO_ELLIPTIC_EXP_INST_2(Plato::LocalMeasureVonMises, Plato::MechanicsElement)

#endif
