# BioDynaMo 1.06: on community-maintained dependencies

*Release notes for the BioDynaMo consortium, to help decide whether to adopt this version. The version is
tagged `v1.06`; the release criteria it was checked against are in
[issue #13](https://github.com/markomanka/biodynamo/issues/13).*

*Marco Manca, with Claude (Anthropic). [SKUNKWORKS.md](SKUNKWORKS.md) describes the fork and its plan.*

## In short

BioDynaMo 1.06 is upstream BioDynaMo, as of 23 October 2026 (after pull request BioDynaMo/biodynamo#499),
changed so that it builds and runs with every dependency taken from community-maintained packages, from
conda-forge by default, instead of the ROOT, ParaView and Qt bundles that the project built and hosted itself.
Features and interfaces are those of 1.05, with the exceptions listed below. All tests pass on Linux and on
macOS (Apple Silicon).

**One change affects results:** random numbers now come from the Philox4x64-10 generator instead of ROOT's
`TRandom3`. Simulations remain reproducible from their seed, but the same seed gives a different sequence than
in 1.05, so stochastic results agree with earlier ones statistically, not number for number. This change is
separable from the rest (see *Taking only part of it*).

## Why

- **The prebuilt bundles have reached their limit.** BioDynaMo's own installation method downloads ROOT,
  ParaView and Qt builds made for it. On macOS they depend on Homebrew's `python@3.9`, which Homebrew disabled on
  15 October 2026, so new Macs can no longer install BioDynaMo that way, and BioDynaMo's macOS CI stopped working.
  Keeping the bundles alive means rebuilding them for every new compiler, operating system and Python.
- **Communities already maintain all of it.** conda-forge publishes ROOT 6.40, ParaView 6.2, VTK 9.7, Python,
  Open MPI, Boost and their dependencies, rebuilt continually for Linux and macOS. One `environment.yml` describes
  a complete, reproducible environment; Spack offers the same for HPC clusters.
- **It is the foundation for what follows.** The fork's later steps remove ROOT altogether and make ParaView
  optional (see [SKUNKWORKS.md](SKUNKWORKS.md)). 1.06 deliberately stops before those steps, so that it can be
  evaluated as BioDynaMo as you know it, only easier to install and keep working.

## What changes for users

### Installation

The recommended installation creates a conda-forge environment and builds BioDynaMo in it:

```bash
conda env create -f environment.yml
conda activate biodynamo
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
source build/bin/thisbdm.sh
```

CMake detects ROOT in the active environment and switches on `BDM_SYSTEM_DEPENDENCIES`, which takes every
dependency from the environment and downloads nothing. [INSTALL.md](INSTALL.md) gives the details, a Spack
recipe, and troubleshooting. The upstream method, with `prerequisites.sh` and the bundles, still works where it
worked before; with a ROOT not taken from the bundle, `-DBDM_ANY_PYTHON=ON` accepts any Python 3.9 or later
instead of the bundle's exact version.

### C++ standard

With conda-forge's ROOT, BioDynaMo and the projects built against it compile as **C++20**, the standard ROOT
itself was built with; mixing standards breaks ROOT's dictionaries. Model code written for C++17 compiles as
C++20 in nearly all cases.

### Random numbers

Details are in [RANDOM-NUMBERS.md](RANDOM-NUMBERS.md). In brief:

- **Generator:** Philox4x64-10, a counter-based generator; its raw stream equals NumPy's `Philox` with the same
  key, so a run's random numbers can be reproduced in Python. Per-thread seeding is unchanged.
- **Results:** reproducible from the seed, number of threads and version, as before, but different from 1.05.
  Comparisons with earlier results must be statistical; tests that compare against stored outputs of stochastic
  runs need new reference outputs.
- **Distributions:** implemented in BioDynaMo instead of taken from ROOT, with the same interfaces and contracts,
  and checked against theory (means, variances, Kolmogorov–Smirnov and chi-square tests).
- **User-defined distributions** previously ignored `random_seed` and were shared by all threads; they are now
  reproducible from the seed, so results that use them change for that reason too.
- **Interface:** `Random::SetGenerator(TRandom*)` is removed, and `DistributionRng::SampleImpl` takes a `Random*`.
  Model code that used either needs a small change.
- **Checkpoints** store the new generator's state (class version 4). Checkpoints from 1.05 still restore, but
  their generator restarts from its default state, so a resumed run diverges from the original from that point.
- **Exact reproduction of earlier results** uses the version that produced them: 1.05, or the fork's branch
  `before-philox`.

### Projects built outside BioDynaMo's source tree

Projects, such as models created with `biodynamo new`, now compile with BioDynaMo's own C++ standard, no
longer require libnuma when BioDynaMo was built without it, and no longer pick up Homebrew's OpenMP on macOS
when OpenMP comes from the environment. The Conda CI builds and runs a demo outside the source tree to keep it
that way.

### ParaView and visualization

ParaView 6.2 from conda-forge, with legacy Catalyst, replaces the bundled ParaView. BioDynaMo links only the
ParaView and VTK libraries it uses, its ParaView plugin declares its dependencies, and `pvbatch` is found on the
`PATH` when `ParaView_DIR` is not set. `thisbdm.sh` now puts BioDynaMo's headers on `ROOT_INCLUDE_PATH`, which
conda-forge's ROOT needs for its interpreter.

### Optional NUMA support

Without libnuma, BioDynaMo builds without NUMA support and says so, instead of failing; `conda install libnuma`
restores it on multi-socket Linux servers.

### Retired

BioDynaMo's macOS CI and macOS System CI workflows are retired, since their bundles need Homebrew's
`python@3.9`; the new Conda CI workflow covers macOS. The Ubuntu CI is unchanged.

## What does not change

Agents, behaviours, operations, the scheduler, diffusion, the neuroscience module, visualization, notebooks and
the command-line tool work as in 1.05, and ROOT remains the I/O and reflection layer. Model code needs no change
unless it used the two random-number interfaces above. The version also keeps upstream's licence, Apache 2.0;
modified files carry a notice saying so, as the licence requires.

## Known issues

- **Linux, Abseil pin.** conda-forge's current ParaView build needs VTK 9.7.1, which requires gl2ps 1.4.3, while
  ROOT still requires gl2ps 1.4.2; solvers therefore fall back to an older ParaView build that uses Abseil
  20260526 without declaring it. `environment.yml` pins `libabseil=20260526` until conda-forge rebuilds ROOT
  ([root-feedstock#397](https://github.com/conda-forge/root-feedstock/issues/397)) or corrects the old builds
  ([paraview-feedstock#348](https://github.com/conda-forge/paraview-feedstock/issues/348)).
- **macOS, ROOT's interpreter with a macOS 15 or newer SDK.** conda-forge's ROOT 6.40.04 cannot include
  `<stdint.h>` with those SDKs ([root-feedstock#396](https://github.com/conda-forge/root-feedstock/issues/396));
  INSTALL.md shows how to point ROOT at a macOS 14 SDK meanwhile.
- **Spack** is supported by the same CMake option but not yet tested in CI.
- **GPUs:** BioDynaMo's CUDA and OpenCL options are untouched but have not been tested with the new toolchain.

## How it was tested

- **Continuous integration:** upstream's Ubuntu CI, and a new Conda CI on Ubuntu 24.04 and macOS 15 that builds
  with every dependency from conda-forge and runs the whole test suite (456 tests on Linux, 450 on macOS), the
  ParaView integration tests (on Linux through Xvfb, as upstream does), and a project built outside the tree.
- **The build downloads nothing.** On Linux the Conda CI configures and builds inside a network namespace where
  only the loopback interface is up, so any download attempt fails. Two things had to be fixed for this to
  hold: OptimLib's configure script cloned Armadillo from GitLab whenever it found no Armadillo headers, so
  Armadillo now comes from conda-forge (and OptimLib builds as C++14, which current Armadillo requires); and
  the installation failed on a normal build because it copied a documentation folder that only exists when the
  documentation is built.
- **The installation is tested as such:** BioDynaMo is installed into a prefix, the build folder is moved away,
  and the test suite and a freshly copied demo then run from the installation alone.
- **An independent numerical check:** a model of monolayer growth, described once and implemented in both this
  BioDynaMo and PhysiCell, gave results that agree within run-to-run randomness (at most 0.35 % apart in colony
  diameter over 13 simulated days), while the model's own choices changed the outcome by about 10 %.

## How the changes were made

The changes were developed in the fork's pull requests, each built and tested by CI before it was merged.
Commit messages explain what each change does and why. The commits were written with Anthropic's AI assistant
Claude, and carry a `Co-authored-by` line saying so; the fork's maintainer reviewed and merged each of them and
signs them off (`Signed-off-by`). Consortium policies on AI-assisted contributions apply accordingly.

## If the consortium adopts this version

1. **Review the changes.** From a clone of upstream BioDynaMo:

   ```bash
   git remote add skunkworks https://github.com/markomanka/biodynamo.git
   git fetch skunkworks tag v1.06
   git log --oneline --no-merges master..v1.06    # about 40 commits, each with its reasoning
   git diff --stat master v1.06
   ```

2. **Bring them in** on a branch, keeping the history and its explanations, and open a pull request in
   BioDynaMo's repository so that upstream's CI runs on it:

   ```bash
   git switch -c adopt-1.06 master
   git merge --no-ff v1.06 -m "Merge BioDynaMo 1.06: community-maintained dependencies"
   ```

   No conflicts are expected while upstream's `master` has not moved past BioDynaMo/biodynamo#499.

3. **Continuous integration.** Keep `.github/workflows/conda-ci.yml`, which needs no secrets; decide whether to
   delete the retired macOS workflows; keep the Ubuntu CI.
4. **Release.** Tag 1.06 upstream, publish these notes with it, and make the conda-forge installation the
   recommended method in the user guide on biodynamo.org; INSTALL.md can serve as its text.
5. **Tell users about the random numbers.** The release announcement should say that stochastic results differ
   from 1.05 from the same seed, why, and how to reproduce earlier results exactly (1.05, or `before-philox`).
6. **The prebuilt bundles** can stay available for earlier versions; new versions no longer need them.
7. **Optional next steps,** with which the fork's maintainer is glad to help: a conda-forge package of BioDynaMo
   itself, so that `conda install biodynamo` works; a Spack package; a CUDA variant of the environment.

### Taking only part of it

The random-number change and the dependency work touch separate files (apart from documentation and a
copyright notice in a comment), so a version without the new generator can be prepared on request and checked
in CI before it is offered. The dependency work is the larger benefit; the generator change is a prerequisite of
the later ROOT-free steps, not of this release.

## Appendix: changes by pull request

| Pull request | Change |
| --- | --- |
| #1, #3 | Logging and math helpers without ROOT; visualization plugins loaded as before, through ROOT |
| #4, #5 | The Philox4x64-10 generator and in-house distributions, documented in RANDOM-NUMBERS.md |
| #6 | The version number ignores tags that are not versions |
| #7 | Copyright and modification notices; credit to NumPy for the Poisson sampler |
| #8 | Every dependency from conda-forge or Spack: `BDM_SYSTEM_DEPENDENCIES`, `environment.yml`, ParaView 6.2 and VTK 9.7, ROOT's C++ standard, `ROOT_INCLUDE_PATH`, macOS fixes, the Conda CI |
| #10 | macOS CI and macOS System CI retired |
| #12 | Projects outside the source tree: C++ standard, NUMA, Homebrew's libomp on macOS; a CI check |
| #2, #9 | The fork's purpose and plan (SKUNKWORKS.md) |
