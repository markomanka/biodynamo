# BioDynaMo skunkworks: packaging, scalability, interoperability

This repository is an independent, experimental fork of [BioDynaMo](https://github.com/BioDynaMo/biodynamo).
Anything here may change or be dropped, and nothing here is an official BioDynaMo release. The fork is not
affiliated with or endorsed by the BioDynaMo team.

## Goals

1. **Packaging for general-purpose distributions.** BioDynaMo should build from source with system packages
   only (Debian and Ubuntu, Fedora, conda-forge, Spack, Homebrew), with no downloads during the build and with
   any supported Python.
2. **Scalability.** Replace ROOT I/O for checkpoints and for exchanging agents between processes with a lighter,
   validated format, as groundwork for running one simulation across many nodes.
3. **Interoperability with other agent-based simulation suites.** Read and write the formats other tools use,
   starting with PhysiCell's, and share boundary-condition and exchange semantics, so that models, data and
   analysis tools can move between suites.

## What blocks packaging today

Found while building BioDynaMo outside its prebuilt bundles (October 2026):

| Dependency | Problem for distributions | Plan |
| --- | --- | --- |
| ROOT | When no compatible ROOT is installed, the build downloads a pinned prebuilt one (6.30.02 on Linux) from cern.ch. Reflection dictionaries, the Cling interpreter, checkpoints, plotting and notebooks all depend on ROOT. | Replace it feature by feature (next section) |
| Python | The build requires one exact Python 3.9 release, because the prebuilt ROOT and ParaView bundles were built against it. Python 3.9 reached end of life in October 2025. | `-DBDM_ANY_PYTHON=ON` accepts any Python 3.9 or newer; it becomes the default once ROOT is gone |
| Open MPI | Mandatory even when no MPI feature is used | Make it optional |
| optim (bundled) | Built in C++11 mode, so it needs Armadillo older than 13 | Build it as C++14 or newer |
| ParaView | Prebuilt bundle downloaded when enabled | Use a system ParaView or VTK when present |

## Replacing ROOT

Replacements must be compatible with Apache-2.0, so GPL libraries are excluded.

| Area | Replacement | Status |
| --- | --- | --- |
| Logging, math helpers | In-house, with ROOT's output format and verbosity levels | Done (pull request #1) |
| Random numbers and distributions | Philox4x64-10 with in-house distribution algorithms; see [RANDOM-NUMBERS.md](RANDOM-NUMBERS.md) | Done; user-defined distributions still use ROOT's function classes |
| Plugin loading, system information | `dlopen`, `std::chrono`, `std::filesystem` | Planned |
| Plotting | CSV or JSON export with matplotlib; optional matplot++ in C++ | Planned |
| Notebooks | xeus-cpp Jupyter kernel; pyvista for 3D views | Planned |
| Reflection, checkpoints, parameter JSON, MPI messages | A source generator built on Clang LibTooling, and a separate I/O library with HDF5 files | Planned |

## Scalability

- ROOT I/O was built for long-term storage with schema evolution, not for exchanging agents every iteration.
  In prototype measurements (single core, synthetic agents shaped like BioDynaMo's), a generated ROOT-free
  encoder serialized agents 17–25× faster than ROOT I/O, within 1.25–1.55× of the minimum possible cost.
- Removing the Cling interpreter from the runtime also removes a per-process memory cost that
  [Breitwieser et al. (arXiv:2509.24063)](https://arxiv.org/abs/2509.24063) report at several hundred
  megabytes per MPI rank.
- One ghost-agent mechanism will serve borders between processes, periodic boundaries and symmetry planes.

## Interoperability with PhysiCell and other suites

- **Data exchange:** PhysiCell writes MultiCellDS XML with cell data in MATLAB Level 4 `.mat` files. An
  importer and exporter will let initial conditions and results move between the two suites.
- **Checkpoints:** a format complete enough for restarts in both suites; PhysiCell's current resume does not
  restore intracellular model state, cell graphs, the `is_movable` flag or custom global state.
- **Comparable results at boundaries:** closed walls clamp agents onto the wall, and chemotaxis moves agents
  at full speed along even negligible gradients. Both make agents accumulate at borders. Fixing them, with the
  old behaviour kept behind a flag, makes models comparable across suites.
- **In-situ tools:** MPI-native interfaces for tools such as ParaView Catalyst and OpenLB.

## Status

| Branch | Content | State |
| --- | --- | --- |
| `rootfree/phase1` | Logging and math helpers without ROOT; `BDM_ANY_PYTHON` option | Merged into `master` (pull request #1) |
| `rootfree/phase1-part2` | Plugin loading with `dlopen`; output-folder size without a shell | In review |
| `rootfree/rng` | Philox4x64-10 random number generator; changes stochastic results (see [RANDOM-NUMBERS.md](RANDOM-NUMBERS.md)) | In review |

## Building this fork

Follow BioDynaMo's [developer guide](https://www.biodynamo.org/developer-guide/). To build against a ROOT that does
not come from the prebuilt bundle, for example conda-forge's, add `-DBDM_ANY_PYTHON=ON`. One configuration that
builds:

```
conda create -n bdm -c conda-forge root cxx-compiler cmake ninja openmpi "armadillo<13"
conda activate bdm
cmake -S . -B build -G Ninja -DBDM_ANY_PYTHON=ON -DROOT_DIR=$CONDA_PREFIX/cmake \
  -Dparaview=OFF -Dnuma=OFF -Dboost=OFF -Dnotebooks=OFF -Dsbml=OFF
cmake --build build
```

## Working rules

- Every change goes through a pull request inside this repository. Nothing is sent to the official BioDynaMo
  repository from here; any proposal upstream will be made separately, starting with a discussion with the
  BioDynaMo team.
- Each change keeps BioDynaMo building with ROOT until the last ROOT use is removed, and BioDynaMo's own CI
  must pass.
- Commits are signed off (Developer Certificate of Origin). Work done with AI assistance is credited in the
  commit with `Co-authored-by: Claude <noreply@anthropic.com>`.
- Changes from the official BioDynaMo are merged in periodically.

## Licence and attribution

BioDynaMo is developed by the BioDynaMo collaboration (see [NOTICE](NOTICE)) and licensed under Apache-2.0;
this fork keeps that licence. The modifications are recorded in this repository's history and noted in NOTICE.
