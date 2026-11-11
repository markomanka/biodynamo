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

#include "core/util/math.h"
#include "gtest/gtest.h"
#include "unit/test_util/test_util.h"

namespace bdm {

TEST(MathUtilTest, L2Distance) {
  Real3 vector1 = {0, 0, 0};
  Real3 vector2 = {1, 2, 3};
  auto result1 = Math::GetL2Distance(vector1, vector2);
  auto result2 = Math::GetL2Distance(vector2, vector1);

  EXPECT_NEAR(3.7416573867739413855, result1, abs_error<real_t>::value);
  EXPECT_NEAR(3.7416573867739413855, result2, abs_error<real_t>::value);
}

TEST(MathUtilTest, CrossProduct) {
  Real3 a = {1.1, 2.2, 3.3};
  Real3 b = {5.8, 7.3, 11.87};

  auto&& result = Math::CrossProduct(a, b);
  EXPECT_ARR_NEAR(result, {2.024, 6.083, -4.73});
}

TEST(MathUtilTest, RotAroundAxis) {
  Real3 axis = {1.0, 1.0, 0.0};
  Real3 vector = {4, 5, 6};
  real_t theta = Math::kPi;

  auto&& result = Math::RotAroundAxis(vector, theta, axis);
  EXPECT_ARR_NEAR(result, {5, 4, -6});
}

TEST(MathUtilTest, Perp3) {
  Real3 vector = {4, 5, 6};
  real_t random = 1.1234;

  auto&& result = Math::Perp3(vector, random);
  EXPECT_ARR_NEAR(result, {0.83614150897258999, -0.010824848782715613,
                           -0.54840696532946365});
}

TEST(MathUtilTest, AngleRadian) {
  Real3 a = {1, 2, 3};
  Real3 b = {9, 8, 7};

  real_t result = Math::AngleRadian(a, b);
  EXPECT_NEAR(0.489306575615854, result, abs_error<real_t>::value);
}

TEST(MathUtilTest, ProjectionOnto) {
  Real3 a = {1, 2, 3};
  Real3 b = {9, 8, 7};

  auto result = Math::ProjectionOnto(a, b);
  EXPECT_ARR_NEAR(
      result, {{2.134020618556701, 1.8969072164948453, 1.6597938144329896}});
}

TEST(MathUtilTest, MSE) {
  std::vector<real_t> v1 = {1, 2};
  std::vector<real_t> v2 = {4, 9};
  auto result = Math::MSE(v1, v2);
  EXPECT_NEAR(29.0, result, abs_error<real_t>::value);
}

// The two densities below replaced ROOT::Math::normal_pdf and
// ROOT::Math::poisson_pdf; the reference values are computed independently.
// Added after Aravind Raju's review of v1.06 asked for their edge cases.

TEST(MathUtilTest, NormalPdf) {
  const real_t tol = static_cast<real_t>(1e-6);
  // The standard normal at its mean: 1 / sqrt(2 pi).
  EXPECT_NEAR(0.3989422804014327, NormalPdf(0, 1, 0), tol);
  // x = 1, sigma = 2, mean = 3: one standard deviation below the mean.
  EXPECT_NEAR(0.12098536225957168, NormalPdf(1, 2, 3), tol);
  // Symmetric about the mean.
  EXPECT_NEAR(NormalPdf(1, 2, 3), NormalPdf(5, 2, 3), tol);
  // The sign of sigma does not matter, as in ROOT's definition.
  EXPECT_NEAR(NormalPdf(1, 2, 3), NormalPdf(1, -2, 3), tol);
  // Far in the tail the density is tiny and still not negative.
  EXPECT_GE(NormalPdf(40, 1, 0), 0);
  EXPECT_LT(NormalPdf(40, 1, 0), tol);
  // A narrower distribution is taller at its mean.
  EXPECT_GT(NormalPdf(0, static_cast<real_t>(0.5), 0), NormalPdf(0, 1, 0));
}

TEST(MathUtilTest, PoissonPdf) {
  const real_t tol = static_cast<real_t>(1e-6);
  const real_t mu = static_cast<real_t>(2.5);
  EXPECT_NEAR(0.0820849986238988, PoissonPdf(0, mu), tol);  // exp(-mu)
  EXPECT_NEAR(0.21376301724973645, PoissonPdf(3, mu), tol);
  // The number of events is floor(x).
  EXPECT_NEAR(PoissonPdf(3, mu), PoissonPdf(static_cast<real_t>(3.9), mu), tol);
  // No probability for a negative count, where ROOT's call was undefined.
  EXPECT_EQ(0, PoissonPdf(-1, mu));
  EXPECT_EQ(0, PoissonPdf(static_cast<real_t>(-0.5), mu));
  // With mu = 0 nothing ever happens: all the probability is at zero events.
  EXPECT_EQ(1, PoissonPdf(0, 0));
  EXPECT_EQ(0, PoissonPdf(4, 0));
}

TEST(MathUtilTest, PoissonPdfSumsToOneAndDoesNotOverflow) {
  const real_t mu = static_cast<real_t>(2.5);
  real_t sum = 0;
  for (int n = 0; n < 60; ++n) {
    sum += PoissonPdf(static_cast<real_t>(n), mu);
  }
  EXPECT_NEAR(1, sum, static_cast<real_t>(1e-5));
  // 200! overflows a double if computed directly; the logarithmic form does
  // not. The value is the probability of 200 events when 190 are expected.
  EXPECT_NEAR(0.02177108076424367, PoissonPdf(200, 190), static_cast<real_t>(1e-4));
}

}  // namespace bdm
