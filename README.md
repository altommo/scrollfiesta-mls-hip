# ScrollFiesta MLS → HIP (AMD ROCm / RDNA4)

A **clean-room HIP/ROCm port** of [ScrollFiesta](https://github.com/Hob3rMallow/scrollfiesta_public)'s
**MLS-midpoint projection** kernel — the ~83% runtime hotspot of its Herculaneum-scroll meshing
pipeline — so it runs on AMD GPUs. **Hardware-validated on a Radeon RX 9070 (gfx1201, RDNA4).**

The MLS kernel here is an original implementation written from a functional specification, not a
translation of upstream's CUDA source (see [`CLEAN_ROOM_MANIFEST.md`](CLEAN_ROOM_MANIFEST.md)). It
exposes the same `MLS_project_verts` C ABI as upstream, so it drops in via the included adapter.

## Results (RX 9070 / gfx1201, ROCm 7.2.4)

| Measurement | Result |
|---|---|
| MLS kernel throughput | **16.76×** vs multi-threaded CPU |
| Single-cube end-to-end | **2.46×** |
| Multi-cube (16 concurrent) | **5.81×** — near the Amdahl ceiling (MLS = 83% ⇒ cap ~5.9–6.4×) |
| Mesh parity | topologically equivalent (components / boundary loops / Euler / manifoldness / exact vertex count), weld-safe |
| Peak VRAM (16-way) | ~1.75 GiB |

FP32 GPU results differ from FP32 CPU by a few ULPs; the correct acceptance bar is **topological
equivalence + weld safety**, not bit-exact match (exact match through a greedy QEM decimator is a
hard FP32 limit). Full evidence in [`reports/`](reports/).

## What's here

```
clean-side/impl/          HIP MLS kernel (mls_project_hip.hip) + Makefile + runner + C header
clean-side/verification/  Independent FP32 oracle, 15 conformance goldens (.mlsg), comparator
clean-side/adapter/       Drop-in adapter for ScrollFiesta's MLS_project_verts ABI (+ ABI_MAPPING.md)
dirty-side/               Functional spec the clean side was built from
run_checks.sh             Build for gfx1201 + run all conformance families vs the oracle
reports/                  Benchmark, real-cube parity, and end-to-end reports
CLEAN_ROOM_MANIFEST.md    Clean-room provenance + evidence
OPERATOR_CORRECTION.md    The one-pass-per-call + caller-level-LOP operator fix
```

## Build & test

Requires ROCm with a `gfx1201`-capable `hipcc` (adjust `ARCH` for other AMD GPUs).

```sh
# build the kernel + library + runner
make -C clean-side/impl ARCH=gfx1201

# full conformance: compile for the GPU + compare every synthetic family vs the independent oracle
./run_checks.sh
```

Use it as a drop-in via `clean-side/adapter/` — the `MLS_project_verts` symbol replaces upstream's
`common/mls_project.o`, and ScrollFiesta's own caller loop supplies the 5-pass (extract) / 20-pass
(resplit) moving-support LOP semantics. A standalone LOP helper is provided for benchmarking.

### Integration note — Clipper2 ABI

ScrollFiesta's own build (both its `CMakeLists.txt` and its hand-written `src/Makefile`) links
**both** `Clipper2` (non-USINGZ, 16-byte `Point64`) and `Clipper2Z` (USINGZ, 24-byte). Its C++
wrappers (`clipper2_wrap.cpp`, `multicut_wrap.cpp`) compile with a single `USINGZ` state, and both
Clipper variants export `AddPaths_` with the same mangled name — so the linker (ODR) can bind
`Clipper2_union` to a mismatched-layout `AddPaths_`. It then strides the point buffer with the wrong
`Point64` size and overruns the vertex allocation: an ASan-confirmed heap-buffer-overflow on every
union call, which in a normal build can silently corrupt geometry rather than crash.

When you build ScrollFiesta with this backend, **link only the Clipper variant matching how the
wrappers are compiled**: the `src/Makefile` path (no `-DUSINGZ`, 16-byte) links only `-lClipper2`;
the CMake path (`CLIPPER2_USINGZ`, 24-byte) links only `Clipper2Z`. Fixed upstream in
[Hob3rMallow/scrollfiesta_public#5](https://github.com/Hob3rMallow/scrollfiesta_public/pull/5).

## Attribution & license

This port is **MIT-licensed** ([`LICENSE`](LICENSE)). It ports the *algorithm and ABI* of
ScrollFiesta (**MIT © 2026 Nicholas Vining**); the HIP kernel/oracle/goldens are original work. See
[`ATTRIBUTION.md`](ATTRIBUTION.md). Moving Least Squares surface projection is a published technique.

Upstream: <https://github.com/Hob3rMallow/scrollfiesta_public> ·
CUDA MLS reference (pscamillo): <https://github.com/pscamillo/scrollfiesta_public/tree/cuda-mls>
