/*
  Copyright (C) 2005-2011 Steven L. Scott

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
#ifndef BOOM_STATE_SPACE_MODEL_BASE_HPP_
#define BOOM_STATE_SPACE_MODEL_BASE_HPP_
#include <Models/StateSpace/StateModels/StateModel.hpp>
#include <Models/StateSpace/Filters/SparseVector.hpp>
#include <Models/StateSpace/Filters/SparseMatrix.hpp>
#include <Models/StateSpace/Filters/ScalarKalmanStorage.hpp>
#include <Models/StateSpace/PosteriorSamplers/SufstatManager.hpp>
#include <LinAlg/Matrix.hpp>
#include <LinAlg/Vector.hpp>

#include <cpputil/math_utils.hpp>
#include <memory>

namespace BOOM{

  class StateSpaceModelBase : virtual public Model {
   public:
    StateSpaceModelBase();
    StateSpaceModelBase(const StateSpaceModelBase &rhs);
    StateSpaceModelBase * clone() const override = 0;

    //----- sizes of things ------------
    // The number of time points in the training data.
    virtual int time_dimension() const = 0;

    // Number of elements in the state vector at a single time point.
    virtual int state_dimension() const;

    // The number of state models.  Presently, a fixed regression
    // model does not count as a state model, nor does a Harvey
    // Cumulator.  This may change in the future.
    int nstate() const {return state_models_.size();}

    //--------- Access to client models and model parameters ---------------
    // Returns a pointer to the model responsible for the observation
    // variance.
    virtual PosteriorModeModel * observation_model() = 0;
    virtual const PosteriorModeModel * observation_model() const = 0;

    // Returns a pointer to the specified state model.
    Ptr<StateModel> state_model(int s){return state_models_[s];}
    const Ptr<StateModel> state_model(int s) const {return state_models_[s];}

    // Parameters of initial state distribution, specified in the state models
    // given to add_state.  The initial state refers to the state at time 0
    // (other implementations sometimes assume the initial state is at time -1).
    virtual Vector initial_state_mean() const;
    virtual SpdMatrix initial_state_variance() const;

    // Overrides that would normally be handled by a parameter policy.
    // These are needed to ensure that parameters are vectorized in
    // the correct order.
    ParamVector parameter_vector() override;
    const ParamVector parameter_vector() const override;

    // Return the subset of the vectorized set of model parameters pertaining to
    // the observation model, or to a specific state model.  Can also be used to
    // take subsets of gradients of functions of model parameters.
    //
    // Args:
    //   model_parameters: A vector of model parameters, ordered in
    //     the same way as model->vectorize_params(true).
    //   s: The index of the state model for which a parameter subset
    //     is desired.
    //
    // Returns:
    //   The subset of the parameter vector corresponding to the specified
    //   model.
    VectorView state_parameter_component(Vector &model_parameters, int s) const;
    ConstVectorView state_parameter_component(const Vector &model_parameters,
                                              int s) const;
    VectorView observation_parameter_component(Vector &model_parameters) const;
    ConstVectorView observation_parameter_component(
        const Vector &model_parameters) const;

    // Sets an observer in 'params' that invalidates the Kalman filter
    // whenever params changes.
    void observe(Ptr<Params> params);

    //------------- Parameters for structural equations. --------------
    // Variance of observed data y[t], given state alpha[t].  Durbin
    // and Koopman's H.
    //
    // SCALAR:
    virtual double observation_variance(int t) const = 0;

    // Durbin and Koopman's T[t] built from state models.
    virtual const SparseKalmanMatrix * state_transition_matrix(int t) const;

    // Durbin and Koopman's Z[t].transpose() built from state models.
    //
    // SCALAR:
    virtual SparseVector observation_matrix(int t) const;

    // Durbin and Koopman's RQR^T.  Built from state models, often
    // less than full rank.
    virtual const SparseKalmanMatrix * state_variance_matrix(int t) const;

    // Durbin and Koopman's R matrix from the transition equation:
    //    state[t+1] = (T[t] * state[t]) + (R[t] * state_error[t]).
    //
    // This is the matrix that takes the low dimensional state_errors
    // and turns them into error terms for states.
    virtual const SparseKalmanMatrix * state_error_expander(int t) const;

    // The full rank variance matrix for the errors in the transition
    // equation.  This is Durbin and Koopman's Q[t].  The errors with
    // this variance are multiplied by state_error_expander(t) to
    // produce the errors described by state_variance_matrix(t).
    virtual const SparseKalmanMatrix * state_error_variance(int t) const;

    //----------------- Access to data -----------------
    // Returns y[t], after adjusting for regression effects that are
    // not included in the state vector.  This is the value that the
    // time series portion of the model is supposed to describe.  If
    // there are no regression effects, or if the state contains a
    // RegressionStateModel this is literally y[t].  If there are
    // regression effects it is y[t] - beta * x[t].  If y[t] is
    // missing then infinity() is returned.
    //
    // SCALAR:
    virtual double adjusted_observation(int t) const = 0;

    // Returns true if observation t is missing, and false otherwise.
    virtual bool is_missing_observation(int t) const = 0;

    // Clears sufficient statistics for state models and for
    // the client model describing observed data given state.
    virtual void clear_client_data();

    // This function is designed to be called in the constructor of a
    // PosteriorSampler managing this object.
    //
    // Args:
    //   observer: A functor, typically containing a pointer to a
    //     PosteriorSampler managing both this object and a set of
    //     complete data sufficient statistics.  Calling the functor
    //     with a non-negative integer t indicates that observation t
    //     has changed, so the complete data sufficient statistics
    //     should be updated.  Calling the functor with a negative
    //     integer is a signal to reset the complete data sufficient
    //     statistics.
    void register_data_observer(StateSpace::SufstatManagerBase *observer);

    // Send a signal to all data observers (typically just 1) that the
    // complete data for observation t has changed.
    //
    // This function is designed to be called as part of the
    // implementation for observe_data_given_state.
    void signal_complete_data_change(int t);

    //--------- Utilities for implementing data augmentation ----------
    // Sets the behavior of all client state models to 'behavior.'  State models
    // that can be represented as mixtures of normals should be set to MIXTURE
    // during data augmentation, and MARGINAL during forecasting.
    void set_state_model_behavior(StateModel::Behavior behavior);

    // Use the Durbin and Koopman method of forward filtering-backward
    // sampling.
    //  1. Sample the state vector and an auxiliary y
    //     vector from the model.
    //  2. Subtract the expected value of the state given the
    //     simulated y.
    //  3. Add the expected value of the state given the observed y.
    virtual void impute_state();

    // Update the complete data sufficient statistics for the
    // observation model based on the posterior distribution of the
    // observation model error term at time t.
    //
    // Args:
    //   t: The time of the observation.
    //   observation_error_mean: Mean of the observation error given
    //     model parameters and all observed y's.
    //   observation_error_variance: Variance of the observation error given
    //     model parameters and all observed y's.
    virtual void update_observation_model_complete_data_sufficient_statistics(
        int t,
        double observation_error_mean,
        double observation_error_variance);

    //---------------- Prediction, filtering, smoothing ---------------
    // filter() evaluates log likelihood and computes the final values
    // a[t+1] and P[t+1] needed for future forecasting.
    const ScalarKalmanStorage & filter() const;

    // Run the full Kalman filter over the observed data, saving the
    // information produced in the process in full_kalman_storage_.
    // The log likelihood is computed as a by-product.
    const ScalarKalmanStorage & full_kalman_filter();

    bool kalman_filter_is_current() const {return kalman_filter_is_current_;}

    // Returns the vector of one step ahead prediction errors for the
    // training data.
    Vector one_step_prediction_errors() const;

    // Add structure to the state portion of the model.  This is for
    // local linear trend and different seasonal effects.  It is not
    // for regression, which this class will handle separately.  The
    // state model should be initialized (including the model for the
    // initial state), and have its learning method (e.g. posterior
    // sampler) set prior to being added using add_state.
    void add_state(Ptr<StateModel>);

    //---------- Likelihood calculations ---------------------------
    // Returns the log likelihood under the current set of model parameters.  If
    // the Kalman filter is current (i.e. no parameters or data have changed
    // since the last time it was run) then this function does no actual work.
    // Otherwise it sparks a fresh Kalman filter run.
    double log_likelihood() const;

    // Evaluate the model log likelihood as a function of the model
    // parameters.
    // Args:
    //   parameters: The vector of model parameters in the same order
    //     as produced by vectorize_params(true).
    double log_likelihood(const Vector &parameters) const;

    // Evaluate the log likelihood function and its derivatives as a
    // function of model parameters.
    // Args:
    //   parameters: The vector of model parameters in the same order
    //     as produced by vectorize_params(true).
    //   gradient: Will be filled with the derivatives of log
    //     likelihood with respect to the vector of model parameters.
    //     The gradient vector will be resized if needed, and
    //     intialized to zero.
    // Returns:
    //   The value of log likelihood at the specified parameters.
    double log_likelihood_derivatives(const Vector &parameters,
                                      Vector &gradient) const;

    // Evaluate the log likelihood function and its derivatives at the
    // current model parameters.
    // Args:
    //   gradient: Will be filled with the deriviatives of log
    //     likelihood with respect to the current vector of model
    //     parameters.  The gradient vector will be resized if needed,
    //     and intialized to zero.
    // Returns:
    //   The value of log likelihood at the current set of parameter
    //   values.
    //
    // NOTE: This function is used to implement the version that also
    // takes a vector of arbitrary parameters.
    double log_likelihood_derivatives(VectorView gradient);

    //----------------- Simulating from the model -------------------

    // Simulate the initial model state from the initial state distribution.
    // The simulated value is returned in the vector view function argument.
    // The initial state refers to the state at time 0 (other implementations
    // sometimes assume the initial state is at time -1).
    virtual void simulate_initial_state(VectorView v) const;

    // Simulates the value of the state vector for the current time
    // period, t, given the value of state at the previous time
    // period, t-1.
    // Args:
    //   last:  Value of state at time t-1.
    //   next:  VectorView to be filled with state at time t.
    //   t:  The time index of 'next'.
    void simulate_next_state(const ConstVectorView last,
                             VectorView next,
                             int t) const;
    Vector simulate_next_state(const Vector &current_state, int t) const;

    // Simulates the error for the state at time t+1.  (Using the
    // notation of Durbin and Koopman, this uses the model matrices
    // indexed as t.)
    //
    // Returns a vector of size state_dimension().  If the model
    // matrices are not full rank then some elements of this vector
    // will be deterministic functions of other elements.
    virtual Vector simulate_state_error(int t) const;

    //------- Accessors for getting at state comopnents -----------
    ConstVectorView final_state() const;
    ConstVectorView state(int t) const;
    const Matrix &state() const;

    // Returns the contributions of each state model to the overall
    // mean of the series.  The outer vector is indexed by state
    // model.  The inner Vector is a time series.
    std::vector<Vector> state_contributions() const;

    // Returns a time series giving the contribution of state model
    // 'which_model' to the overall mean of the series being modeled.
    Vector state_contribution(int which_model) const;

    // Return true iff the model contains a regression component.
    virtual bool has_regression() const {return false;}

    // If the model contains a regression component, then return the
    // contribution of the regression model to the overall mean of y
    // at each time point.  If there is no regression component then
    // an empty vector is returned.
    virtual Vector regression_contribution() const;

    // Takes the full state vector as input, and returns the component
    // of the state vector belonging to state model s.
    //
    // Args:
    //   state:  The full state vector.
    //   s:  The index of the state model whose state component is desired.
    //
    // Returns:
    //   The subset of the 'state' argument corresponding to state model 's'.
    VectorView state_component(Vector &state, int s) const;
    VectorView state_component(VectorView &state, int s) const;
    ConstVectorView state_component(const ConstVectorView &state, int s) const;

    // Return the component of the full state error vector corresponding to a
    // given state model.
    //
    // Args:
    //   full_state_error: The error for the full state vector (i.e. all state
    //     models).
    //   state_model_number:  The index of the desired state model.
    //
    // Returns:
    //   The error vector for just the specified state model.
    ConstVectorView state_error_component(
        const Vector &full_state_error, int state_model_number) const;

    // Returns the subcomponent of the (block diagonal) error variance matrix
    // corresponding to a specific state model.
    //
    // Args:
    //   full_error_variance:  The full state error variance matrix.
    //   state: The index of the state model defining the desired sub-component.
    ConstSubMatrix state_error_variance_component(
        const SpdMatrix &full_error_variance, int state) const;

    // Returns the complete state vector (across time, so the return value is a
    // matrix) for a specified state component.
    //
    // Args:
    //   state_model_index:  The index of the desired state model.
    //
    // Returns:
    //   A matrix giving the imputed value of the state vector for the specified
    //   state model.  The matrix has S rows and T columns, where S is the
    //   dimension of the state vector for the specified state model, and T is
    //   the number of time points.
    Matrix full_state_subcomponent(int state_model_index) const;

    // The next two functions are mainly used for debugging a simulation.  You
    // can 'permanently_set_state' to the 'true' state value, then see if the
    // model recovers the parameters.  These functions are unlikely to be useful
    // in an actual data analysis.
    void permanently_set_state(const Matrix &m);
    void observe_fixed_state();

    //------------- Parameter estimation by MLE and MAP --------------------
    // Set model parameters to their maximum-likelihood estimates, and
    // return the likelihood at the MLE.  Note that some state models
    // cannot be used with this method.  In particular, regression
    // models with spike-and-slab priors can't be MLE'd.  If the model
    // contains such a state model then an exception will be thrown.
    //
    // Args:
    //   epsilon: Convergence for optimization algorithm will be
    //     declared when consecutive values of log-likelihood are
    //     observed with a difference of less than epsilon.
    //
    // Returns:
    //   The value of the log-likelihood at the MLE.
    double mle(double epsilon = 1e-5);

    // The E-step of the EM algorithm.  Computes complete data
    // sufficient statistics for state models and the observation
    // variance parameter.
    //
    // Args:
    //   save_state_distributions: If true then the state
    //     distributions (the mean vector a and the variance P) will
    //     be saved in full_kalman_storage_.  If not then these
    //     quantities will be left as computed by the
    //     full_kalman_filter.
    //
    // Returns:
    //   The log likelihood of the data computed at the model's
    //   current paramter values.
    double Estep(bool save_state_distributions);

    // To be called after calling Estep().  Given the current values
    // of the complete data sufficient statistics, set model
    // parameters to their complete data maximum likelihood estimates.
    // Args:
    //   epsilon: Additive convergence criteria for models that
    //     require numerical optimization.
    void Mstep(double epsilon);

    // Returns true if all the state models have been assigned priors
    // that implement find_posterior_mode.
    bool check_that_em_is_legal() const;

    // Returns a matrix containing the posterior mean of the state at
    // each time period.  These are stored in full_kalman_storage_,
    // and computed by the combination of full_kalman_filter() and
    // either a call to Estep(true) or full_kalman_smoother().
    //
    // Rows of the returned matrix correspond to state components.
    // Columns correspond to time points.
    Matrix state_posterior_means() const;

    // If called after full_kalman_filter and before any smoothing
    // operations, then state_filtering_means returns a matrix where
    // column t contains the expected value of the state at time t
    // given data to time t-1.
    //
    // It is an error to call this function if full_kalman_filter has
    // not been called, if smoothing steps have been taken after
    // calling the filter, or if model parameters have been changed.
    // In such cases the returned matrix will not contain the expected
    // values.
    Matrix state_filtering_means() const;

    // Returns the posterior variance (given model parameters and
    // observed data) of the state at time t.  This is stored in
    // full_kalman_storage_, and computed by the combination of
    // full_kalman_filter() and either a call to Estep(true) or
    // full_kalman_smoother().
    const SpdMatrix &state_posterior_variance(int t) const;

    // SCALAR:
    Vector observation_error_means() const;
    Vector observation_error_variances() const;

   protected:
    // Update the complete data sufficient statistics for the state
    // models, given the posterior distribution of the state error at
    // time t (for the transition between times t and t+1), given
    // model parameters and all observed data.
    //
    // Args:
    //   state_error_mean: The mean of the state error at time t
    //     given observed data and model parameters.
    //   state_error_variance: The variance of the state error at time
    //     t given observed data and model parameters.
    void update_state_level_complete_data_sufficient_statistics(
        int t,
        const Vector &state_error_mean,
        const SpdMatrix &state_error_variance);

    // Increment the portion of the log-likelihood gradient pertaining
    // to the parameters of the observation model.
    //
    // Args:
    //   gradient: The subset of the log likelihood gradient
    //     pertaining to the observation model.  The gradient will be
    //     incremented by the derivatives of log likelihood with
    //     respect to the observation model parameters.
    //   t:  The time index of the observation error.
    //   observation_error_mean: The posterior mean of the observation
    //     error at time t.
    //   observation_error_variance: The posterior variance of the
    //     observation error at time t.
    //
    // SCALAR:
    virtual void update_observation_model_gradient(
        VectorView gradient,
        int t,
        double observation_error_mean,
        double observation_error_variance);

   private:
    //-----Implementation details for the Kalman filter and smoother -----

    // The 'observe_state' functions compute the contribution to the
    // complete data sufficient statistics (for the observation and
    // state models) once the state at time 't' has been imputed.
    virtual void observe_state(int t);

    // The initial state can be treated specially, though the default for this
    // function is a no-op.  The initial state refers to the state at time 0
    // (other implementations sometimes assume the initial state is at time -1).
    virtual void observe_initial_state();

    // This is a hook that tells the observation model to update its
    // sufficient statisitcs now that the state for time t has been
    // observed.
    virtual void observe_data_given_state(int t) = 0;

    void check_light_kalman_storage(std::vector<LightKalmanStorage> &);
    void allocate_full_kalman_filter(int number_of_time_points,
                                     int state_dimension);
    void initialize_final_kalman_storage() const;
    void kalman_filter_is_not_current() {
      kalman_filter_is_current_ = false;
      mcmc_kalman_storage_is_current_ = false;
    }

    // A helper function used to implement average_over_latent_data().
    // Increments the gradient of log likelihood contribution of the
    // state models at time t (for the transition to time t+1).
    //
    // Args:
    //   gradient:  The gradient to be updated.
    //   t: The time index for the update.  Homogeneous models will
    //     ignore this, but models where the Kalman matrices depend on
    //     t need to know it.
    //   state_error_mean: The posterior mean of the state errors at
    //     time t (for the transition to time t+1).
    //   state_error_mean: The posterior variance of the state errors
    //     at time t (for the transition to time t+1).
    void update_state_model_gradient(
      Vector *gradient,
      int t,
      const Vector &state_error_mean,
      const SpdMatrix &state_error_variance);

    // Utility function used to implement E-step and log_likelihood_derivatives.
    //
    // Args:
    //   update_sufficient_statistics: If true then the complete data
    //     sufficient statistics for the observation model and the
    //     state models will be cleared and updated.  If false they
    //     will not be modified.
    //   save_state_distributions: If true then the state
    //     distributions (the mean vector a and the variance P) will
    //     be saved in full_kalman_storage_.  If not then these
    //     quantities will be left as computed by the
    //     full_kalman_filter.
    //   gradient: If a nullptr is passed then no gradient information
    //     will be computed.  Otherwise the gradient vector is
    //     resized, cleared, and filled with the gradient of log
    //     likelihood.
    //
    // Returns:
    //   The log likeilhood value computed by the Kalman filter.
    double average_over_latent_data(
        bool update_sufficient_statistics,
        bool save_state_distributions,
        Vector *gradient);

    // Send a signal to all data observers (typically just 1) that the
    // complete data sufficient statistics should be reset.
    void signal_complete_data_reset();

    // These are the steps needed to implement impute_state().
    void resize_state();

    // Simulate fake data from the model, given current model
    // parameters, as part of Durbin and Koopman's state-simulation
    // algorithm.
    void simulate_forward();

    double simulate_adjusted_observation(int t);

    // Given a vector of LightKalmanStorage obtained using the Kalman
    // filter, run the Durbin and Koopman 'fast disturbance smoother'.
    // Args:
    //   filter: Storage for the distributions produced as part of the
    //     Kalman filter.
    //
    // Returns:
    //   Durbin and Koopman's r0.
    Vector smooth_disturbances_fast(std::vector<LightKalmanStorage> &filter);

    void propagate_disturbances(const Vector &r0_plus,
                                const Vector &r0_hat,
                                bool observe = true);

    //----------------------------------------------------------------------
    // data starts here
    std::vector<Ptr<StateModel> > state_models_;

    // Dimension of the latent state vector.  Constructors set state_dimension
    // to zero.  It is incremented during calls to add_state.
    int state_dimension_;

    // At construction time state_error_dimension_ is set to zero.  It
    // is incremented during calls to add_state.  It gives the
    // dimension of the state innovation vector (from the transition
    // equation), which can be of lower dimension than the state
    // itself.
    int state_error_dimension_;

    // state_positions_[s] is the index in the state vector where the
    // state for state_models_[s] begins.  There will be one more
    // entry in this vector than the number of state models.  The last
    // entry can be ignored.
    std::vector<int> state_positions_;

    // state_error_positions_[s] is the index in the vector of state
    // errors where the error for state_models_[s] begins.  This
    // vector should have the same number of elements as
    // state_positions_, but the entries can be different because
    // state errors can be lower dimensional than the states
    // themselves.
    std::vector<int> state_error_positions_;

    // Position [s] is the index in the vector of parameters where the
    // parameter for state model s begins.  Note that the parameter
    // vector for the observation model begins in element 0.
    std::vector<int> parameter_positions_;

    // Workspace for impute_state.  No need to worry about this in the
    // constructor, as it will be initialized as needed.
    Matrix state_;
    Vector a_;
    SpdMatrix P_;
    std::vector<LightKalmanStorage> light_kalman_storage_;

    mutable std::vector<ScalarKalmanStorage> full_kalman_storage_;

    // state_is_fixed_ is for use in debugging.  If it is set then the
    // state will be held constant in the data imputation.
    bool state_is_fixed_;
    bool mcmc_kalman_storage_is_current_;

    // Supplemental storage is for filtering simulated observations
    // for Durbin and Koopman's simulation smoother.
    Vector supplemental_a_;
    SpdMatrix supplemental_P_;
    std::vector<LightKalmanStorage> supplemental_kalman_storage_;

    // final_kalman_storage_ holds the output of the Kalman filter.
    // It is for situations where we don't need to store the whole
    // filter, so the name 'final' refers to the fact that it is the
    // last kalman_storage you end up with after running the kalman
    // recursions.  The kalman_filter_is_current_ flag keeps track of
    // whether the parameters have changed since the last time the
    // filter was run.  It must be set by the constructor.  The others
    // will be managed by filter().
    mutable ScalarKalmanStorage final_kalman_storage_;
    mutable double log_likelihood_;
    mutable bool kalman_filter_is_current_;

    mutable std::unique_ptr<BlockDiagonalMatrix>
    default_state_transition_matrix_;

    mutable std::unique_ptr<BlockDiagonalMatrix>
    default_state_variance_matrix_;

    mutable std::unique_ptr<BlockDiagonalMatrix>
    default_state_error_expander_;

    mutable std::unique_ptr<BlockDiagonalMatrix>
    default_state_error_variance_;

    // Data observers exist so that changes to the (latent) data made
    // by the model can be incorporated by PosteriorSampler classes
    // keeping track of complete data sufficient statistics.  Gaussian
    // models do not need observers, but mixtures of Gaussians
    // typically do.
    std::vector<StateSpace::SufstatManager> data_observers_;
  };

  //======================================================================
  namespace StateSpaceUtils {

    // Some complex computations for state space models (e.g. log
    // likelihood and derivatives) can really only be done using the
    // current set of model parameters.  To enable likelihood
    // evaluation at other parameters, the idiom is to replace the
    // parameters, evalute the function, and restore the old set of
    // parameters when the computation concludes.
    //
    // This class holds the "old" set of model parameters while a
    // computation takes place, and restores them when it goes out of
    // scope.  Mutable values held by the model (kalman filter, log
    // likelihood) will no longer be current after the swap is undone.
    // It is the model's responsibility to keep track of these
    // quantities, but one could envision a user being affected by a
    // performance hit in some situations.
    class ParameterHolder {
     public:
      ParameterHolder(StateSpaceModelBase *model,
                      const Vector &parameters)
          : original_parameters_(model->vectorize_params()),
            model_(model) {
        model_->unvectorize_params(parameters);
      }

      ~ParameterHolder() {
        model_->unvectorize_params(original_parameters_);
      }
     private:
      Vector original_parameters_;
      StateSpaceModelBase *model_;
    };

    //----------------------------------------------------------------------
    // A helper class to manage the logical const-ness of evaluating a
    // state space model's log likelihood function.
    class LogLikelihoodEvaluator {
     public:
      LogLikelihoodEvaluator(const StateSpaceModelBase *model)
          : model_(const_cast<StateSpaceModelBase *>(model))
      {}

      double evaluate_log_likelihood(const Vector &parameters) {
        ParameterHolder storage(model_, parameters);
        return model_->log_likelihood();
      }

      double evaluate_log_posterior(const Vector &parameters) {
        ParameterHolder storage(model_, parameters);
        double ans = model_->observation_model()->logpri();
        if (ans <= negative_infinity()) { return ans; }
        for (int s = 0; s < model_->nstate(); ++s) {
          ans += model_->state_model(s)->logpri();
          if (ans <= negative_infinity()) { return ans; }
        }
        ans += model_->log_likelihood();
        return ans;
      }

      double evaluate_log_likelihood_derivatives(
          const ConstVectorView &parameters,
          VectorView gradient) {
        ParameterHolder storage(model_, parameters);
        return model_->log_likelihood_derivatives(gradient);
      }

     private:
      mutable StateSpaceModelBase *model_;
    };
  }  // namespace StateSpaceUtils

}  // namespace BOOM

#endif // BOOM_STATE_SPACE_MODEL_BASE_HPP_
