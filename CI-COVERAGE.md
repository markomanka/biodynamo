# What the continuous integration checks, and what it does not

This fork merges a change only when every check of its pull request has passed. That rule is worth as much as
the checks behind it, so this page lists what they cover and, more importantly, what they leave out: for each
gap, why it is there and what has to be true before it is closed. Whoever changes a workflow, switches a test
off or finds something the CI overlooks updates this page in the same pull request.

State: 16 November 2026, after pull request #35.

## What runs

| Workflow | Runs for | Machines | What it checks |
| --- | --- | --- | --- |
| Conda CI | Pull requests to `master`; pushes to `master`; by hand | Ubuntu 24.04, macOS 15 (Apple Silicon) | The recommended installation: every dependency from conda-forge, nothing downloaded during the build. The whole unit test suite; a project built outside the source tree; an installed copy, its unit tests, and `cell_division` run from it with and without the ParaView export. On macOS also the ParaView tests and that export with the active SDK and no macOS 14 SDK |
| Ubuntu CI | Pull requests to `master`; pushes to `master` | Ubuntu 24.04, 26.04 | Upstream's installation, with the bundled ROOT and ParaView: build with ParaView, benchmarks and libgit2; the whole unit test suite |
| Ubuntu System CI | Pull requests to `master` | Ubuntu 24.04, 26.04 | Upstream's installation again: a project made with `biodynamo new`; the unit tests under valgrind (24.04 only); every demo; the system tests in `test/system`; the notebooks (24.04 only) |
| repository-check | Pull requests to `master`; pushes to `master` | Ubuntu 22.04 | clang-format 14 and the copyright headers. It stays on 22.04, whose default clang-format is version 14; on 24.04 the default is newer and formats differently. Before 22.04's runners are retired, it must install clang-format 14 explicitly or the code be reformatted |
| Singularity Integration | Pull requests; pushes to `master` | Ubuntu | The Singularity image builds and runs |

A pull request is checked once: pushes to other branches start nothing, because the pull request runs the same
checks on the same commit.

Ubuntu 22.04 was dropped from the Ubuntu CI and the Ubuntu System CI on 17 November 2026 (patch 0061). Its standard
support ends in spring 2027, and the fork's users install on 24.04 and newer. In its last days it failed with the
flocking crash described below in every one of the eight attempts of run 44082356609, and with the matrix's
fail-fast setting each such failure also cancelled the 24.04 and 26.04 jobs; fail-fast is now off, so every system
reports its own result. The notebooks moved to 24.04 with it.

The Ubuntu System CI did not run in this fork before 14 November 2026. GitHub had kept it switched off, and it
started when pull request #32 changed its file. Valgrind, the demos and the notebooks were therefore first
exercised here on that day, and what they found is recorded below.

## Switched off in this fork

| Workflow | State | Note |
| --- | --- | --- |
| macOS CI, macOS System CI | By hand only, since pull request #10 | Upstream's installation on macOS, through Homebrew and the bundles; last run on 30 October 2026, failed. The Conda CI covers macOS instead |
| Sonar Source Analysis | Disabled by hand | Needs a SonarCloud project and token that the fork does not have |
| Build and push devcontainer image | Disabled by GitHub, as in every fork | Never ran here |
| Update gitpod integration repository | By hand only | Not used |

Nothing runs on a schedule. Upstream ran the Ubuntu System CI every night at 3:00 UTC; here `master` changes
only through pull requests, which have already run it. GitHub did start the nightly run in the fork, but late:
on 15 November 2026 it began at 10:28 UTC.

## What is not checked

"Next" in the last column means a change that is planned now; the entry is rewritten when it lands.

| What | State | Why | Reinstate when |
| --- | --- | --- | --- |
| The demos with dependencies from conda-forge | Not run. The Conda CI runs `cell_division` only; the other demos run with upstream's bundles | Never added when the Conda CI was written | Next: the demos in the Conda CI's Linux job |
| Single precision (`-Dreal_t=float`) | Not built anywhere | Only the Ubuntu 20.04 leg used `float`, and upstream removed that leg (#408) | Next: one leg of the Ubuntu System CI built with `float` |
| Errors a demo prints | Ignored. A demo passes when it prints "Simulation completed successfully!" and at least one demo wrote a ParaView state file. Known case: `flocking` sets `pos_gamma` to 10000 in `bdm.json`, ROOT cannot find the type name written there (`bdm::MathArray<real_t,3>`), and the demo silently runs with the default, 1000. It was not the cause of the flocking crash (see "Explained") | The demo script looks for nothing else | Next: correct the file; then make such an error fail the demo |
| The unit tests on macOS with the active SDK | The main macOS steps point ROOT at the runner's macOS 14 SDK; only the ParaView tests and one export run with the active SDK | Left from before pull request #35, when the ParaView export needed that SDK | Next: the whole macOS job with the active SDK |
| GPU code: CUDA, OpenCL, `mechanical_forces_op_gpu_test`, `cell_division_gpu` | Not built, not run | No runner has a GPU | A machine with a GPU is available as a self-hosted runner |
| SBML: `-Dsbml=ON`, `sbml_integration` | Not built, not run | Off in every workflow; its dependency is not in `environment.yml` | Step 2 decides whether SBML stays |
| macOS beyond the Conda CI job: demos, system tests, notebooks; macOS 26; Intel Macs | Not run | The old macOS workflows are by hand only, and macOS runners are scarce | By hand before each release; a macOS 26 leg in the Conda CI once the macOS job is reliable to obtain |
| In-situ visualization on macOS | Six tests compiled out (`#ifndef __APPLE__` in `integration_test.cc`) | As in upstream: not supported there | In-situ visualization is supported on macOS |
| Spack | Not tested | No workflow | A user needs BioDynaMo on a cluster through Spack |
| Five unit tests: `DISABLED_DiffusionTest.WrongParameters` and `.ModelInitializer`, `DISABLED_Force.GeneralSphereCylinder` and `.GeneralCylinder`, `DISABLED_NeuronSomaNeuriteElementTest.Displacement` | Switched off in the source | As in upstream; no reason recorded | Tried one at a time: each is switched on, or deleted with the reason |
| Notebooks with dependencies from conda-forge | Not run; they run with upstream's bundles on Ubuntu 24.04 only | They use ROOT's notebook kernel | Step 2, with the `xeus-cpp` kernel |
| Valgrind on part of the unit tests | Excluded in `cmake/Testing.cmake`: the death tests and the ParaView integration tests, which start other processes, and, as in upstream with no reason recorded, `IOTest.InvalidRead`, `SchedulerTest.Backup`, `ResourceManagerTest.SortAndForEachAgentParallel*`, `InlineVector*`, `NeuriteElementBehaviour.*`, `MechanicalInteraction.*`, `DiffusionTest.*Convergence*` and `AgentVectorTest.Equality` | Other processes cannot be followed; the rest is inherited | The inherited exclusions are tried one at a time, like the switched-off tests |
| Valgrind on the demos | Not run: valgrind runs on the unit tests only | The demos take minutes each without valgrind and would take hours with it | A demo that misbehaves only in some runs, as `flocking` did: run that one under valgrind in a throwaway workflow |
| Valgrind on more than one machine | Ubuntu 24.04 only | The other legs check the same code and each took 10 to 15 minutes | Not planned |
| cpplint and clang-tidy | Run on Ubuntu 22.04 for pull requests, as reports: they cannot fail the job | As in upstream | Not planned |
| Benchmarks | Built, never run | Timings on shared runners mean little | A dedicated machine is available |

## Explained

- **`flocking` crashed in the Ubuntu System CI** with a segmentation fault in
  `UniformGridEnvironment::AssignToBoxesFunctor`, first in job 132518363490 (Ubuntu 24.04) on 14 November 2026, and
  from 17 November on Ubuntu 22.04 in nearly every run. 960 runs outside the System CI did not reproduce it. The
  grid check added for it (patch 0060) named the cause in job 134803235513: boids at position (nan, nan, nan) at
  the first update. `Boid::acceleration_accum_scalar` was read in the first step before it had ever been set, and
  whenever the memory happened to hold a NaN, the boid's position became NaN. Fixed in patch 0060 by starting it at
  zero. The grid now stops a simulation with "Agent ... is outside the grid" and the agent's details instead of
  crashing, for any agent it cannot place.

  Valgrind would have reported the uninitialized read, but it runs only on the unit tests, not on the demos: see
  the table above.

## Reading a failed check

- A macOS job that is cancelled with "The job was not acquired by Runner of type hosted" never started: GitHub
  had no machine. Run it again. Nothing else is rerun before its log has been read.
- Valgrind reports on the thread-local storage of threads started with `std::thread` are suppressed in
  `util/valgrind-bdm.supp` (pull request #34): the allocation is made by glibc while it creates the thread.
