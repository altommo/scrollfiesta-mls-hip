# Clean-room provenance and evidence manifest

Prepared: 2026-07-22

## Boundary used

The prohibited implementation materials were:

- `src/mls_project_cuda.cu`
- `src/common/mls_project.c`
- implementation-equivalent upstream MLS source

No agent was authorized to inspect, search, quote, hash, disassemble, or derive
implementation details from those materials.

The dirty-side spec writer used only the supplied dossier, permitted public
README/benchmark/sample metadata, published MLS literature, and available
black-box observations. No local CPU binary or clone was present in this
environment, so no upstream black-box observation was actually performed.

The HIP implementer and independent oracle engineer were started without
conversation history. Each was instructed to read only
`dirty-side/functional-spec.md` as its normative dirty-side input. The
implementer later received only the independently created clean verifier's
`MLS_*` runner interface fields so the two clean components could connect.
Neither clean-side agent read `dirty-side/validation-notes.md`.

## Agent outputs and attestations

| Role | Output | Attested boundary |
|---|---|---|
| Dirty specification | `dirty-side/functional-spec.md` | Neither prohibited MLS source file accessed |
| Dirty validation | `dirty-side/validation-notes.md` | Neither prohibited MLS source file nor equivalent accessed |
| Clean HIP implementation | `clean-side/impl/` | Only the frozen functional spec used for numerical behaviour |
| Clean independent verification | `clean-side/verification/` | Only the frozen functional spec used; upstream observations recorded as none |

## What is implemented

The clean HIP component provides:

- the exact unmangled C signature `MLS_project_verts(void*, const float*,
  size_t, float, const float*, float*, float*)`;
- a separate clean arena API accepting packed FP32 `(x,y,z)` support points;
- a deterministic host-built uniform spatial hash uploaded once per arena;
- one GPU thread per query vertex;
- one projection per public ABI call, plus an explicitly parameterized clean
  fixed-support primitive for validation;
- Wendland C2 weights, weighted-centroid pass, centered weighted-covariance
  pass, FP32 symmetric Jacobi eigensolve, smallest-eigenvalue normal,
  `(1,1,1)` sign orientation, and tangent-plane projection;
- reusable arena-owned query/output buffers, an arena-owned stream, synchronous
  outputs on ABI return, and separate status/introspection functions;
- `gfx1201` Makefile integration, archive/object/symbol checks, and a real raw
  fixture runner.

The independent verification component provides:

- a separately written C++17 FP32 oracle;
- 15 deterministic fixture/golden families;
- checksummed raw `MLSGOLD` input/output storage with metadata;
- max/RMS position error, maximum normal-angle error, finite/status, and strict
  0.25-voxel weld checks;
- fixed comparator thresholds of `1.24e-3` voxel max, `2.2e-4` voxel RMS, and
  `0.006` degrees maximum normal error;
- stable result exit codes and an external HIP runner protocol.

## Evidence obtained here

The following completed successfully in this environment:

```sh
make -C clean-side/verification -j4 \
  test integration-test hip-interface-test goldens
```

The unit suite printed `all clean-room MLS oracle tests passed`. Oracle
round-trip and mock runner-interface comparisons both reported zero non-finite
values, zero non-OK statuses, zero position/normal error, parity `PASS`, weld
safety `PASS`, and exit code 0.

The implementation's public header passed strict C11 syntax checking. Its real
runner passed strict C++17 host compilation with warnings treated as errors.
The HIP translation unit passed a host-syntax parse using a temporary clean
runtime stub, and its Jacobi solver passed axis/oblique-plane smoke checks.
The Makefile dry-run confirms `hipcc --offload-arch=gfx1201` and the intended
object, archive, runner, and symbol checks.

This environment does not contain ROCm, `hipcc`, or a GPU device. Consequently
there is no claim here that the HIP translation unit compiled, that the symbol
was observed with `nm` on a real HIP object, that a kernel executed, or that any
performance figure was measured.

## Release blockers that must not be relabelled as assumptions

1. Exact upstream `arena` type, layout, support-point ownership, lifetime, and
   construction path.
2. Array coordinate order and voxel-centre convention at this private ABI.
3. Exact role of `cell_origin`.
4. Black-box confirmation that the target selects the smallest covariance
   eigenvector despite the dossier's ambiguous word `dominant`.
5. Exact `(1,1,1)` polarity tie and emitted-normal timing.
6. Degenerate-neighbourhood and aliasing behaviour if the real caller can reach
   those cases.
7. Raw upstream CPU MLS input/output capture with original index correspondence.

Until items 1-3 are resolved, the object is not an unchanged-caller drop-in.
Until items 4-7 and the golden comparison pass, upstream numerical parity is
not proven.

## Remaining acceptance evidence

On the target WSL machine:

1. Run `./run_checks.sh` to compile for `gfx1201` and compare every valid
   synthetic family against the independent oracle.
2. Through an authorized public caller or black-box fixture, capture the actual
   arena contract and raw CPU-reference MLS arrays without inspecting prohibited
   implementation source.
3. Add a clean adapter for that observed/public arena contract, then run the raw
   comparator at the fixed numerical thresholds.
4. Feed CPU and HIP positions through the unchanged downstream stages and record
   component, vertex/face, seam-weld, and mesh audit results.
5. Benchmark warmed MLS ABI time, full cube, multi-cube throughput, and peak VRAM
   with complete hardware/compiler/concurrency metadata.

The currently indexed public repository reportedly advertises an MIT licence,
which differs from the supplied dossier's no-licence premise. This manifest
makes no legal conclusion: record the exact reference commit/branch and retain
the clean-room boundary unless the project owner deliberately changes it.

## 2026-07-22 operator correction

Real-reference Phase 3 proved that the original clean specification was wrong:
both the kernel and independent oracle implemented five internal updates against
fixed support, while upstream performs one update per call and refreshes support
between five caller-level LOP passes (20 in resplit). The corrected spec, oracle,
goldens, kernel ABI, and adapter are recorded in `OPERATOR_CORRECTION.md`.
