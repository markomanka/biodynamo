// -----------------------------------------------------------------------------
//
// Copyright (C) 2021 CERN & University of Surrey for the benefit of the
// BioDynaMo collaboration. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
//
// See the LICENSE file distributed with this work for details.
// See the NOTICE file distributed with this work for additional information
// regarding copyright ownership.
//
// -----------------------------------------------------------------------------

#include "core/util/random.h"
#include <TF1.h>
#include <TF2.h>
#include <TF3.h>
#include <TRandom.h>

#include <algorithm>
#include <climits>
#include <iterator>
#include <random>
#include "core/simulation.h"
#include "core/util/random_algorithms.h"

namespace bdm {

// -----------------------------------------------------------------------------
namespace {
/// Lets ROOT's TF1/TF2/TF3 draw their uniform numbers from a bdm::Random.
class TRandomAdapter : public TRandom {
 public:
  explicit TRandomAdapter(Random* random) : random_(random) {}
  Double_t Rndm() override { return 1.0 - random_->Uniform(); }  // (0, 1]

 private:
  Random* random_;
};
}  // namespace

Random::Random() { SetSeed(4357); }

Random::Random(TRootIOCtor*) {}

Random::Random(const Random& other) { *this = other; }

Random::~Random() {
  delete tf_adapter_;
  for (auto& el : udd_tf1_map_) {
    delete el.second;
  }
  for (auto& el : udd_tf2_map_) {
    delete el.second;
  }
  for (auto& el : udd_tf3_map_) {
    delete el.second;
  }
}

Random& Random::operator=(const Random& other) {
  if (&other != this) {
    seed_ = other.seed_;
    std::copy(std::begin(other.key_), std::end(other.key_), key_);
    std::copy(std::begin(other.counter_), std::end(other.counter_), counter_);
    std::copy(std::begin(other.buffer_), std::end(other.buffer_), buffer_);
    buffer_position_ = other.buffer_position_;
  }
  return *this;
}

double Random::NextUniform() {
  return random_algorithms::ToUnitInterval(random_algorithms::PhiloxNext(
      key_, counter_, buffer_, &buffer_position_));
}

TRandom* Random::GetTRandomAdapter() {
  if (tf_adapter_ == nullptr) {
    tf_adapter_ = new TRandomAdapter(this);
  }
  return tf_adapter_;
}

real_t Random::Uniform(real_t max) {
  return static_cast<real_t>(max * NextUniform());
}

real_t Random::Uniform(real_t min, real_t max) {
  return static_cast<real_t>(min + (max - min) * NextUniform());
}

real_t Random::Gaus(real_t mean, real_t sigma) {
  auto u = [this]() { return NextUniform(); };
  return static_cast<real_t>(mean +
                             sigma * random_algorithms::StandardNormal(u));
}

real_t Random::Exp(real_t tau) {
  auto u = [this]() { return NextUniform(); };
  return static_cast<real_t>(tau * random_algorithms::StandardExponential(u));
}

real_t Random::Landau(real_t mean, real_t sigma) {
  if (sigma <= 0) {
    return 0;
  }
  auto u = [this]() { return NextUniform(); };
  return static_cast<real_t>(mean +
                             sigma * random_algorithms::StandardLandau(u));
}

real_t Random::PoissonD(real_t mean) {
  auto u = [this]() { return NextUniform(); };
  return static_cast<real_t>(random_algorithms::Poisson(u, mean));
}

real_t Random::BreitWigner(real_t mean, real_t gamma) {
  auto u = [this]() { return NextUniform(); };
  return static_cast<real_t>(random_algorithms::BreitWigner(u, mean, gamma));
}

unsigned Random::Integer(int max) {
  if (max <= 0) {
    return 0;
  }
  return static_cast<unsigned>(max * NextUniform());
}

int Random::Binomial(int ntot, real_t prob) {
  auto u = [this]() { return NextUniform(); };
  return static_cast<int>(random_algorithms::Binomial(u, ntot, prob));
}

int Random::Poisson(real_t mean) {
  auto u = [this]() { return NextUniform(); };
  const double k = random_algorithms::Poisson(u, mean);
  return k > INT_MAX ? INT_MAX : static_cast<int>(k);
}

MathArray<real_t, 2> Random::Circle(real_t r) {
  const double phi = 2 * random_algorithms::kPi * NextUniform();
  return {static_cast<real_t>(r * std::cos(phi)),
          static_cast<real_t>(r * std::sin(phi))};
}

MathArray<real_t, 3> Random::Sphere(real_t r) {
  auto u = [this]() { return NextUniform(); };
  double x, y, z;
  random_algorithms::PointOnSphere(u, r, &x, &y, &z);
  return {static_cast<real_t>(x), static_cast<real_t>(y),
          static_cast<real_t>(z)};
}

void Random::SetSeed(uint64_t seed) {
  // Seed 0 keeps its previous meaning: pick a non-reproducible seed.
  while (seed == 0) {
    std::random_device device;
    seed = (static_cast<uint64_t>(device()) << 32) ^ device();
  }
  seed_ = seed;
  key_[0] = seed;
  key_[1] = 0;
  std::fill(std::begin(counter_), std::end(counter_), 0);
  std::fill(std::begin(buffer_), std::end(buffer_), 0);
  buffer_position_ = 4;
}

uint64_t Random::GetSeed() const { return seed_; }

template <typename TSample>
TSample DistributionRng<TSample>::Sample() {
  auto* rng = Simulation::GetActive()->GetRandom();
  return SampleImpl(rng);
}
template real_t DistributionRng<real_t>::Sample();
template int DistributionRng<int>::Sample();

// -----------------------------------------------------------------------------
template <typename TSample>
MathArray<TSample, 2> DistributionRng<TSample>::Sample2() {
  auto* rng = Simulation::GetActive()->GetRandom();
  return Sample2Impl(rng);
}
template MathArray<real_t, 2> DistributionRng<real_t>::Sample2();
template MathArray<int, 2> DistributionRng<int>::Sample2();

// -----------------------------------------------------------------------------
template <typename TSample>
MathArray<TSample, 2> DistributionRng<TSample>::Sample2Impl(Random* rng) {
  return MathArray<TSample, 2>({Sample(), Sample()});
}
template MathArray<real_t, 2> DistributionRng<real_t>::Sample2Impl(Random*);
template MathArray<int, 2> DistributionRng<int>::Sample2Impl(Random*);

// -----------------------------------------------------------------------------
template <typename TSample>
MathArray<TSample, 3> DistributionRng<TSample>::Sample3() {
  auto* rng = Simulation::GetActive()->GetRandom();
  return Sample3Impl(rng);
}
template MathArray<real_t, 3> DistributionRng<real_t>::Sample3();
template MathArray<int, 3> DistributionRng<int>::Sample3();

// -----------------------------------------------------------------------------
template <typename TSample>
MathArray<TSample, 3> DistributionRng<TSample>::Sample3Impl(Random* rng) {
  return MathArray<TSample, 3>(
      {SampleImpl(rng), SampleImpl(rng), SampleImpl(rng)});
}
template MathArray<real_t, 3> DistributionRng<real_t>::Sample3Impl(Random*);
template MathArray<int, 3> DistributionRng<int>::Sample3Impl(Random*);

// -----------------------------------------------------------------------------
UniformRng::UniformRng(real_t min, real_t max) : min_(min), max_(max) {}
UniformRng::~UniformRng() = default;
real_t UniformRng::SampleImpl(Random* rng) { return rng->Uniform(min_, max_); }

UniformRng Random::GetUniformRng(real_t min, real_t max) const {
  return UniformRng(min, max);
}

// -----------------------------------------------------------------------------
GausRng::GausRng(real_t mean, real_t sigma) : mean_(mean), sigma_(sigma) {}
GausRng::~GausRng() = default;
real_t GausRng::SampleImpl(Random* rng) { return rng->Gaus(mean_, sigma_); }

GausRng Random::GetGausRng(real_t mean, real_t sigma) const {
  return GausRng(mean, sigma);
}

// -----------------------------------------------------------------------------
ExpRng::ExpRng(real_t tau) : tau_(tau) {}
ExpRng::~ExpRng() = default;
real_t ExpRng::SampleImpl(Random* rng) { return rng->Exp(tau_); }

ExpRng Random::GetExpRng(real_t tau) const { return ExpRng(tau); }

// -----------------------------------------------------------------------------
LandauRng::LandauRng(real_t mean, real_t sigma) : mean_(mean), sigma_(sigma) {}
LandauRng::~LandauRng() = default;
real_t LandauRng::SampleImpl(Random* rng) { return rng->Landau(mean_, sigma_); }

LandauRng Random::GetLandauRng(real_t mean, real_t sigma) const {
  return LandauRng(mean, sigma);
}

// -----------------------------------------------------------------------------
PoissonDRng::PoissonDRng(real_t mean) : mean_(mean) {}
PoissonDRng::~PoissonDRng() = default;
real_t PoissonDRng::SampleImpl(Random* rng) { return rng->PoissonD(mean_); }

PoissonDRng Random::GetPoissonDRng(real_t mean) const {
  return PoissonDRng(mean);
}

// -----------------------------------------------------------------------------
BreitWignerRng::BreitWignerRng(real_t mean, real_t gamma)
    : mean_(mean), gamma_(gamma) {}
BreitWignerRng::~BreitWignerRng() = default;
real_t BreitWignerRng::SampleImpl(Random* rng) {
  return rng->BreitWigner(mean_, gamma_);
}

BreitWignerRng Random::GetBreitWignerRng(real_t mean, real_t gamma) const {
  return BreitWignerRng(mean, gamma);
}

// -----------------------------------------------------------------------------
UserDefinedDistRng1D::UserDefinedDistRng1D(TF1* function, const char* option)
    : function_(function), option_(option) {}
UserDefinedDistRng1D::~UserDefinedDistRng1D() = default;

real_t UserDefinedDistRng1D::SampleImpl(Random* rng) {
  auto min = function_->GetXmin();
  auto max = function_->GetXmax();
  return function_->GetRandom(min, max, rng->GetTRandomAdapter());
}
void UserDefinedDistRng1D::Draw(const char* option) { function_->Draw(option); }
TF1* UserDefinedDistRng1D::GetTF1() { return function_; }

// -----------------------------------------------------------------------------
UserDefinedDistRng1D Random::GetUserDefinedDistRng1D(
    double (*f)(const double*, const double*),
    const FixedSizeVector<real_t, 10>& params, real_t min, real_t max,
    const char* option) {
  TF1* tf1 = nullptr;
  UserDefinedDist udd{f, params, min, max};
  auto it = udd_tf1_map_.find(udd);
  if (it == udd_tf1_map_.end()) {
    tf1 = new TF1("", f, min, max, params.size());
    udd_tf1_map_[udd] = tf1;
    tf1->SetParameters(params[0], params[1], params[2], params[3], params[4],
                       params[5], params[6], params[7], params[8], params[9]);
  } else {
    tf1 = it->second;
  }
  return UserDefinedDistRng1D(tf1, option);
}

// -----------------------------------------------------------------------------
UserDefinedDistRng2D::UserDefinedDistRng2D(TF2* function, const char* option)
    : function_(function), option_(option) {}
UserDefinedDistRng2D::~UserDefinedDistRng2D() = default;

real_t UserDefinedDistRng2D::SampleImpl(Random* rng) {
  auto min = function_->GetXmin();
  auto max = function_->GetXmax();
  return function_->GetRandom(min, max, rng->GetTRandomAdapter());
}
MathArray<real_t, 2> UserDefinedDistRng2D::Sample2Impl(Random* rng) {
  MathArray<double, 2> ret;
  function_->GetRandom2(ret[0], ret[1], rng->GetTRandomAdapter());
  return {static_cast<real_t>(ret[0]), static_cast<real_t>(ret[1])};
}
void UserDefinedDistRng2D::Draw(const char* option) { function_->Draw(option); }
TF2* UserDefinedDistRng2D::GetTF2() { return function_; }

UserDefinedDistRng2D Random::GetUserDefinedDistRng2D(
    double (*f)(const double*, const double*),
    const FixedSizeVector<real_t, 10>& params, real_t xmin, real_t xmax,
    real_t ymin, real_t ymax, const char* option) {
  TF2* tf2 = nullptr;
  UserDefinedDist udd{f, params, xmin, xmax, ymin, ymax};
  auto it = udd_tf2_map_.find(udd);
  if (it == udd_tf2_map_.end()) {
    tf2 = new TF2("", f, xmin, xmax, ymin, ymax, params.size());
    udd_tf2_map_[udd] = tf2;
    tf2->SetParameters(params[0], params[1], params[2], params[3], params[4],
                       params[5], params[6], params[7], params[8], params[9]);
  } else {
    tf2 = it->second;
  }
  return UserDefinedDistRng2D(tf2, option);
}

// -----------------------------------------------------------------------------
UserDefinedDistRng3D::UserDefinedDistRng3D(TF3* function, const char* option)
    : function_(function), option_(option) {}
UserDefinedDistRng3D::~UserDefinedDistRng3D() = default;

real_t UserDefinedDistRng3D::SampleImpl(Random* rng) {
  auto min = function_->GetXmin();
  auto max = function_->GetXmax();
  return function_->GetRandom(min, max, rng->GetTRandomAdapter());
}
MathArray<real_t, 2> UserDefinedDistRng3D::Sample2Impl(Random* rng) {
  MathArray<double, 2> ret;
  function_->GetRandom2(ret[0], ret[1], rng->GetTRandomAdapter());
  return {static_cast<real_t>(ret[0]), static_cast<real_t>(ret[1])};
}
MathArray<real_t, 3> UserDefinedDistRng3D::Sample3Impl(Random* rng) {
  MathArray<double, 3> ret;
  function_->GetRandom3(ret[0], ret[1], ret[2], rng->GetTRandomAdapter());
  return {static_cast<real_t>(ret[0]), static_cast<real_t>(ret[1]),
          static_cast<real_t>(ret[2])};
}
void UserDefinedDistRng3D::Draw(const char* option) { function_->Draw(option); }
TF3* UserDefinedDistRng3D::GetTF3() { return function_; }

UserDefinedDistRng3D Random::GetUserDefinedDistRng3D(
    double (*f)(const double*, const double*),
    const FixedSizeVector<real_t, 10>& params, real_t xmin, real_t xmax,
    real_t ymin, real_t ymax, real_t zmin, real_t zmax, const char* option) {
  TF3* tf3 = nullptr;
  UserDefinedDist udd{f, params, xmin, xmax, ymin, ymax, zmin, zmax};
  auto it = udd_tf3_map_.find(udd);
  if (it == udd_tf3_map_.end()) {
    tf3 = new TF3("", f, xmin, xmax, ymin, ymax, zmin, zmax, params.size());
    udd_tf3_map_[udd] = tf3;
    tf3->SetParameters(params[0], params[1], params[2], params[3], params[4],
                       params[5], params[6], params[7], params[8], params[9]);
  } else {
    tf3 = it->second;
  }
  return UserDefinedDistRng3D(tf3, option);
}

// -----------------------------------------------------------------------------
BinomialRng::BinomialRng(int ntot, real_t prob) : ntot_(ntot), prob_(prob) {}
BinomialRng::~BinomialRng() = default;
int BinomialRng::SampleImpl(Random* rng) { return rng->Binomial(ntot_, prob_); }

BinomialRng Random::GetBinomialRng(int ntot, real_t prob) const {
  return BinomialRng(ntot, prob);
}

// -----------------------------------------------------------------------------
PoissonRng::PoissonRng(real_t mean) : mean_(mean) {}
PoissonRng::~PoissonRng() = default;
int PoissonRng::SampleImpl(Random* rng) { return rng->Poisson(mean_); }

PoissonRng Random::GetPoissonRng(real_t mean) const { return PoissonRng(mean); }

}  // namespace bdm
