/*
  Copyright (C) 2005-2013 Steven L. Scott

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/

#ifndef BOOM_HIERARCHICAL_POISSON_MODEL_HPP_
#define BOOM_HIERARCHICAL_POISSON_MODEL_HPP_

#include <Models/PoissonModel.hpp>
#include <Models/GammaModel.hpp>
#include <Models/Policies/CompositeParamPolicy.hpp>
#include <Models/Hierarchical/HierarchicalModel.hpp>

namespace BOOM {

  class HierarchicalPoissonData : public Data {
   public:
    HierarchicalPoissonData(double event_count, double exposure);
    HierarchicalPoissonData * clone()const override;
    ostream & display(ostream &out)const override;
    double event_count() const {return event_count_;}
    double exposure() const {return exposure_;}
   private:
    double event_count_;
    double exposure_;
  };

  // The usual Poisson/gamma hierarchical model.
  //
  //      y[i] | lambda[i] ~ Poisson(lambda[i] * exposure[i])
  // labmda[i] | (a, b)    ~ Gamma(a, b)
  class HierarchicalPoissonModel
      : public HierarchicalModelBase<PoissonModel, GammaModel> {
   public:
    HierarchicalPoissonModel(double lambda_prior_guess,
                             double lambda_prior_sample_size);
    HierarchicalPoissonModel(Ptr<GammaModel> prior_model);
    HierarchicalPoissonModel * clone() const override;

    // Creates a new data_level_model with data assigned.
    void add_data(Ptr<Data>) override;

    double prior_mean()const;
    double prior_sample_size()const;
  };
}  // namespace BOOM

#endif //  BOOM_HIERARCHICAL_POISSON_MODEL_HPP_
