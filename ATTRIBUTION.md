# Attribution & provenance

This repository is a **clean-room HIP/ROCm port** of the MLS-midpoint projection stage of
**ScrollFiesta**.

## Upstream
- **ScrollFiesta** — virtual meshing & unwrapping for the Herculaneum papyri.
  MIT License, © 2026 **Nicholas Vining**. <https://github.com/Hob3rMallow/scrollfiesta_public>
  (MIT covers ScrollFiesta's own `src/`, `include/`, `scripts/`, `python/`.)
- **CUDA MLS reference** — GPU-accelerated MLS branch by **pscamillo**.
  <https://github.com/pscamillo/scrollfiesta_public/tree/cuda-mls>

## What is original here vs. derived
- **Original (clean-room) work:** the HIP kernel (`clean-side/impl/`), the independent FP32 oracle
  and conformance goldens (`clean-side/verification/`), and the functional spec (`dirty-side/`).
  The implementer built these from the specification and **did not read upstream's MLS source**
  (`src/mls_project_cuda.cu` / `src/common/mls_project.c`). See `CLEAN_ROOM_MANIFEST.md`.
- **Derived (MIT-attributed):** the adapter (`clean-side/adapter/`) bridges upstream's
  `MLS_project_verts` arena/coordinate ABI to the clean kernel. It references upstream's MIT-licensed
  code with file:line citations — see `clean-side/adapter/ABI_MAPPING.md`.
- **Public technique:** Moving Least Squares surface projection (Wendland C² weights, PCA-normal via
  covariance eigen-decomposition, tangent-plane projection) is a published method.

## Not included
Upstream vendors **Jonathan Shewchuk's Triangle** (`deps/src/triangle/`), which is **not** MIT and
restricts commercial distribution. It is **not** part of this repository. If you build the full
ScrollFiesta pipeline, obtain Triangle from upstream under its own terms.
