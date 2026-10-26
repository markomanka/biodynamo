# Random numbers in this fork

This fork generates random numbers with **Philox4x64-10**, a counter-based generator, instead of ROOT's
`TRandom3` (a Mersenne Twister). Simulations stay reproducible from their seed, but the same seed now
produces a different sequence. Results of stochastic simulations therefore do not match, number for number,
runs made with the official BioDynaMo or with this fork before the switch. They should agree statistically.

## What changed

| | Before (official BioDynaMo, this fork up to branch `before-philox`) | Now |
| --- | --- | --- |
| Generator | `TRandom3` from ROOT | Philox4x64-10 (Salmon et al., SC'11); the raw stream equals `numpy.random.Philox(key=[seed, 0])` |
| Seeding per thread | thread `t` uses `random_seed * (t + 1)` | unchanged |
| Distributions | ROOT's `TRandom` algorithms | in-house algorithms in `src/core/util/random_algorithms.h` (table below) |
| User-defined distributions (`GetUserDefinedDistRng1D/2D/3D`) | sampled from ROOT's global `gRandom`, ignoring `random_seed` and shared by all threads | sampled from the simulation's per-thread generator |
| Seed 0 | picks a non-reproducible seed | same; `GetSeed()` now returns the seed actually used |
| API | `Random::SetGenerator(TRandom*)` | removed; `DistributionRng::SampleImpl` takes `Random*` |
| Checkpoints | `Random` class version 3, with the `TRandom3` state | version 4, with the Philox state |

| Distribution | Algorithm | Uniform numbers used per sample |
| --- | --- | --- |
| `Uniform`, `Integer` | top 53 bits of a 64-bit output, in [0, 1) | 1 |
| `Gaus` | Box–Muller | 2 |
| `Exp`, `BreitWigner` | inversion | 1 |
| `Landau` | Chambers–Mallows–Stuck (stable law, α = β = 1, scale π/2) | 2 or more |
| `Poisson`, `PoissonD` | multiplication method below mean 10, PTRS (Hörmann, 1993) above | variable |
| `Binomial` | exact inversion, from 0 or from the mode outwards | 1 |
| `Circle`, `Sphere` | same constructions as before | 1; 2 or more |

## What this means for reproducibility

- **Runs with this version** repeat exactly, as before, given the same seed, number of threads, version and
  settings.
- **Comparisons with earlier results** must be statistical. Use the same model and parameters, run several
  seeds, and compare distributions, means and confidence intervals rather than individual numbers. Tests or
  scripts that compare against stored outputs of stochastic runs need new reference outputs.
- **User-defined distributions** were not reproducible before, because they ignored `random_seed` and were
  shared by all threads. They are now reproducible from the seed, so results that use them change for that
  reason as well.
- **Checkpoints written before the switch** can still be restored, but the generator restarts from its
  default state (seed 4357), because the saved `TRandom3` state cannot be carried over. A run resumed from
  such a checkpoint diverges from the original run from the restore point onwards.
- **Number of threads**: as before, every thread has its own generator, so results depend on the number of
  threads and on how work is divided among them. A counter-based generator makes it possible to give each
  agent its own stream, which would remove that dependency; this is not implemented yet.

## Reproducing earlier results exactly

Use the version that produced them, with the same seed and number of threads: the official BioDynaMo
release, or this fork at branch `before-philox`. Checkpoints written by those versions continue exactly only
with those versions.

## Reproducing BioDynaMo's stream in Python

```python
import numpy as np

def uniforms(random_seed, thread, n):
    """The first n values of Random::Uniform() on thread `thread` of a simulation."""
    key = np.array([random_seed * (thread + 1), 0], dtype=np.uint64)
    return np.random.Generator(np.random.Philox(key=key)).random(n)
```

Only the uniform stream maps one to one; other distributions consume uniform numbers as listed above.

## How the generator was checked

- Its 64-bit output matches `numpy.random.Philox` exactly for several keys, across block boundaries, and a
  unit test pins the first values for the default seed.
- Means and variances of the normal, exponential, Poisson and binomial distributions match theory within
  statistical tolerance over 200,000 or more samples.
- `Landau` passes a Kolmogorov–Smirnov test against ROOT's `landau_cdf`.
- `Poisson` and `Binomial` pass chi-square tests against their exact probabilities for several parameter
  sets and seeds.
