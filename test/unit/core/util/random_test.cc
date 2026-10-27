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
#include <Math/DistFunc.h>
#include <TF1.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <limits>
#include <numeric>
#include <set>
#include <vector>
#include "core/randomized_rm.h"
#include "unit/test_util/io_test.h"
#include "unit/test_util/test_util.h"

namespace bdm {

// Known-answer test: Random's stream equals numpy.random.Philox(key=[seed, 0]),
// converted to doubles with numpy's random() (top 53 bits).
TEST(RandomTest, MatchesNumpyPhilox) {
  Random random;  // default seed 4357
  const double expected[] = {0.10505628982287174, 0.34172994571577753,
                             0.46206037266469713, 0.34848655774794601,
                             0.41068526819046502, 0.402192415045875};
  for (double e : expected) {
    EXPECT_REAL_EQ(static_cast<real_t>(e), random.Uniform());
  }
}

TEST(RandomTest, SameSeedSameSequence) {
  Random a, b;
  a.SetSeed(42);
  b.SetSeed(42);
  for (int i = 0; i < 50; ++i) {
    EXPECT_EQ(a.Uniform(), b.Uniform());
    EXPECT_EQ(a.Uniform(-3, 7), b.Uniform(-3, 7));
    EXPECT_EQ(a.Gaus(1, 2), b.Gaus(1, 2));
    EXPECT_EQ(a.Exp(3), b.Exp(3));
    EXPECT_EQ(a.Landau(0, 1), b.Landau(0, 1));
    EXPECT_EQ(a.PoissonD(12.5), b.PoissonD(12.5));
    EXPECT_EQ(a.BreitWigner(0, 1), b.BreitWigner(0, 1));
    EXPECT_EQ(a.Integer(100), b.Integer(100));
    EXPECT_EQ(a.Binomial(50, 0.3), b.Binomial(50, 0.3));
    EXPECT_EQ(a.Poisson(4.5), b.Poisson(4.5));
    auto ca = a.Circle(2), cb = b.Circle(2);
    EXPECT_EQ(ca[0], cb[0]);
    EXPECT_EQ(ca[1], cb[1]);
    auto sa = a.Sphere(3), sb = b.Sphere(3);
    for (int k = 0; k < 3; ++k) {
      EXPECT_EQ(sa[k], sb[k]);
    }
  }
}

TEST(RandomTest, DifferentSeedsGiveDifferentSequences) {
  Random a, b;
  a.SetSeed(1);
  b.SetSeed(2);
  int same = 0;
  for (int i = 0; i < 10; ++i) {
    same += a.Uniform() == b.Uniform() ? 1 : 0;
  }
  EXPECT_LT(same, 10);
}

TEST(RandomTest, CopiesContinueTheSameSequence) {
  Random a;
  a.SetSeed(7);
  for (int i = 0; i < 7; ++i) {  // stop inside a Philox block
    a.Uniform();
  }
  Random copy(a);
  Random assigned;
  assigned = a;
  for (int i = 0; i < 20; ++i) {
    const real_t expected = a.Uniform();
    EXPECT_EQ(expected, copy.Uniform());
    EXPECT_EQ(expected, assigned.Uniform());
  }
}

TEST(RandomTest, SeedZeroPicksANonZeroSeed) {
  Random a;
  a.SetSeed(0);
  EXPECT_NE(0u, a.GetSeed());
  Random b;
  b.SetSeed(123);
  EXPECT_EQ(123u, b.GetSeed());
}

TEST(RandomTest, RangesAndEdgeCases) {
  Random r;
  r.SetSeed(99);
  for (int i = 0; i < 10000; ++i) {
    const real_t x = r.Uniform(3, 5);
    EXPECT_GE(x, 3);
    EXPECT_LT(x, 5);
    EXPECT_LT(r.Integer(7), 7u);
  }
  EXPECT_EQ(0u, r.Integer(0));
  EXPECT_EQ(0, r.Binomial(10, 0));
  EXPECT_EQ(10, r.Binomial(10, 1));
  EXPECT_EQ(0, r.Poisson(0));
  EXPECT_EQ(0, r.Landau(1, 0));
  auto c = r.Circle(2);
  EXPECT_NEAR(2, std::sqrt(c[0] * c[0] + c[1] * c[1]),
              abs_error<real_t>::value);
  auto s = r.Sphere(3);
  EXPECT_NEAR(3, s.Norm(), abs_error<real_t>::value * 10);
}

// Integer reads its argument as an unsigned 32-bit value, as ROOT's
// TRandom::Integer(UInt_t) did.
TEST(RandomTest, IntegerReadsMaxAsUnsigned) {
  Random r;
  r.SetSeed(5);
  unsigned largest = 0;
  for (int i = 0; i < 1000; ++i) {
    largest = std::max(largest, r.Integer(-1));
  }
  EXPECT_GT(largest, static_cast<unsigned>(std::numeric_limits<int>::max()));
}

// RandomizedRm shuffles with Ubrng outside Linux; a constant output would
// leave the agents in their original order.
TEST(RandomTest, UbrngProducesVaryingOutput) {
  Simulation simulation(TEST_NAME);
  Ubrng generator(simulation.GetRandom());
  std::set<Ubrng::result_type> values;
  for (int i = 0; i < 100; ++i) {
    values.insert(generator());
  }
  EXPECT_GT(values.size(), 95u);
}

TEST(RandomTest, DistributionsHaveTheExpectedMoments) {
  Random r;
  r.SetSeed(2024);
  const int n = 200000;
  auto moments = [&](auto draw, double* mean, double* var) {
    double s = 0, s2 = 0;
    for (int i = 0; i < n; ++i) {
      const double x = draw();
      s += x;
      s2 += x * x;
    }
    *mean = s / n;
    *var = s2 / n - *mean * *mean;
  };
  double m, v;
  moments([&] { return r.Gaus(2, 3); }, &m, &v);
  EXPECT_NEAR(2, m, 0.05);
  EXPECT_NEAR(9, v, 0.3);
  moments([&] { return r.Exp(2); }, &m, &v);
  EXPECT_NEAR(2, m, 0.05);
  moments([&] { return r.PoissonD(37); }, &m, &v);
  EXPECT_NEAR(37, m, 0.1);
  EXPECT_NEAR(37, v, 1.5);
  moments([&] { return r.Binomial(1000, 0.3); }, &m, &v);
  EXPECT_NEAR(300, m, 0.3);
  EXPECT_NEAR(210, v, 8);
  int below = 0;
  const double median = 1 + 2 * ROOT::Math::landau_quantile(0.5);
  for (int i = 0; i < n; ++i) {
    below += r.Landau(1, 2) < median ? 1 : 0;
  }
  EXPECT_NEAR(0.5, static_cast<double>(below) / n, 0.01);
  std::vector<double> bw(n);
  for (auto& x : bw) {
    x = r.BreitWigner(2, 3);
  }
  std::sort(bw.begin(), bw.end());
  EXPECT_NEAR(2, bw[n / 2], 0.05);
  EXPECT_NEAR(3, bw[3 * n / 4] - bw[n / 4], 0.1);
}

TEST(RandomTest, DistributionRngsDrawFromTheSimulationGenerator) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  auto compare = [&](auto direct, auto rng) {
    random->SetSeed(42);
    std::vector<double> expected;
    for (int i = 0; i < 20; ++i) {
      expected.push_back(direct());
    }
    random->SetSeed(42);
    for (int i = 0; i < 20; ++i) {
      EXPECT_EQ(expected[i], rng.Sample());
    }
  };
  compare([&] { return random->Uniform(1, 2); }, random->GetUniformRng(1, 2));
  compare([&] { return random->Gaus(1, 2); }, random->GetGausRng(1, 2));
  compare([&] { return random->Exp(3); }, random->GetExpRng(3));
  compare([&] { return random->Landau(1, 2); }, random->GetLandauRng(1, 2));
  compare([&] { return random->PoissonD(4); }, random->GetPoissonDRng(4));
  compare([&] { return random->BreitWigner(1, 2); },
          random->GetBreitWignerRng(1, 2));
  compare([&] { return random->Binomial(30, 0.4); },
          random->GetBinomialRng(30, 0.4));
  compare([&] { return random->Poisson(6); }, random->GetPoissonRng(6));
}

// User-defined distributions used ROOT's global gRandom before; they now draw
// from the simulation's generator, so its seed makes them reproducible.
TEST(RandomTest, UserDefinedDistributionsUseTheSimulationGenerator) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  auto function = [](const double* x, const double* params) {
    return ROOT::Math::lognormal_pdf(*x, params[0], params[1]);
  };
  auto rng1 = random->GetUserDefinedDistRng1D(function, {1.1, 1.2}, 1, 4);
  auto rng2 = random->GetUserDefinedDistRng2D(function, {1.1, 1.2}, 1, 4, 1, 4);
  auto rng3 =
      random->GetUserDefinedDistRng3D(function, {1.1, 1.2}, 1, 4, 1, 4, 1, 4);
  std::vector<real_t> first, second;
  for (auto* out : {&first, &second}) {
    random->SetSeed(42);
    for (int i = 0; i < 10; ++i) {
      const real_t x = rng1.Sample();
      EXPECT_GE(x, 1);
      EXPECT_LE(x, 4);
      out->push_back(x);
      auto p2 = rng2.Sample2();
      out->insert(out->end(), {p2[0], p2[1]});
      auto p3 = rng3.Sample3();
      out->insert(out->end(), {p3[0], p3[1], p3[2]});
    }
  }
  EXPECT_EQ(first, second);
}

// This test is neither a proper death test nor a proper function test. We run
// this test, to see if it is possible to initialize the UserDefinedDistRng1D
// in a parallel region because the generator is based on ROOT's TF1 class which
// seems to have trouble when the constructor is called in parallel. Before the
// fix provided in PR #208, this test would fail roughly 2/3 times.
TEST(RandomTest, UserDefinedDistRng1DParallel) {
  Simulation simulation(TEST_NAME);
  std::vector<real_t> results;
#pragma omp parallel shared(simulation, results)
  {
    auto* random = simulation.GetRandom();
    auto function = [](const double* x, const double* params) {
      return ROOT::Math::lognormal_pdf(*x, params[0], params[1]);
    };
    real_t min = 1;
    real_t max = 4;
    size_t n_samples = 10000;
    real_t result = 0.0;
    auto distrng =
        random->GetUserDefinedDistRng1D(function, {1.1, 1.2}, min, max);
    for (size_t i = 0; i < n_samples; i++) {
      result += distrng.Sample();
    }
#pragma omp critical
    { results.push_back(result / n_samples); }
  }
  real_t sum = std::accumulate(results.begin(), results.end(), 0.0);
  EXPECT_LT(sum, std::numeric_limits<real_t>::max());
}

// This test is neither a proper death test nor a proper function test. We run
// this test, to see if it is possible to initialize the UserDefinedDistRng2D
// in a parallel region because the generator is based on ROOT's TF2 class which
// seems to have trouble when the constructor is called in parallel. Before the
// fix provided in PR #208, this test would fail roughly 2/3 times.
TEST(RandomTest, UserDefinedDistRng2DParallel) {
  Simulation simulation(TEST_NAME);
  std::vector<real_t> results;
#pragma omp parallel shared(simulation, results)
  {
    auto* random = simulation.GetRandom();
    auto function = [](const double* x, const double* params) {
      return ROOT::Math::lognormal_pdf(*x, params[0], params[1]);
    };
    real_t min = 1;
    real_t max = 4;
    size_t n_samples = 10000;
    real_t result = 0.0;
    auto distrng = random->GetUserDefinedDistRng2D(function, {1.1, 1.2}, min,
                                                   max, min, max);
    for (size_t i = 0; i < n_samples; i++) {
      result += distrng.Sample2().Norm();
    }
#pragma omp critical
    { results.push_back(result / n_samples); }
  }
  real_t sum = std::accumulate(results.begin(), results.end(), 0.0);
  EXPECT_LT(sum, std::numeric_limits<real_t>::max());
}

// This test is neither a proper death test nor a proper function test. We run
// this test, to see if it is possible to initialize the UserDefinedDistRng3D
// in a parallel region because the generator is based on ROOT's TF3 class which
// seems to have trouble when the constructor is called in parallel. Before the
// fix provided in PR #208, this test would fail roughly 2/3 times.
TEST(RandomTest, UserDefinedDistRng3DParallel) {
  Simulation simulation(TEST_NAME);
  std::vector<real_t> results;
#pragma omp parallel shared(simulation, results)
  {
    auto* random = simulation.GetRandom();
    auto function = [](const double* x, const double* params) {
      return ROOT::Math::lognormal_pdf(*x, params[0], params[1]);
    };
    real_t min = 1;
    real_t max = 4;
    size_t n_samples = 10000;
    real_t result = 0.0;
    auto distrng = random->GetUserDefinedDistRng3D(function, {1.1, 1.2}, min,
                                                   max, min, max, min, max);
    for (size_t i = 0; i < n_samples; i++) {
      result += distrng.Sample3().Norm();
    }
#pragma omp critical
    { results.push_back(result / n_samples); }
  }
  real_t sum = std::accumulate(results.begin(), results.end(), 0.0);
  EXPECT_LT(sum, std::numeric_limits<real_t>::max());
}

#ifdef USE_DICT
TEST_F(IOTest, Random) {
  Random random;
  random.SetSeed(42);
  for (int i = 0; i < 5; i++) {
    random.Gaus();
  }
  Random* restored;
  BackupAndRestore(random, &restored);
  EXPECT_EQ(random.GetSeed(), restored->GetSeed());
  for (int i = 0; i < 10; i++) {
    EXPECT_EQ(random.Uniform(i, i + 2), restored->Uniform(i, i + 2));
  }
}

TEST_F(IOTest, UserDefinedDistRng1D) {
  Simulation simulation(TEST_NAME);
  auto* random = simulation.GetRandom();
  auto ud_dist = [](const double* x, const double* param) { return sin(*x); };
  auto udd_rng = random->GetUserDefinedDistRng1D(ud_dist, {}, 0, 3);

  random->SetSeed(42);
  std::vector<real_t> expected;
  for (int i = 0; i < 10; ++i) {
    expected.push_back(udd_rng.Sample());
  }

  UserDefinedDistRng1D* restored;
  BackupAndRestore(udd_rng, &restored);

  random->SetSeed(42);
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_NEAR(expected[i], restored->Sample(), 1e-6);
  }
}
#endif  // USE_DICT

}  // namespace bdm
