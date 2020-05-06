/*
 * ExpVolume.hpp
 *
 *  Created on: Apr 29, 2018
 */

#ifndef PLATO_EXPVOLUME_HPP_
#define PLATO_EXPVOLUME_HPP_

#include <memory>

#include <Teuchos_ParameterList.hpp>

#include "ImplicitFunctors.hpp"

#include "ApplyProjection.hpp"
#include "elliptic/AbstractScalarFunction.hpp"
#include "LinearTetCubRuleDegreeOne.hpp"
#include "SimplexStructuralDynamics.hpp"

namespace Plato
{

/******************************************************************************/
template<typename EvaluationType, typename PenaltyFuncType, typename ProjectionFuncType>
class ExpVolume :
        public Plato::SimplexStructuralDynamics<EvaluationType::SpatialDim, EvaluationType::NumControls>,
        public Plato::Elliptic::AbstractScalarFunction<EvaluationType>
/******************************************************************************/
{
private:
    using StateScalarType = typename EvaluationType::StateScalarType;
    using ControlScalarType = typename EvaluationType::ControlScalarType;
    using ConfigScalarType = typename EvaluationType::ConfigScalarType;
    using ResultScalarType = typename EvaluationType::ResultScalarType;

private:
    PenaltyFuncType mPenaltyFunction;
    ProjectionFuncType mProjectionFunction;
    Plato::ApplyProjection<ProjectionFuncType> mApplyProjection;

    std::shared_ptr<Plato::LinearTetCubRuleDegreeOne<EvaluationType::SpatialDim>> mCubatureRule;

public:
    /**************************************************************************/
    explicit ExpVolume(Omega_h::Mesh& aMesh,
                       Omega_h::MeshSets& aMeshSets,
                       Plato::DataMap& aDataMap, 
                       Teuchos::ParameterList & aPenaltyParams) :
            Plato::Elliptic::AbstractScalarFunction<EvaluationType>(aMesh, aMeshSets, aDataMap, "Experimental Volume"),
            mProjectionFunction(),
            mPenaltyFunction(aPenaltyParams),
            mApplyProjection(mProjectionFunction),
            mCubatureRule(std::make_shared<Plato::LinearTetCubRuleDegreeOne<EvaluationType::SpatialDim>>())
    /**************************************************************************/
    {
    }

    /**************************************************************************/
    explicit ExpVolume(Omega_h::Mesh& aMesh,
                       Omega_h::MeshSets& aMeshSets, 
                       Plato::DataMap& aDataMap) :
            Plato::Elliptic::AbstractScalarFunction<EvaluationType>(aMesh, aMeshSets, aDataMap, "Experimental Volume"),
            mProjectionFunction(),
            mPenaltyFunction(3.0, 0.0),
            mApplyProjection(mProjectionFunction),
            mCubatureRule(std::make_shared<Plato::LinearTetCubRuleDegreeOne<EvaluationType::SpatialDim>>())
    /**************************************************************************/
    {
    }

    /**************************************************************************/
    ~ExpVolume()
    {
    }
    /**************************************************************************/

    /**************************************************************************/
    void evaluate(const Plato::ScalarMultiVectorT<StateScalarType> &,
                  const Plato::ScalarMultiVectorT<ControlScalarType> & aControl,
                  const Plato::ScalarArray3DT<ConfigScalarType> & aConfig,
                  Plato::ScalarVectorT<ResultScalarType> & aResult,
                  Plato::Scalar aTimeStep = 0.0) const
                  /**************************************************************************/
    {
        Plato::ComputeCellVolume<EvaluationType::SpatialDim> tComputeCellVolume;

        auto tNumCells = aControl.extent(0);
        auto & tApplyProjection = mApplyProjection;
        auto & tPenaltyFunction = mPenaltyFunction;
        auto tQuadratureWeight = mCubatureRule->getCubWeight();
        Kokkos::parallel_for(Kokkos::RangePolicy<>(0, tNumCells), LAMBDA_EXPRESSION(const Plato::OrdinalType & aCellOrdinal)
        {
            ConfigScalarType tCellVolume;
            tComputeCellVolume(aCellOrdinal, aConfig, tCellVolume);
            tCellVolume *= tQuadratureWeight;
            aResult(aCellOrdinal) = tCellVolume;

            ControlScalarType tCellDensity = tApplyProjection(aCellOrdinal, aControl);
            ControlScalarType tPenaltyValue = tPenaltyFunction(tCellDensity);
            aResult(aCellOrdinal) *= tPenaltyValue;
        },"Experimental Volume");
    }
};
// class ExpVolume

} // namespace Plato

#endif /* PLATO_EXPVOLUME_HPP_ */
