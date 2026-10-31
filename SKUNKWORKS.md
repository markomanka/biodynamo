# BioDynaMo skunkworks: ROOT-free, easy to install, interoperable

This repository is an independent, experimental fork of [BioDynaMo](https://github.com/BioDynaMo/biodynamo).
Anything here may change or be dropped, and nothing here is an official BioDynaMo release. The fork is not
affiliated with or endorsed by the BioDynaMo team.

## Goals

1. **BioDynaMo without ROOT.** No feature may depend on ROOT: not checkpoints, parameters, the exchange of
   agents between processes or reflection, and not notebooks, statistics, plotting or visualization either.
2. **A shared I/O library.** Checkpoints, agent exchange and visualization files move into
   [abm-io](https://github.com/markomanka/abm-io) (DOI
   [10.5281/zenodo.23731697](https://doi.org/10.5281/zenodo.23731697)), a library that other agent-based suites
   can adopt as well, starting with PhysiCell.
3. **Easy installation and maintenance.** Every dependency comes from a community-maintained ecosystem:
   conda-forge by default, Spack on HPC clusters. Nothing is downloaded at build time.
4. **Performance and scalability are kept.** Every step is benchmarked, and the I/O work aims to make
   multi-node runs faster, not slower.

Visualization keeps using ParaView, taken from conda-forge. BioDynaMo will write open file formats and use
open in-situ interfaces, so that ParaView can be replaced if a competitive interactive 3D tool appears.

## Plan

| Step | Content | Result | Status |
| --- | --- | --- | --- |
| 1 · Community dependencies | `BDM_SYSTEM_DEPENDENCIES` option; `environment.yml`; ParaView adaptor ported to ParaView 6.2 and VTK 9.7; Conda CI on Linux and macOS; [INSTALL.md](INSTALL.md) | No Python 3.9, Qt 5 or CERN downloads. ROOT stays only as a bridge until step 3 | Done (pull request #8) |
| 2 · ROOT-free features | Plotting and statistics (`TimeSeries`, `LineGraph`) export their data, with plots from Vega-Lite specifications in notebooks or matplotlib; notebooks on the `xeus-cpp` kernel; user-defined distributions without `TF1`/`TF2`/`TF3`; the ROOT geometry viewer removed; the remaining `TMath`, `TEnv` and `TSystem`; demos | No user-facing feature needs ROOT | Planned |
| 3 · I/O and reflection | `abm-gen` (a Clang LibTooling generator, replacing `rootcling`) and `abm-io`: HDF5 checkpoints, JSON parameters, MPI exchange, a type registry, and field tables replacing Cling's run-time code generation; then plugin loading through `dlopen`; then `rootcling` and ROOT leave the build; `abm-io` is released and gets a PhysiCell adapter | ROOT gone completely | Planned |
| 4 · Visualization independence | `abm-io` writes VTKHDF (HDF5-based VTK files, read by ParaView and PyVista) from the field tables, without linking VTK; Catalyst 2 for in-situ use; evaluation of alternatives to ParaView | ParaView optional and replaceable | Planned |

Physics fixes for agents accumulating at borders run in parallel (see below). `abm-io`'s
`docs/ROADMAP.md` maps these steps onto its milestones, gates and issues.

### Step 1 in practice

- **Result:** BioDynaMo builds and passes all its tests with every dependency from conda-forge: 456 tests on
  Ubuntu 24.04 and 450 on macOS 15 (Conda CI). It is built as C++20, the standard of conda-forge's ROOT.
- **Problems the move uncovered, fixed in BioDynaMo:** ROOT's interpreter, which in conda-forge's ROOT uses
  runtime C++ modules, needs BioDynaMo's headers on `ROOT_INCLUDE_PATH`; `pvbatch` is taken from the `PATH` when
  `ParaView_DIR` is not set; the Linux ParaView tests render through Xvfb, as in upstream's CI; on macOS,
  Python's library is kept despite conda-forge's `-dead_strip_dylibs`; `environment.yml` takes the CPU build of
  Viskores, whose CUDA builds would require NVIDIA's compiler.
- **Reported to conda-forge:** ParaView 6.2.0 for macOS linked a library that no package ships
  ([paraview-feedstock#342](https://github.com/conda-forge/paraview-feedstock/issues/342), fixed in build 2);
  ROOT 6.40.04 cannot include `<stdint.h>` with a macOS 15 or newer SDK
  ([root-feedstock#396](https://github.com/conda-forge/root-feedstock/issues/396), open: CI and
  [INSTALL.md](INSTALL.md) use a macOS 14 SDK meanwhile); ParaView 6.2.0 uses Abseil 20260526 without pinning it
  ([paraview-feedstock#345](https://github.com/conda-forge/paraview-feedstock/issues/345), open:
  `environment.yml` pins `libabseil=20260526` meanwhile).
- **BioDynaMo's own macOS CI is retired:** its bundles depend on Homebrew's `python@3.9`, which Homebrew disabled
  on 15 October 2026. The Conda CI covers macOS (pull request #10).
- **GPUs:** the Viskores choice affects only ParaView's accelerated filters; BioDynaMo's own CUDA and OpenCL
  options are untouched. Next come a CUDA variant of the environment and a test of BioDynaMo's CUDA path with
  the new toolchain. BioDynaMo has no HIP backend for AMD GPUs.

### Decisions taken

| Question | Decision |
| --- | --- |
| Scope of the ROOT removal | Everything, including notebooks, statistics, plotting and visualization |
| Random numbers | Philox4x64-10; results stay reproducible from the seed, but differ from ROOT's (see [RANDOM-NUMBERS.md](RANDOM-NUMBERS.md)) |
| File container | HDF5, through HighFive; ADIOS2 only if HPC users need it |
| Who writes visualization files | `abm-io`, as VTKHDF from the generated field tables |
| Dependency ecosystem | conda-forge by default, Spack for HPC clusters |
| ParaView | conda-forge's ParaView 6.2 (legacy Catalyst) now; Catalyst 2 later; replaceable |
| Plugin loading | Stays with ROOT until step 3: with `dlopen`, Cling's agent export failed on macOS |

## Where ROOT is used today

Measured on `master` in October 2026: 114 files, about 470 uses.

| Area | Files | Uses | What it is | Step |
| --- | --- | --- | --- | --- |
| Library (`src/`) | 57 | 246 | Class registration for I/O (`BDM_CLASS_DEF`, `TRootIOCtor`), reflection, checkpoints, the Cling-based agent export, plotting (`TGraph`, `TCanvas`), `TF1` | 2, 3 |
| CMake | 13 | 88 | Bundle download, dictionaries (`rootcling`), `ROOTSYS` | 3 |
| Demos | 12 | 54 | Plots and ROOT files | 2 |
| Notebooks | 15 | 39 | ROOT's notebook kernel and its JavaScript plots (JSROOT) | 2 |
| Tests, docs, scripts, other | 22 | 43 | Assorted | 2, 3 |

## Replacing ROOT

Replacements must be compatible with Apache-2.0, so GPL libraries are excluded, and must be packaged on
conda-forge and Spack.

| Area | Replacement | Status |
| --- | --- | --- |
| Logging, math helpers | In-house, with ROOT's output format and verbosity levels | Done (pull request #1) |
| Random numbers and distributions | Philox4x64-10 with in-house distribution algorithms | Done (pull request #4); user-defined distributions in step 2 |
| System calls | `std::filesystem` | Folder size done (pull request #3); `TEnv` and `TSystem` in step 2 |
| Plotting and statistics | Data export (CSV, HDF5); Vega-Lite specifications rendered by Jupyter; matplotlib for static figures | Step 2 |
| Notebooks | The `xeus-cpp` kernel (conda-forge) instead of ROOT's; PyVista for 3D views | Step 2 |
| ROOT geometry viewer | Removed; ParaView and the file export cover its uses | Step 2 |
| Reflection, checkpoints, parameter JSON, MPI messages | `abm-gen` and `abm-io`, with HDF5 files and nlohmann/json | Step 3 |
| Cling's run-time code generation (agent export) | Generated field tables | Step 3 |
| Plugin loading | `dlopen` | Step 3, after the field tables |
| Building | ROOT, `rootcling` and the bundle downloads leave CMake | Step 3 |

## Visualization

- **Now:** ParaView 6.2 from conda-forge, built with Qt 6. It still contains legacy Catalyst, the in-situ
  interface BioDynaMo uses, and all of BioDynaMo's ParaView code compiles against it; only the zero-copy agent
  array needed porting, because VTK 9.7 removed `vtkMappedDataArray`. The port also builds against the VTK of
  the old ParaView 5.10 bundle.
- **Files:** `abm-io` will write VTKHDF from the field tables. Any tool that reads VTKHDF (ParaView, PyVista,
  others) can then open BioDynaMo output, and BioDynaMo no longer needs VTK to write it.
- **In situ:** Catalyst 2 replaces legacy Catalyst. conda-forge has no Catalyst 2 package (`libcatalyst`) yet;
  Spack has one, and contributing one to conda-forge is part of the plan.
- **Alternatives:** interactive 3D tools that could replace ParaView will be evaluated on performance with large
  agent counts, in-situ support, licence and packaging.

## Scalability

- ROOT I/O was built for long-term storage with schema evolution, not for exchanging agents every iteration.
  In prototype measurements (single core, synthetic agents shaped like BioDynaMo's), a generated ROOT-free
  encoder serialized agents 17–25× faster than ROOT I/O, within 1.25–1.55× of the minimum possible cost.
- Removing the Cling interpreter from the runtime also removes a per-process memory cost that
  [Breitwieser et al. (arXiv:2509.24063)](https://arxiv.org/abs/2509.24063) report at several hundred
  megabytes per MPI rank.
- One ghost-agent mechanism will serve borders between processes, periodic boundaries and symmetry planes.
- NUMA awareness and MPI stay available with conda-forge (`libnuma`, Open MPI) and Spack.

## Interoperability with PhysiCell and other suites

- **Data exchange:** PhysiCell writes MultiCellDS XML with cell data in MATLAB Level 4 `.mat` files. An
  importer and exporter in `abm-io` will let initial conditions and results move between the two suites.
- **Checkpoints:** a format complete enough for restarts in both suites; PhysiCell's current resume does not
  restore intracellular model state, cell graphs, the `is_movable` flag or custom global state.
- **Comparable results at boundaries:** closed walls clamp agents onto the wall, and chemotaxis moves agents
  at full speed along even negligible gradients. Both make agents accumulate at borders. Fixing them, with the
  old behaviour kept behind a flag, makes models comparable across suites.
- **Visualization:** VTKHDF output and Catalyst 2 are open to every suite, not only BioDynaMo.

## Status

| Branch | Content | State |
| --- | --- | --- |
| `rootfree/phase1` | Logging and math helpers without ROOT; `BDM_ANY_PYTHON` option | Merged (pull request #1) |
| `rootfree/phase1-part2` | Output-folder size without a shell; plugin loading back on ROOT until step 3 | Merged (pull request #3) |
| `rootfree/rng` | Philox4x64-10 random number generator; changes stochastic results | Merged (pull request #4) |
| `docs/random-numbers` | [RANDOM-NUMBERS.md](RANDOM-NUMBERS.md) | Merged (pull request #5) |
| `rootfree/version-tags` | Version detection ignores tags that are not version numbers | Merged (pull request #6) |
| `rootfree/notices` | Copyright and modification notices; NumPy credit | Merged (pull request #7) |
| `ci/maintenance` | Node.js 24 actions; no spurious Homebrew errors | Not merged; its Homebrew part is moot since the old macOS CI was retired |
| `step1/community-deps` | Step 1: dependencies from conda-forge or Spack | Merged (pull request #8) |
| `ci/retire-macos` | Old macOS CI retired; Abseil pinned until ParaView is rebuilt | Merged (pull request #10) |
| `before-philox` | The state before the random-number switch, kept to reproduce earlier results | Reference branch |

## Installing this fork

See [INSTALL.md](INSTALL.md). In short, with conda-forge:

```bash
conda env create -f environment.yml && conda activate biodynamo
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel
source build/bin/thisbdm.sh
```

BioDynaMo's own installation method, with prebuilt bundles from CERN, still works on Linux. On macOS it no
longer does: it depends on Homebrew's `python@3.9`, which Homebrew disabled on 15 October 2026 (`qt@5` follows
in May 2027).

## Working rules

- Every change goes through a pull request inside this repository. Nothing is sent to the official BioDynaMo
  repository from here; any proposal upstream will be made separately, starting with a discussion with the
  BioDynaMo team.
- BioDynaMo keeps working at every step: its own Ubuntu CI and the Conda CI (Linux, macOS) must pass, and a
  feature is replaced before its ROOT version is removed. Its own macOS CI was retired when Homebrew disabled
  `python@3.9`.
- Performance and scalability are checked against recorded baselines before a step is merged.
- Changes that alter results on purpose are documented, with a way to reproduce the old results.
- Commits are signed off (Developer Certificate of Origin). Work done with AI assistance is credited in the
  commit with `Co-authored-by: Claude <noreply@anthropic.com>`.
- Changes from the official BioDynaMo are merged in periodically.

## Licence and attribution

BioDynaMo is developed by the BioDynaMo collaboration (see [NOTICE](NOTICE)) and licensed under Apache-2.0;
this fork keeps that licence. The modifications are recorded in this repository's history and noted in NOTICE.
Third-party code adapted here is listed in NOTICE, with its licences in `third_party/licenses/`. Files added in
this fork carry their own copyright line below BioDynaMo's header, and modified files say so below it.
