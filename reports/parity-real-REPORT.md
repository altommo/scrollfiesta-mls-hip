# Phase 3 - Real CPU-Reference Parity Report

## Stabilized final rerun (2026-07-22)

OBSERVATION: Characterization of the original 131 Component-1 normal outliers
found three dominant spatial neighborhoods. Of the 131, 100 had
`lambda0/lambda1 > 0.9`, 13 changed radius membership, none had degenerate
status, and none triggered the `(1,1,1)` sign fallback. Exact upstream-double
simulation reproduced the CPU normal within `1.21e-6` degrees and showed that
the residual came from accumulated support-state divergence, not the final
eigensolve.

OBSERVATION: Adding compensated FP32 accumulation to the centroid pass is the
minimal contract-preserving correction. All synthetic checks and adapter smoke
tests pass after rebuilding. On the same real cube it reduces five-pass
Component-1 normal outliers from 131 to 20, max position error from `0.0132285`
to `0.00213777` vox, and RMS from `3.13694e-5` to `1.02769e-5` vox.

| Component | Passes | Max pos vox | RMS pos vox | Max normal deg | Pos outliers | Normal outliers | Weld | Strict |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| comp001 | 5 | 0.00213777 | 0.0000102769 | 0.0248666 | 2 | 20 | PASS | FAIL |
| comp001 | 20 | 0.0390429 | 0.000104177 | 1.69153 | 17 | 90 | PASS | FAIL |
| comp002 | 5 | 0.0000219138 | 0.00000678682 | 0.0000316703 | 0 | 0 | PASS | PASS |
| comp002 | 20 | 0.0000511795 | 0.0000133401 | 0.0000217670 | 0 | 0 | PASS | PASS |

OBSERVATION: The remaining five-pass set is 20/331,013 vertices. Seventeen have
`lambda0/lambda1 > 0.9`, one changes radius membership, and none are degenerate.
Its largest radius-connected group is five points versus random p95 four. The
large original pattern is removed; the residual is marginal FP32
tied-eigenspace sensitivity. Raw per-outlier evidence is in
`outputs/comp001_kahan_lop5.normal_outliers.csv`.

OBSERVATION: `hipMemGetInfo` replaces the unusable `rocm-smi` probe. It reports
16,974,905,344 total bytes and a 98,000,896-byte before/after free-memory delta
for Component 1. This is not a peak measurement.

INFERENCE: Strict per-vertex normal parity remains failed on Component 1 and is
reported honestly. Weld safety and the corrected downstream topology run are
the production acceptance evidence; see `../e2e/REPORT.md`.

## Superseded corrected-contract rerun (2026-07-22)

OBSERVATION: The original Phase-3 failure correctly identified a specification
error. Upstream performs one projection per `MLS_project_verts` call and the
caller performs 5 moving-support passes. The clean spec, oracle, goldens, kernel
ABI, and adapter were corrected and fully revalidated before this rerun.

OBSERVATION: The reference harness now links the exact MIT upstream
`src/common/mls_project.c` under the renamed symbol
`CPU_MLS_project_verts`. CPU and HIP receive the same cube-local `(z,y,x)`
arrays and original cell origin `(16128,2560,7680)`. This avoids the FP32 loss
caused by comparing world-shifted OBJ coordinates near z=16k.

OBSERVATION: `./run_checks.sh` passes all host tests, regenerated one-pass
goldens, HIP build/symbol checks, and all 10 valid synthetic HIP/oracle families
on `gfx1201`.

### Final real results

| Component | Points | Max pos (vox) | RMS pos (vox) | Max normal | Pos outliers | Normal outliers | Weld | Strict parity |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| comp001 | 331,013 | 0.0132285 | 0.0000313694 | 0.105113 deg | 8 | 131 | PASS | FAIL |
| comp002 | 1,688 | 0.0000305176 | 0.00000753617 | 0.0000314625 deg | 0 | 0 | PASS | PASS |

Thresholds are max position `0.00124`, RMS `0.00022`, normal angle `0.006`
degrees, and strict weld distance `<0.25` voxel. Both components have finite
outputs, unit nonzero normals, matching degenerate-normal masks, and pass weld
safety. Component 1 still fails strict parity on a small ambiguous subset, so
Phase 3 remains a precise divergence result rather than a full pass.

INFERENCE: The operator mismatch is fixed. Remaining comp001 divergence is
localized FP32-vs-CPU-double sensitivity in near-ambiguous covariance
neighborhoods; it is not the former fixed-support/five-internal-update error.
The full five-pass RMS is well inside tolerance and maximum displacement remains
18.8x below the weld threshold, but raw strict thresholds must not be relabelled
as passed.

OBSERVATION: The final HIP path keeps neighbor search, weights, centroid, and
compensated covariance in FP32, with FP64 used only for the 3x3 Jacobi
eigensolve. The reported operator benchmark includes that mixed-precision cost.

OBSERVATION: Phase 4 was not run because the corrected real-cube contract still
fails strict parity on comp001.

Raw final evidence:

- `outputs/comp001_final_current_lop5_compare.txt`
- `outputs/comp002_final_lop5_compare.txt`
- `outputs/comp001_benchmark_lop5.txt`
- raw CPU/HIP FP32 arrays with matching prefixes in `outputs/`

The remainder of this report is the original pre-correction investigation and
is retained as historical evidence.

## Status

OBSERVATION: Phase 3 ran on Alan-selected real cube:

`<HERCULANEUM>/cache/real-surface-mesh/scrollfiesta_grid_t050/cubes_PRED/z16128_y02560_x07680.tif`

OBSERVATION: CPU diagnostic capture completed successfully with `EXTRACT_DIAG=1` using the existing built ScrollFiesta binary:

`<HERCULANEUM>/external/scrollfiesta_public/build-wsl-tools/cube_mesh`

OBSERVATION: Local upstream build in `~/scrollfiesta-rocm/src` did not link because `../deps/lib` lacks `libtriangle`, `libClipper2`, and `libClipper2Z`. No packages were installed. Existing built artifact was used instead.

OBSERVATION: Real-cube parity FAILS at the fixed Phase 3 thresholds on both Step 0 components. Phase 4 is intentionally not run because the brief says to escalate if parity fails.

## CPU Capture Method

Command:

```bash
mkdir -p ~/mls-hip-cleanroom/parity-real/cpu_diag/dump ~/mls-hip-cleanroom/parity-real/cpu_diag/cubes
EXTRACT_DIAG=1 VESUVIUS_THREADS=2 \
  <HERCULANEUM>/external/scrollfiesta_public/build-wsl-tools/cube_mesh \
  <HERCULANEUM>/cache/real-surface-mesh/scrollfiesta_grid_t050/cubes_PRED/z16128_y02560_x07680.tif \
  ~/mls-hip-cleanroom/parity-real/cpu_diag/cubes/z16128_y02560_x07680.tif \
  --halo 13 \
  --dump-obj ~/mls-hip-cleanroom/parity-real/cpu_diag/dump \
  --no-timeout \
  2>&1 | tee ~/mls-hip-cleanroom/parity-real/cpu_diag/cube_mesh_diag.log
```

CPU run summary:

```text
cube_mesh: <HERCULANEUM>/cache/real-surface-mesh/scrollfiesta_grid_t050/cubes_PRED/z16128_y02560_x07680.tif (threads=2, halo=13)
HaloLoader: cube z16128_y02560_x07680 p_size=154 halo=13  loaded=8/27 missing=19
  Extract: 2 components (68.492s)
  Timings: extract=68.492 qem=0.077 trim=0.000 dump=0.002 total=107.614s  Status: OK
```

Captured files:

- CPU MLS inputs, world-shifted z,y,x OBJ point order:
  - `cpu_diag/dump/z16128_y02560_x07680/z16128_y02560_x07680_step0_mls/z16128_y02560_x07680_step0_mls_pre000.obj`
  - `cpu_diag/dump/z16128_y02560_x07680/z16128_y02560_x07680_step0_mls/z16128_y02560_x07680_step0_mls_pre001.obj`
- CPU MLS outputs with normals:
  - `cpu_diag/dump/z16128_y02560_x07680/z16128_y02560_x07680_step0_post_lop_points/z16128_y02560_x07680_post_lop_points_comp001.obj`
  - `cpu_diag/dump/z16128_y02560_x07680/z16128_y02560_x07680_step0_post_lop_points/z16128_y02560_x07680_post_lop_points_comp002.obj`

INFERENCE: The OBJ point order is the index correspondence for comparison. The upstream dump writer emits `v z y x` and `vn nz ny nx`; source comments in `src/common/dump_obj.h` and `src/common/dump_obj.c` document this convention.

## HIP Comparison Method

OBSERVATION: `run_real_parity.cpp` was created under `~/mls-hip-cleanroom/parity-real/`. It reads CPU pre/post OBJ files, runs the Phase 2 adapter once with `radius=12.0` and `cell_origin={0,0,0}` on world-coordinate z,y,x points, writes raw f32 artifacts, and reports the fixed Phase 3 thresholds:

- max position: `1.24e-3` vox
- RMS position: `2.2e-4` vox
- max normal angle: `0.006` degrees
- weld position: `0.25` vox

Build command:

```bash
cd ~/mls-hip-cleanroom/parity-real
g++ -O2 -std=c++17 \
  -I../cleanroom/clean-side/adapter \
  -I../cleanroom/clean-side/impl \
  -c run_real_parity.cpp -o run_real_parity.o
HSA_ENABLE_DXG_DETECTION=1 ROCPROFILER_REGISTER_ENABLED=0 \
  /usr/bin/hipcc --offload-arch=gfx1201 \
  run_real_parity.o \
  ../cleanroom/clean-side/adapter/build/mls_scrollfiesta_adapter.o \
  ../cleanroom/clean-side/adapter/build/libmls_project_clean_renamed.a \
  -o run_real_parity
```

OBSERVATION: One-step `hipcc` compile+link failed because it treated `.o`/`.a` inputs as HIP source. The compile-object then link sequence above succeeded.

## Results

| Component | Vertices | HIP wall ms | Max pos vox | RMS pos vox | Max normal angle deg | Weld pass | Parity pass |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| comp001 | 331,013 | 1578.21 | 8.51003 | 1.73742 | 179.704 | false | false |
| comp002 | 1,688 | 271.564 | 2.29486 | 0.245393 | 26.6448 | false | false |

Raw comparator outputs:

```text
component=comp001
count=331013
hip_wall_ms=1578.21
nonfinite_count=0
max_position_error_vox=8.51003 threshold=0.00124 worst_index=329058
rms_position_error_vox=1.73742 threshold=0.00022
max_normal_angle_deg=179.704 threshold=0.006 worst_index=117754
max_normal_length_error=1
weld_max_position_error_vox=8.51003 strict_threshold=0.25
weld_pass=false
parity_pass=false
```

```text
component=comp002
count=1688
hip_wall_ms=271.564
nonfinite_count=0
max_position_error_vox=2.29486 threshold=0.00124 worst_index=30
rms_position_error_vox=0.245393 threshold=0.00022
max_normal_angle_deg=26.6448 threshold=0.006 worst_index=41
max_normal_length_error=1.10349e-07
weld_max_position_error_vox=2.29486 strict_threshold=0.25
weld_pass=false
parity_pass=false
```

Raw f32 artifacts:

- `outputs/comp001.pre.positions_zyx.f32`
- `outputs/comp001.cpu.positions_zyx.f32`
- `outputs/comp001.cpu.normals_zyx.f32`
- `outputs/comp001.hip.positions_zyx.f32`
- `outputs/comp001.hip.normals_zyx.f32`
- `outputs/comp002.pre.positions_zyx.f32`
- `outputs/comp002.cpu.positions_zyx.f32`
- `outputs/comp002.cpu.normals_zyx.f32`
- `outputs/comp002.hip.positions_zyx.f32`
- `outputs/comp002.hip.normals_zyx.f32`

## Divergence Analysis

OBSERVATION: Upstream ScrollFiesta calls `MLS_project_verts` once per LOP iteration and rebuilds the neighborhood/support cloud from the previous iteration's output. `~/scrollfiesta-rocm/src/extract/mesh_extract.c:1019-1028` loops over `MLS_PROJECT_ITERS`; `~/scrollfiesta-rocm/src/common/pipeline_constants.h:73-77` sets that to 5.

OBSERVATION: The clean HIP kernel performs five internal iterations in one call using the arena support points fixed at arena creation. `~/mls-hip-cleanroom/cleanroom/clean-side/impl/mls_project_hip.hip:445-473` contains the internal five-iteration loop.

INFERENCE: The largest likely cause of the Phase 3 failure is semantic, not GPU execution: CPU is doing five Jacobi-style whole-cloud iterations with refreshed support each pass; the clean HIP kernel is doing five query updates against one fixed support cloud. That is not the same MLS operator.

OBSERVATION: comp001 also reports `max_normal_length_error=1`, meaning at least one HIP normal is zero while CPU reference normal is unit length. The adapter currently allows clean `MLS_CLEAN_STATUS_DEGENERATE_NEIGHBORHOOD` and copies outputs.

INFERENCE: Degenerate-neighborhood handling/status propagation also needs tightening before another parity attempt.

## Conclusion

Phase 3 produced the requested real-cube divergence report. It does not pass parity. The next engineering step is to resolve the iteration/support-cloud contract before Phase 4:

- either add a validated clean-side one-iteration API and let ScrollFiesta keep its five caller iterations;
- or change the attributed upstream-copy caller to execute one clean five-iteration call and compare against a CPU reference configured to the same fixed-support semantics.

Until that is resolved, downstream topology and end-to-end speedup numbers would not be meaningful.
