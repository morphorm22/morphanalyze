#include "elliptic/ScalarFunctionBaseFactory_decl.hpp"

#ifdef PLATOANALYZE_USE_EXPLICIT_INSTANTIATION

#include "elliptic/ScalarFunctionBaseFactory_def.hpp"

#include "BaseExpInstMacros.hpp"
#include "Thermal.hpp"
#include "Mechanics.hpp"
#include "Thermomechanics.hpp"
#include "Electromechanics.hpp"
#include "elliptic/electrical/Electrical.hpp"

PLATO_ELEMENT_DEF(Plato::Elliptic::ScalarFunctionBaseFactory, Plato::Thermal)
PLATO_ELEMENT_DEF(Plato::Elliptic::ScalarFunctionBaseFactory, Plato::Mechanics)
PLATO_ELEMENT_DEF(Plato::Elliptic::ScalarFunctionBaseFactory, Plato::Electrical)
PLATO_ELEMENT_DEF(Plato::Elliptic::ScalarFunctionBaseFactory, Plato::Thermomechanics)
PLATO_ELEMENT_DEF(Plato::Elliptic::ScalarFunctionBaseFactory, Plato::Electromechanics)


#endif
