# Phase 1 BENCH_REPORT

## Stabilized FP32 operator benchmark (2026-07-22, final)

The compensated-centroid build supersedes every benchmark below. It preserves
the specified FP32 operator while removing the large clustered real-cube
residual documented in `../parity-real/REPORT.md`.

| Path | Median wall ms | Measured reps | Speedup |
| --- | ---: | ---: | ---: |
| Upstream CPU, 16 threads, 5 moving-support passes | 14,699.1 | 6 | 1.00x |
| HIP adapter, full rebuild/transfer path, 5 moving-support passes | 877.134 | 6 | **16.76x** |

CPU reps (ms): `14364.1, 14467.5, 14692.6, 14705.5, 14826.1, 14823.8`.

HIP reps (ms): `880.809, 877.087, 874.950, 875.675, 877.180, 879.041`.

Raw output: `../parity-real/outputs/comp001_kahan_benchmark_lop5.txt`.

`hipMemGetInfo` reported 16,974,905,344 total bytes and a 98,000,896-byte
free-memory decrease from immediately before to immediately after the HIP run.
This is a retained-memory snapshot, not a peak-VRAM claim. `rocm-smi` remains
unusable under WSL/DXG and is no longer used as a probe.

### Accumulated parity

| Component | Passes | Max pos vox | RMS pos vox | Max normal deg | Pos outliers | Normal outliers | Weld |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| comp001 (331,013) | 5 | 0.00213777 | 0.0000102769 | 0.0248666 | 2 | 20 | PASS |
| comp001 (331,013) | 20 | 0.0390429 | 0.000104177 | 1.69153 | 17 | 90 | PASS |
| comp002 (1,688) | 5 | 0.0000219138 | 0.00000678682 | 0.0000316703 | 0 | 0 | PASS |
| comp002 (1,688) | 20 | 0.0000511795 | 0.0000133401 | 0.0000217670 | 0 | 0 | PASS |

OBSERVATION: Component 1 accumulates a small, spatially correlated
ambiguous-normal residual at 20 passes, but both 5- and 20-pass positions remain
inside the 0.25-voxel weld threshold. Component 2 passes every strict threshold
at both pass counts.

## Superseded corrected moving-support benchmark (2026-07-22)

The former `18.70x` result below predates compensated centroid accumulation and
is retained only as historical evidence.

The original benchmark below measured the wrong five-internal/fixed-support
operator and is superseded by this section.

OBSERVATION: Workload is real component 1 from cube
`z16128_y02560_x07680`: 331,013 points, radius 12, five caller-level LOP
passes. Every pass rebuilds the support grid from the previous output. HIP time
includes adapter conversion, host sorting/grid construction, H2D, kernel, D2H,
and arena destruction for all five passes.

OBSERVATION: The CPU path is the exact upstream MIT
`src/common/mls_project.c`, linked under a renamed symbol and run with 16 OpenMP
threads. CPU and HIP were measured in one process for six repetitions after one
warmup on `gfx1201` with `HSA_ENABLE_DXG_DETECTION=1`.

| Path | Median wall ms | Measured reps | Speedup |
| --- | ---: | ---: | ---: |
| Upstream CPU, 16 threads, 5 moving-support passes | 15,659.7 | 6 | 1.00x |
| HIP adapter, full rebuild/transfer path, 5 moving-support passes | 837.244 | 6 | **18.70x** |

Raw CPU reps (ms): `15334.6, 15664.8, 15690.7, 15683.2, 15654.5, 15507.1`

Raw HIP reps (ms): `835.457, 835.904, 839.835, 838.584, 841.051, 829.006`

Raw output: `../parity-real/outputs/comp001_benchmark_lop5.txt`.

OBSERVATION: `rocminfo` reports `gfx1201`. Peak VRAM remains unavailable under
WSL/DXG because `rocm-smi --showmeminfo vram` reports that the amdgpu driver is
not initialized; no VRAM number is inferred.

INFERENCE: The corrected operator-level result is in the expected ~20x range
despite rebuilding and transferring the arena five times. Persistent/cached
arena work remains an optimization, not a prerequisite for the measured target.

## Superseded original benchmark

## OBSERVATION
- Workload: `310000` support points and `310000` query points, radius `12`, iterations `5`.
- Workload source: generated regular wavy surface in `<HOME>/mls-hip-cleanroom/bench/run_phase1_bench.py`.
- Raw arrays: `<HOME>/mls-hip-cleanroom/bench/work/bench.samples.f32`, `<HOME>/mls-hip-cleanroom/bench/work/bench.queries.f32`, `<HOME>/mls-hip-cleanroom/bench/work/bench.params.txt`.
- CPU command: `python3 <HOME>/mls-hip-cleanroom/bench/run_phase1_bench.py` using bench-local Numba parallel CPU MLS on the same generated surface.
- HIP throughput command: `MLS_* env <HOME>/mls-hip-cleanroom/bench/bench_hip_loop`, which creates the clean arena once and times repeated `MLS_project_verts` calls in-process.
- HIP build command: `/usr/bin/hipcc --offload-arch=gfx1201 -O3 -std=c++17 -I../cleanroom/clean-side/impl -c bench_hip_loop.cpp -o bench_hip_loop.o && /usr/bin/hipcc --offload-arch=gfx1201 bench_hip_loop.o ../cleanroom/clean-side/impl/build/libmls_project_clean.a -o bench_hip_loop`.
- Clean kernel was not edited.

| Path | Median wall ms | Measured reps | Speedup vs CPU |
|---|---:|---:|---:|
| CPU Numba parallel MLS oracle | 634.441 | 6 | 1.00x |
| HIP clean runner, process+arena per rep | 316.154 | 6 | 2.01x |
| HIP clean runner, in-process project-only | 48.364 | 6 | 13.12x |

## Hardware Evidence

`rocminfo | grep gfx`:

```text
Name:                    gfx1201                            
      Name:                    amdgcn-amd-amdhsa--gfx1201         
      Name:                    amdgcn-amd-amdhsa--gfx12-generic
```

`rocm-smi --showmeminfo vram`:

```text
ERROR:root:Driver not initialized (amdgpu not found in modules)
```

## Raw CPU Output

```text
rep=0 cpu_total_ms=2267.294 threads=16
rep=1 cpu_total_ms=632.027 threads=16
rep=2 cpu_total_ms=634.870 threads=16
rep=3 cpu_total_ms=658.811 threads=16
rep=4 cpu_total_ms=655.558 threads=16
rep=5 cpu_total_ms=634.012 threads=16
rep=6 cpu_total_ms=628.315 threads=16
```

## Raw HIP Output: Process + Arena

```text
rep=0 hip_wall_ms=1005.494
rep=1 hip_wall_ms=332.842
rep=2 hip_wall_ms=288.934
rep=3 hip_wall_ms=323.960
rep=4 hip_wall_ms=288.817
rep=5 hip_wall_ms=319.093
rep=6 hip_wall_ms=313.216
```

## Raw HIP Output: In-Process Project Only

```text
rep=0 hip_project_ms=61.9387
rep=1 hip_project_ms=47.9805
rep=2 hip_project_ms=47.7861
rep=3 hip_project_ms=48.0616
rep=4 hip_project_ms=48.8887
rep=5 hip_project_ms=48.8658
rep=6 hip_project_ms=48.6673
arena_points=310000 query_capacity=524288 cell_size=6 device_id=0
```

## Build Output

```text
make: Entering directory '<HOME>/mls-hip-cleanroom/cleanroom/clean-side/impl'
cc -std=c11 -Wall -Wextra -Werror -I. -x c -fsyntax-only mls_project_clean.h
test -s build/mls_project_hip.o
file build/mls_project_hip.o
build/mls_project_hip.o: ELF 64-bit LSB relocatable, x86-64, version 1 (SYSV), not stripped
readelf -h build/mls_project_hip.o >/dev/null
test -s build/libmls_project_clean.a
ar t build/libmls_project_clean.a | grep -Fx 'mls_project_hip.o'
mls_project_hip.o
test -x build/mls_clean_runner
nm -g --defined-only build/mls_project_hip.o | awk '$3 == "MLS_project_verts" { found=1 } END { exit !found }'
nm -g --defined-only build/mls_project_hip.o | awk '$3 == "mls_clean_arena_create" { found=1 } END { exit !found }'
nm -g --defined-only build/mls_project_hip.o | awk '$3 == "mls_clean_arena_destroy" { found=1 } END { exit !found }'
nm -g --defined-only build/mls_project_hip.o | awk '$3 == "mls_clean_last_status" { found=1 } END { exit !found }'
nm -g --defined-only build/mls_project_hip.o | awk '$3 == "mls_clean_arena_get_info" { found=1 } END { exit !found }'
readelf -Ws build/mls_project_hip.o | grep -Eq '[[:space:]]MLS_project_verts$'
make: Leaving directory '<HOME>/mls-hip-cleanroom/cleanroom/clean-side/impl'
```

## INFERENCE
- The project-only clean HIP speedup over this measured 16-thread CPU baseline is `13.12x`.
- This clears the Phase 1 escalation threshold (`<5x`) but is below the brief's expected `~20x` headline. A likely contributor is that the CPU baseline here is a generated regular-grid workload with an optimized direct neighbor enumeration, not the upstream irregular point-cloud/OpenMP baseline cited in `BENCHMARKS.md`.
- `rocm-smi` does not expose VRAM through this WSL/DXG stack; `rocminfo` does expose `gfx1201`.
