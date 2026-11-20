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
| 3 · I/O and reflection | `abm-gen` (Python with Clang's parser, `libclang`, replacing `rootcling`) and `abm-io`: HDF5 checkpoints, JSON parameters, MPI exchange, a type registry, and field tables replacing Cling's run-time code generation; then plugin loading through `dlopen`; then `rootcling` and ROOT leave the build; `abm-io` is released and gets a PhysiCell adapter | ROOT gone completely | Planned |
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
  ([root-feedstock#396](https://github.com/conda-forge/root-feedstock/issues/396), open, with no reply by
  15 November 2026: the ParaView export no longer depends on it, the notebooks do until step 2, see
  [INSTALL.md](INSTALL.md)); ParaView 6.2.0 used Abseil 20260526 without pinning it
  ([paraview-feedstock#345](https://github.com/conda-forge/paraview-feedstock/issues/345), fixed for new builds);
  with ROOT in the environment, solvers still pick old builds, because ROOT requires an older gl2ps than the new
  builds' VTK ([paraview-feedstock#348](https://github.com/conda-forge/paraview-feedstock/issues/348),
  [root-feedstock#397](https://github.com/conda-forge/root-feedstock/issues/397), open: `environment.yml` pins
  `libabseil=20260526` meanwhile).
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
| File container | HDF5; whether through HighFive or HDF5's C API is decided by tests and measurements in `abm-io`; ADIOS2 only if HPC users need it |
| Who writes visualization files | `abm-io`, as VTKHDF from the generated field tables |
| Dependency ecosystem | conda-forge by default, Spack for HPC clusters |
| ParaView | conda-forge's ParaView 6.2 (legacy Catalyst) now; Catalyst 2 later; replaceable |
| Plugin loading | The ParaView adaptor is registered by its library and loaded with `dlopen` (pull request #35, brought forward with the end of the run-time code generation; see below); other adaptor names stay with ROOT's plugin handlers until step 3 |

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
| Cling's run-time code generation (agent export) | Array types compiled into the library and chosen by the data member's type; then generated field tables in place of ROOT's dictionaries | Run-time generation removed (pull request #35), which ended the ParaView export's need for a macOS 14 SDK; field tables in step 3 |
| Plugin loading | A registry of adaptors and `dlopen` | ParaView adaptor done (pull request #35); ROOT's plugin handlers for other adaptors in step 3 |
| Building | ROOT, `rootcling` and the bundle downloads leave CMake | Step 3 |

### Why plugin loading and the run-time code generation left ROOT together

Leaving plugin loading alone was tried and reverted, and the reason is a dependency rather than a preference, so
it is recorded here rather than only in the history (commit `1a2fb970`).

The ParaView agent export compiled code at run time with Cling, in `JitForEachDataMemberFunctor`. ROOT's
plugin manager loads the adaptor with `gSystem->Load`, which tells Cling about the library, and creates the
adaptor by interpreting its constructor, which makes Cling load the adaptor's declarations. A bare `dlopen`
does neither. On Linux Cling still found what it needed; on macOS it did not: with the adaptors opened by
`dlopen`, `FLAKY_ParaviewIntegrationTest.ExportAgents_*` died on every attempt on macOS 26 with hundreds of
Cling errors ("expected expression"), and a macOS 15 job hung until the six-hour limit. Through the plugin
manager the same tests pass in 8 to 25 seconds with no Cling errors. That attempt left no code behind;
`shared_library.h` and its test were removed with the revert.

So the two had to leave ROOT together, and that was planned for step 3, with the generated field tables. Found
again by Aravind Raju during his review of v1.06, from the source rather than from this document, which is why
the reasoning lives here.

**Brought forward in November 2026 (pull request #35).** The same review showed that the ParaView export did
not work on current Macs at all: conda-forge's ROOT cannot read system headers with a macOS SDK of version 15
or newer, only with a 14 SDK, which current versions of macOS and Xcode do not provide. The incompatibility
was reported upstream on 30 October 2026
([conda-forge/root-feedstock#396](https://github.com/conda-forge/root-feedstock/issues/396)) and by
15 November had received no reply. Rather than wait, the part of step 3 that removes the interpreter from the
ParaView export was done at once, both halves together:

- **Run-time code generation.** The data member types the export supports are few and fixed (`real_t`, `int`,
  `uint64_t`, `AgentUid`, `AgentPointer`, and `MathArray` of the three numeric types), so `vtk_agents.cc`
  compiles one array type for each and chooses by the member's type. A member of another type stops the
  simulation with a message that names it.
- **Plugin loading.** `VisualizationAdaptor::Register` records how to create an adaptor; the ParaView library
  registers itself when loaded, and `VisualizationAdaptor::Create` loads it with `dlopen` from the folder of
  `libbiodynamo`. Nothing needs Cling to know about the adaptor any more, which is why `dlopen` is now enough.

What stays for step 3: the names, types and offsets of data members still come from ROOT's dictionaries, to be
replaced by the generated field tables, and adaptors other than ParaView's still go through ROOT's plugin
handlers. The tests were written to outlast that change. `ParaviewExportContractTest` looks only at the
exported files (which arrays, with which types and values), and `VisualizationAdaptorTest` only at what any
way of loading adaptors must do; neither depends on ROOT's dictionaries or interpreter. On macOS the Conda CI
runs the ParaView tests and an export from a project outside the tree with the active SDK, and fails if the
dependency on the interpreter comes back.

What this does not cover: ROOT's notebook kernel interprets every cell and is still subject to the same
incompatibility on macOS. It is not worked around; the notebooks move to the `xeus-cpp` kernel in step 2, as
planned.

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
| `fix/out-of-tree-projects` | Projects outside the source tree: C++ standard, NUMA, Homebrew's libomp; a CI check | Merged (pull request #12) |
| `docs/release-1.06` | [RELEASE-1.06.md](RELEASE-1.06.md): release notes and adoption steps for the consortium | Merged (pull request #15); version 1.06 tagged as `v1.06` (pull request #18) |
| `mm/release-1.07` | Version 1.07, the ParaView export without ROOT's interpreter: [RELEASE-1.07.md](RELEASE-1.07.md) | Tagged `v1.07` on `master` |
| `before-philox` | The state before the random-number switch, kept to reproduce earlier results | Reference branch |

## What the continuous integration covers

[CI-COVERAGE.md](CI-COVERAGE.md) lists what the checks of a pull request cover and what they leave out, with
the reason for each gap and the condition for closing it.

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
