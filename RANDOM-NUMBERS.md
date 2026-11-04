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
| `Uniform`, `Integer` | top 53 bits of a 64-bit output, in [0, 1); `Uniform` redraws an exact 0, keeping ROOT's open interval (0, max) | 1 |
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
- **Number of threads**: as before, every thread has its own generator (`Simulation::GetRandom()` returns
  `random_[omp_get_thread_num()]`), so results depend on the number of threads and on how work is divided
  among them. A counter-based generator makes it possible to give each agent its own stream, which would
  remove that dependency; this is not implemented yet. See below.

A model can be invariant even though the engine is not. If a model draws its random numbers in an order it
fixes itself, for example by collecting the agents that will draw, sorting them by a stable identifier and
drawing for them in that order, its results do not depend on the number of threads. That costs a sort and
is worth it wherever runs must be compared across machines or thread counts.

## Per-agent streams: what would remove the dependency

Philox computes its output from a key and a counter rather than from a state carried forward, so a draw can
be addressed instead of being taken in turn: draw *n* for agent *a* at step *s* can be a pure function of
the seed and those three numbers. The stream then follows the agent rather than the thread, and results no
longer depend on the number of threads, on how work is divided among them, or, in a distributed run, on
which process owns the agent.

What that needs is a key that is stable across threads, processes and restarts. BioDynaMo's `AgentUid`
today is an index with a reuse counter, unique within one process; two agents on different processes, or an
agent before and after a reuse, can share a key. A suitable key is an identifier unique for the whole run
and never reused, which is what the shared I/O library planned in [SKUNKWORKS.md](SKUNKWORKS.md) defines for
its own purposes. So the ingredient is already on the way, and the change belongs with step 3, where that
library is brought in, rather than before it.

The gain would be worth having: a run reproducible from its seed alone, whatever machine, thread count or
number of processes it ran on. That is a stronger reproducibility guarantee than BioDynaMo has ever
offered, and it is the main reason the generator was changed at all.

## Bringing an existing model to this version

Almost certainly nothing. A model that draws through BioDynaMo's `Random` (`Uniform`, `Gaussian`,
`Poisson`, the distribution helpers, the user-defined distributions) compiles unchanged and runs. Its
numbers differ; its code does not. None of BioDynaMo's demos or tests needed a change.

Two interfaces did change, and both fail at compile time rather than silently:

| If the model does this | What to do |
| --- | --- |
| calls `Random::SetGenerator(TRandom*)` to install its own generator | Remove the call: the generator is no longer replaceable, and `random_seed` now covers everything, including the user-defined distributions it previously bypassed |
| defines a distribution by subclassing `DistributionRng` and overriding `SampleImpl(TRandom*)` | Change the parameter to `Random*`, and draw with that object's methods instead of ROOT's |

One command tells a model author which case applies, in seconds:

```bash
grep -rnE "SetGenerator|SampleImpl" path/to/model
```

What does need attention is not code but references. **Tests that compare against stored output of a
stochastic run will fail**, because the same seed now gives a different sequence. Those references must be
regenerated with this version; a failure of that kind says nothing about the model.

And a point worth separating from the mechanics: a model whose conclusions change when the stream changes
was never reproducible in any useful sense, only self-consistent. Running an established model on both
versions and comparing distributions is therefore a check on the model as much as on BioDynaMo, and a
cheap one.

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

Only the uniform stream maps one to one; other distributions consume uniform numbers as listed above. BioDynaMo's `Uniform()` redraws an exact 0.0, which numpy returns with probability 2⁻⁵³ per value, so the
two sequences could only part after such a value.

## How the generator was checked

- Its 64-bit output matches `numpy.random.Philox` exactly for several keys, across block boundaries, and a
  unit test pins the first values for the default seed.
- Means and variances of the normal, exponential, Poisson and binomial distributions match theory within
  statistical tolerance over 200,000 or more samples.
- `Landau` passes a Kolmogorov–Smirnov test against ROOT's `landau_cdf`.
- `Poisson` and `Binomial` pass chi-square tests against their exact probabilities for several parameter
  sets and seeds.
