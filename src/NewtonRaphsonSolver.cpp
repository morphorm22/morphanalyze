/*
 * NewtonRaphsonSolver.cpp
 *
 *  Created on: Mar 3, 2020
 */

#include "NewtonRaphsonSolver.hpp"

#ifdef PLATOANALYZE_2D
template class Plato::NewtonRaphsonSolver<Plato::InfinitesimalStrainPlasticity<2>>;
#endif

#ifdef PLATOANALYZE_3D
template class Plato::NewtonRaphsonSolver<Plato::InfinitesimalStrainPlasticity<3>>;
#endif

