/*
  Copyright (C) 2005-2014 Steven L. Scott

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

#include <Models/PosteriorSamplers/GenericGaussianVarianceSampler.hpp>
#include <distributions.hpp>
#include <distributions/trun_gamma.hpp>
#include <cpputil/math_utils.hpp>

namespace BOOM {

  GenericGaussianVarianceSampler::GenericGaussianVarianceSampler(
      Ptr<GammaModelBase> prior)
      : prior_(prior),
        sigma_max_(infinity())
  {}

  GenericGaussianVarianceSampler::GenericGaussianVarianceSampler(
      Ptr<GammaModelBase> prior,
      double sigma_max)
      : prior_(prior)
  {
    set_sigma_max(sigma_max);
  }

  void GenericGaussianVarianceSampler::set_sigma_max(double sigma_max) {
    if (sigma_max < 0) {
      report_error("sigma_max must be non-negative.");
    }
    sigma_max_ = sigma_max;
  }

  double GenericGaussianVarianceSampler::draw(
      RNG &rng,
      double data_df,
      double data_ss) const {
    double DF = data_df + 2 * prior_->alpha();
    double SS = data_ss + 2 * prior_->beta();
    if (sigma_max_ == 0.0) {
      return 0.0;
    } else if(sigma_max_ == infinity()){
      return 1.0 / rgamma_mt(rng, DF/2, SS/2);
    } else {
      return 1.0 / rtrun_gamma_mt(rng, DF/2, SS/2, 1.0/square(sigma_max_));
    }
  }

  double GenericGaussianVarianceSampler::posterior_mode(
      double data_df, double data_ss) const {
    double DF = data_df + 2 * prior_->alpha();
    double SS = data_ss + 2 * prior_->beta();
    double alpha = DF / 2;
    double beta = SS / 2;
    double mode = beta / (alpha + 1);
    double sigsq_max = square(sigma_max_);
    return mode > sigsq_max ? sigsq_max : mode;
  }

  double GenericGaussianVarianceSampler::log_prior(double sigsq) const {
    // Use the prior on 1/sigsq to evaluate the base log density, then
    // add in the log of the Jacobian of the reciprocal
    // transformation.
    return prior_->logp(1.0 / sigsq) - 2 * log(sigsq);
  }

}  // namespace BOOM
