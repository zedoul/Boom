/*
  Copyright (C) 2005-2017 Steven L. Scott

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

#ifndef BOOM_STATE_SPACE_STUDENT_REGRESSION_MODEL_HPP_
#define BOOM_STATE_SPACE_STUDENT_REGRESSION_MODEL_HPP_

#include <Models/StateSpace/StateSpaceNormalMixture.hpp>
#include <Models/Glm/TRegression.hpp>
#include <Models/Policies/IID_DataPolicy.hpp>
#include <Models/Policies/PriorPolicy.hpp>

namespace BOOM {
  namespace StateSpace {

    // If y ~ T(mu, sigma^2, nu) then
    //          y = mu + epsilon / sqrt(w)
    //    epsilon ~ N(0, sigma^2)
    //          w ~ Gamma(nu / 2, nu / 2)
    // with epsilon independent of w.
    //
    // This class denotes 'w' as the 'weight' of y, which is a latent variable
    // that can be imputed from its full conditional distribution.  w / sigma^2
    // is the precision of y.
    //
    // In the case of multiplexed data, the "value" of this data point is the
    // precision weighted average of (y - x * beta) across observations.  The
    // precision weighted average has precision = (sum of weights) / sigma^2.
    class AugmentedStudentRegressionData
        : public Data {
     public:
      AugmentedStudentRegressionData();
      AugmentedStudentRegressionData(double y, const Vector &x);
      AugmentedStudentRegressionData(
          const std::vector<Ptr<RegressionData>> &data);

      AugmentedStudentRegressionData * clone() const override;
      std::ostream & display(std::ostream &out) const override;

      void add_data(const Ptr<RegressionData> &observation);

      double weight(int observation) const {return weights_[observation];}
      void set_weight(double weight, int observation);

      double adjusted_observation(const GlmCoefs &coefficients) const;
      double sum_of_weights() const;

      double state_model_offset() const {return state_model_offset_;}
      void set_state_model_offset(double offset);

      int sample_size() const {return regression_data_.size();}
      const RegressionData &regression_data(int observation) const {
        return *(regression_data_[observation]);
      }

     private:
      std::vector<Ptr<RegressionData>> regression_data_;

      Vector weights_;

      double state_model_offset_;
    };
  }  // namespace StateSpace

  class StateSpaceStudentRegressionModel
      : public StateSpaceNormalMixture,
        public IID_DataPolicy<StateSpace::AugmentedStudentRegressionData>,
        public PriorPolicy
  {
   public:
    StateSpaceStudentRegressionModel(int xdim);
    StateSpaceStudentRegressionModel(
        const Vector &y,
        const Matrix &X,
        const std::vector<bool> &observed = std::vector<bool>());
    StateSpaceStudentRegressionModel(
        const StateSpaceStudentRegressionModel &rhs);
    StateSpaceStudentRegressionModel * clone() const override;

    int time_dimension() const override;

    // The total number of observations across all time points.
    int sample_size() const;

    const RegressionData & data(int t, int observation) const override {
      return dat()[t]->regression_data(observation);
    }

    // Returns the imputed observation variance from the latent data
    // for observation t.  This is sigsq() / w[t] from the comment
    // above.
    double observation_variance(int t) const override;

    // Returns the value for observation t minus x[t]*beta.  Returns
    // infinity if observation t is missing.
    double adjusted_observation(int t) const override;

    // Returns true if observation t is missing, false otherwise.
    bool is_missing_observation(int t) const override;

    TRegressionModel *observation_model() override {
      return observation_model_.get(); }
    const TRegressionModel *observation_model() const override {
      return observation_model_.get(); }

    // Set the offset in the data to the state contribution.
    void observe_data_given_state(int t) override;

    Vector simulate_forecast(const Matrix &predictors,
                             const Vector &final_state);
    Vector one_step_holdout_prediction_errors(
        RNG &rng,
        const Vector &response,
        const Matrix &predictors,
        const Vector &final_state);

   private:
    // Sets up observers on model parameters, so that the Kalman
    // filter knows when it needs to be recomputed.
    void set_observers();
    Ptr<TRegressionModel> observation_model_;
  };

}  // namespace BOOM

#endif  // BOOM_STATE_SPACE_STUDENT_REGRESSION_MODEL_HPP_
