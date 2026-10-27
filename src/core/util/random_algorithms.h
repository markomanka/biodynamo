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

#ifndef CORE_UTIL_RANDOM_ALGORITHMS_H_
#define CORE_UTIL_RANDOM_ALGORITHMS_H_

#include <cmath>
#include <cstdint>

namespace bdm {
namespace random_algorithms {

/// Full 64x64 -> 128 bit product, returned as high and low halves.
inline void MulHiLo64(uint64_t a, uint64_t b, uint64_t* hi, uint64_t* lo) {
#if defined(__SIZEOF_INT128__)
  __extension__ typedef unsigned __int128 Uint128;
  const Uint128 product = static_cast<Uint128>(a) * b;
  *hi = static_cast<uint64_t>(product >> 64);
  *lo = static_cast<uint64_t>(product);
#else
  const uint64_t a_lo = a & 0xFFFFFFFFull, a_hi = a >> 32;
  const uint64_t b_lo = b & 0xFFFFFFFFull, b_hi = b >> 32;
  const uint64_t ll = a_lo * b_lo, lh = a_lo * b_hi, hl = a_hi * b_lo;
  const uint64_t hh = a_hi * b_hi;
  const uint64_t mid = (ll >> 32) + (lh & 0xFFFFFFFFull) + (hl & 0xFFFFFFFFull);
  *lo = (mid << 32) | (ll & 0xFFFFFFFFull);
  *hi = hh + (lh >> 32) + (hl >> 32) + (mid >> 32);
#endif
}

/// One block of the Philox4x64-10 counter-based generator (Salmon, Moraes,
/// Dror, Shaw: "Parallel random numbers: as easy as 1, 2, 3", SC'11).
inline void Philox4x64Block(const uint64_t counter[4], const uint64_t key[2],
                            uint64_t out[4]) {
  constexpr uint64_t kM0 = 0xD2E7470EE14C6C93ull;
  constexpr uint64_t kM1 = 0xCA5A826395121157ull;
  constexpr uint64_t kW0 = 0x9E3779B97F4A7C15ull;
  constexpr uint64_t kW1 = 0xBB67AE8584CAA73Bull;
  uint64_t c0 = counter[0], c1 = counter[1], c2 = counter[2], c3 = counter[3];
  uint64_t k0 = key[0], k1 = key[1];
  for (int round = 0; round < 10; ++round) {
    if (round > 0) {
      k0 += kW0;
      k1 += kW1;
    }
    uint64_t hi0, lo0, hi1, lo1;
    MulHiLo64(kM0, c0, &hi0, &lo0);
    MulHiLo64(kM1, c2, &hi1, &lo1);
    const uint64_t n0 = hi1 ^ c1 ^ k0;
    const uint64_t n2 = hi0 ^ c3 ^ k1;
    c0 = n0;
    c1 = lo1;
    c2 = n2;
    c3 = lo0;
  }
  out[0] = c0;
  out[1] = c1;
  out[2] = c2;
  out[3] = c3;
}

/// Next 64-bit value of a Philox stream. The stream equals the raw output of
/// numpy.random.Philox(key=[key[0], key[1]]) when started with a zero counter
/// and `position` = 4: the counter is incremented before each block.
inline uint64_t PhiloxNext(const uint64_t key[2], uint64_t counter[4],
                           uint64_t buffer[4], int* position) {
  if (*position >= 4) {
    for (int i = 0; i < 4 && ++counter[i] == 0; ++i) {
    }
    Philox4x64Block(counter, key, buffer);
    *position = 0;
  }
  return buffer[(*position)++];
}

/// Uniform double in [0, 1) from the top 53 bits, as numpy's random().
inline double ToUnitInterval(uint64_t x) {
  return static_cast<double>(x >> 11) * (1.0 / 9007199254740992.0);
}

constexpr double kPi = 3.14159265358979323846;

// Distributions. `u` is a callable returning uniform doubles in [0, 1).
// Each function documents how many values it consumes.

/// Standard normal by Box-Muller; consumes 2 values.
template <typename U>
double StandardNormal(U& u) {
  const double radius = std::sqrt(-2.0 * std::log(1.0 - u()));
  return radius * std::cos(2.0 * kPi * u());
}

/// Exponential with mean 1; consumes 1 value.
template <typename U>
double StandardExponential(U& u) {
  return -std::log(1.0 - u());
}

/// Standard Landau distribution (as ROOT::Math::landau_pdf with xi = 1,
/// x0 = 0): the stable law with alpha = 1, beta = 1, scale pi/2, generated
/// with the Chambers-Mallows-Stuck method; consumes 2 or more values.
template <typename U>
double StandardLandau(U& u) {
  constexpr double kHalfPi = kPi / 2;
  while (true) {
    const double w = kPi * (u() - 0.5);
    const double e = StandardExponential(u);
    const double shifted = kHalfPi + w;
    if (e <= 0 || shifted <= 0) {
      continue;  // measure-zero edge values
    }
    const double z =
        (2.0 / kPi) *
        (shifted * std::tan(w) - std::log(kHalfPi * e * std::cos(w) / shifted));
    return kHalfPi * z + std::log(kHalfPi);
  }
}

/// Poisson-distributed count with the given mean: multiplication method for
/// mean < 10, transformed rejection (Hoermann's PTRS, 1993) otherwise.
template <typename U>
double Poisson(U& u, double mean) {
  if (!(mean > 0)) {
    return 0;
  }
  if (mean < 10) {
    const double limit = std::exp(-mean);
    double k = 0, product = u();
    while (product > limit) {
      k += 1;
      product *= u();
    }
    return k;
  }
  const double slam = std::sqrt(mean), loglam = std::log(mean);
  const double b = 0.931 + 2.53 * slam;
  const double a = -0.059 + 0.02483 * b;
  const double invalpha = 1.1239 + 1.1328 / (b - 3.4);
  const double vr = 0.9277 - 3.6224 / (b - 2);
  while (true) {
    const double uu = u() - 0.5;
    const double v = u();
    const double us = 0.5 - std::abs(uu);
    const double k = std::floor((2 * a / us + b) * uu + mean + 0.43);
    if (us >= 0.07 && v <= vr) {
      return k;
    }
    if (k < 0 || (us < 0.013 && v > us)) {
      continue;
    }
    if (std::log(v) + std::log(invalpha) - std::log(a / (us * us) + b) <=
        -mean + k * loglam - std::lgamma(k + 1)) {
      return k;
    }
  }
}

/// Binomial count of successes in n trials with probability p. Exact
/// inversion: from 0 when n * min(p, 1 - p) < 10, otherwise from the mode
/// outwards. Consumes 1 value.
template <typename U>
int64_t Binomial(U& u, int64_t n, double p) {
  if (n <= 0 || !(p > 0)) {
    return 0;
  }
  if (p >= 1) {
    return n;
  }
  if (p > 0.5) {
    return n - Binomial(u, n, 1 - p);
  }
  const double q = 1 - p;
  double v = u();
  if (n * p < 10) {
    const double s = p / q, a = (n + 1) * s;
    double r = std::exp(n * std::log(q));
    int64_t x = 0;
    while (v > r && x < n) {
      v -= r;
      ++x;
      r *= a / x - s;
    }
    return x;
  }
  const int64_t mode = static_cast<int64_t>(std::floor((n + 1) * p));
  const double nd = static_cast<double>(n), md = static_cast<double>(mode);
  const double pmode = std::exp(std::lgamma(nd + 1) - std::lgamma(md + 1) -
                                std::lgamma(nd - md + 1) + md * std::log(p) +
                                (nd - md) * std::log(q));
  v -= pmode;
  if (v < 0) {
    return mode;
  }
  int64_t up = mode, down = mode;
  double pup = pmode, pdown = pmode;
  while (up < n || down > 0) {
    if (up < n) {
      pup *=
          (static_cast<double>(n - up) / static_cast<double>(up + 1)) * (p / q);
      ++up;
      v -= pup;
      if (v < 0) {
        return up;
      }
    }
    if (down > 0) {
      pdown *= (static_cast<double>(down) / static_cast<double>(n - down + 1)) *
               (q / p);
      --down;
      v -= pdown;
      if (v < 0) {
        return down;
      }
    }
  }
  return mode;  // rounding left a tiny remainder
}

/// Breit-Wigner (Cauchy) with the given location and full width; consumes 1.
template <typename U>
double BreitWigner(U& u, double mean, double gamma) {
  return mean + 0.5 * gamma * std::tan(kPi * (u() - 0.5));
}

/// Uniformly distributed point on a sphere of radius r (Marsaglia's method,
/// as in ROOT's TRandom::Sphere); consumes 2 or more values.
template <typename U>
void PointOnSphere(U& u, double r, double* x, double* y, double* z) {
  double a, b, r2;
  do {
    a = u() - 0.5;
    b = u() - 0.5;
    r2 = a * a + b * b;
  } while (r2 > 0.25);
  *z = r * (-1.0 + 8.0 * r2);
  const double scale = 8.0 * r * std::sqrt(0.25 - r2);
  *x = a * scale;
  *y = b * scale;
}

}  // namespace random_algorithms
}  // namespace bdm

#endif  // CORE_UTIL_RANDOM_ALGORITHMS_H_
