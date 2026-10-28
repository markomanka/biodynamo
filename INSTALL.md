# Installing this fork of BioDynaMo

This fork (see [SKUNKWORKS.md](SKUNKWORKS.md)) can take every dependency from a community-maintained package
ecosystem. That is the recommended way to install it:

| Method | Dependencies come from | Status |
| --- | --- | --- |
| **conda-forge** (recommended) | conda-forge, through `environment.yml` | Linux and macOS (Apple Silicon); tested by the Conda CI workflow |
| Spack | Spack, for HPC clusters | Supported by the same CMake option; not yet tested in CI |
| Upstream method | `prerequisites.sh`, Homebrew or apt, and prebuilt ROOT and ParaView bundles downloaded from CERN | Still works, but on macOS needs Python 3.9 and Qt 5, which Homebrew is withdrawing |

With conda-forge or Spack, CMake runs with `BDM_SYSTEM_DEPENDENCIES=ON`: it takes ROOT, ParaView, Python, MPI and
the other libraries from the active environment, and downloads nothing. The option is switched on automatically
when the active conda environment contains ROOT; `-DBDM_SYSTEM_DEPENDENCIES=ON` or `OFF` overrides that.

ROOT is still needed for now. It is a temporary bridge, which the fork's ROOT-free I/O removes (see the plan in
SKUNKWORKS.md); after that, `root` simply disappears from `environment.yml`.

## conda-forge

You need a conda installer: [Miniforge](https://github.com/conda-forge/miniforge) (recommended), Mambaforge or
micromamba. On macOS, also install Apple's command line tools: `xcode-select --install`.

```bash
git clone https://github.com/markomanka/biodynamo.git
cd biodynamo
conda env create -f environment.yml        # or: mamba env create / micromamba create
conda activate biodynamo
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
source build/bin/thisbdm.sh                 # in each new shell, after conda activate
bdm config
```

What the environment contains:

| Package | Role |
| --- | --- |
| `c-compiler`, `cxx-compiler`, `cmake`, `ninja`, `make`, `git`, `pkg-config` | Toolchain: GCC on Linux, Clang on macOS |
| `llvm-openmp`, `openmpi` | Shared-memory (OpenMP) and distributed (MPI) parallelism |
| `libboost-devel` | Boost-based behaviours, such as `RegulatoryNetwork` |
| `python` | Python 3.11 or later; ROOT and ParaView are built against it |
| `root` | ROOT 6.36 or later; temporary |
| `paraview` | ParaView 6.2 or later, with Qt 6 and legacy Catalyst for in-situ visualization; optional |
| `tbb-devel`, `eigen`, `expat`, `liblzma-devel`, `fast_float`, `exprtk` | Development files that ParaView's and VTK's CMake packages look for, which conda-forge does not install with them; only needed with ParaView |
| `libxslt` | `xsltproc`, which ParaView uses to document BioDynaMo's ParaView plugin; only needed with ParaView |

Optional additions:

- **Without ParaView:** delete the `paraview` line and the packages listed under it before
  creating the environment, and add `-Dparaview=OFF` to the `cmake` command.
- **NUMA awareness** (Linux, multi-socket servers): `conda install -c conda-forge libnuma` before running `cmake`.
  Without it, NUMA support is switched off with a message.
- **Memory checks** (Linux): `conda install -c conda-forge valgrind`.

To update the environment after `environment.yml` changes: `conda env update -f environment.yml --prune`.

## Spack

```bash
spack env create biodynamo
spack env activate biodynamo
spack add cmake ninja openmpi boost python root paraview+python
spack install
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DBDM_SYSTEM_DEPENDENCIES=ON
cmake --build build --parallel
```

BioDynaMo needs ROOT's Geom, Gui and GenVector libraries, and ParaView's Catalyst, PythonCatalyst and
VTKExtensionsIOCore modules. This path is not yet tested in CI; please report problems.

## Upstream method

Follow BioDynaMo's [installation guide](https://biodynamo.org/docs/userguide/installation/) and leave
`BDM_SYSTEM_DEPENDENCIES` off (outside a conda environment it is off by default). CMake then downloads ROOT and
ParaView as before. Expect it to stop working on new Macs as Homebrew disables `python@3.9` (October 2026) and
`qt@5` (May 2027).

## Troubleshooting

- **"ROOT was not found in the environment"**: the environment is not active, or it lacks ROOT. Run
  `conda activate biodynamo` and check that `root-config --version` works.
- **"ParaView was not found in the environment"**: install `paraview` in the environment, or configure with
  `-Dparaview=OFF`.
- **A build directory configured with the upstream method**: start from an empty build directory, because CMake
  remembers earlier choices in its cache.
- **`thisbdm.sh` reports a ROOT version mismatch**: the environment's ROOT changed since the build. Rebuild, or
  recreate the environment with the version used before.
