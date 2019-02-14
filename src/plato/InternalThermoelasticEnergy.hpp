#ifndef INTERNAL_THERMOELASTIC_ENERGY_HPP
#define INTERNAL_THERMOELASTIC_ENERGY_HPP

#include "plato/ScalarProduct.hpp"
#include "plato/ApplyWeighting.hpp"
#include "plato/Strain.hpp"
#include "plato/LinearStress.hpp"
#include "plato/AbstractScalarFunction.hpp"
#include "plato/LinearTetCubRuleDegreeOne.hpp"
#include "plato/ToMap.hpp"

/******************************************************************************/
template<typename EvaluationType, typename IndicatorFunctionType>
class InternalThermoelasticEnergy : 
  public Plato::SimplexThermomechanics<EvaluationType::SpatialDim>,
  public AbstractScalarFunction<EvaluationType>
/******************************************************************************/
{
  private:
    static constexpr int SpaceDim = EvaluationType::SpatialDim;
    
    using Plato::SimplexThermomechanics<SpaceDim>::m_numVoigtTerms;
    using Simplex<SpaceDim>::m_numNodesPerCell;
    using Plato::SimplexThermomechanics<SpaceDim>::m_numDofsPerCell;

    using AbstractScalarFunction<EvaluationType>::mMesh;
    using AbstractScalarFunction<EvaluationType>::m_dataMap;

    using StateScalarType   = typename EvaluationType::StateScalarType;
    using ControlScalarType = typename EvaluationType::ControlScalarType;
    using ConfigScalarType  = typename EvaluationType::ConfigScalarType;
    using ResultScalarType  = typename EvaluationType::ResultScalarType;

    Teuchos::RCP<Plato::LinearThermoelasticMaterial<SpaceDim>> m_materialModel;
    
    Plato::Scalar m_quadratureWeight;

    IndicatorFunctionType m_indicatorFunction;
    ApplyWeighting<SpaceDim,m_numVoigtTerms,IndicatorFunctionType> m_applyWeighting;
    std::shared_ptr<Plato::LinearTetCubRuleDegreeOne<EvaluationType::SpatialDim>> m_CubatureRule;

    std::vector<std::string> m_plottable;

  public:
    /**************************************************************************/
    InternalThermoelasticEnergy(Omega_h::Mesh& aMesh,
                          Omega_h::MeshSets& aMeshSets,
                          Plato::DataMap& aDataMap,
                          Teuchos::ParameterList& aProblemParams,
                          Teuchos::ParameterList& aPenaltyParams ) :
            AbstractScalarFunction<EvaluationType>(aMesh, aMeshSets, aDataMap, "Internal Thermoelastic Energy"),
            m_indicatorFunction(aPenaltyParams),
            m_applyWeighting(m_indicatorFunction),
            m_CubatureRule(std::make_shared<Plato::LinearTetCubRuleDegreeOne<EvaluationType::SpatialDim>>())
    /**************************************************************************/
    {
      Plato::ThermoelasticModelFactory<SpaceDim> mmfactory(aProblemParams);
      m_materialModel = mmfactory.create();

      if( aProblemParams.isType<Teuchos::Array<std::string>>("Plottable") )
        m_plottable = aProblemParams.get<Teuchos::Array<std::string>>("Plottable").toVector();
    }

    /**************************************************************************/
    void evaluate(const Plato::ScalarMultiVectorT<StateScalarType> & aState,
                  const Plato::ScalarMultiVectorT<ControlScalarType> & aControl,
                  const Plato::ScalarArray3DT<ConfigScalarType> & aConfig,
                  Plato::ScalarVectorT<ResultScalarType> & aResult,
                  Plato::Scalar aTimeStep = 0.0) const
    /**************************************************************************/
    {
      auto numCells = mMesh.nelems();
      auto cellStiffness = m_materialModel->getStiffnessMatrix();

      Plato::ComputeGradientWorkset<SpaceDim> computeGradient;
      Strain<SpaceDim>                        voigtStrain;
      LinearStress<SpaceDim>                  voigtStress(cellStiffness);
      ScalarProduct<m_numVoigtTerms>          scalarProduct;

      using StrainScalarType = 
        typename Plato::fad_type_t<Plato::SimplexThermomechanics<EvaluationType::SpatialDim>,
                            StateScalarType, ConfigScalarType>;

      Plato::ScalarVectorT<ConfigScalarType>
        cellVolume("cell weight",numCells);

      Kokkos::View<StrainScalarType**, Kokkos::LayoutRight, Plato::MemSpace>
        strain("strain",numCells,m_numVoigtTerms);

      Kokkos::View<ConfigScalarType***, Kokkos::LayoutRight, Plato::MemSpace>
        gradient("gradient",numCells,m_numNodesPerCell,SpaceDim);

      Kokkos::View<ResultScalarType**, Kokkos::LayoutRight, Plato::MemSpace>
        stress("stress",numCells,m_numVoigtTerms);

      auto quadratureWeight = m_quadratureWeight;
      auto applyWeighting  = m_applyWeighting;
      Kokkos::parallel_for(Kokkos::RangePolicy<int>(0,numCells), LAMBDA_EXPRESSION(const int & aCellOrdinal)
      {
        computeGradient(aCellOrdinal, gradient, aConfig, cellVolume);
        cellVolume(aCellOrdinal) *= quadratureWeight;

        // compute strain
        //
        voigtStrain(aCellOrdinal, strain, aState, gradient);

        // compute stress
        //
        voigtStress(aCellOrdinal, stress, strain);

        // apply weighting
        //
        applyWeighting(aCellOrdinal, stress, aControl);
    
        // compute element internal energy (inner product of strain and weighted stress)
        //
        scalarProduct(aCellOrdinal, aResult, stress, strain, cellVolume);

      },"energy gradient");

      if( std::count(m_plottable.begin(),m_plottable.end(),"strain") ) toMap(m_dataMap, strain, "strain");
      if( std::count(m_plottable.begin(),m_plottable.end(),"stress") ) toMap(m_dataMap, stress, "stress");

    }
};

#endif
