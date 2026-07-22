# Phase 4 - Downstream Mesh and End-to-End Report

Date: 2026-07-22

## Controlled build

OBSERVATION: CPU and HIP release binaries were built out of tree from the same
current ScrollFiesta checkout, commit `ad21bdc2b540def8da3a63c71ccafbb7fa23096f`.
Every linked object is shared except `common/mls_project.o`: CPU links exact
upstream MLS and HIP links the clean adapter/kernel. The available external
`cube_mesh` binary was rejected because its checkout is at a different commit.

Input is the accepted real cube `z16128_y02560_x07680.tif`, halo 13, QEM
enabled, 16 CPU/OpenMP threads. Both runs completed successfully and emitted
full stage OBJ dumps.

## End-to-end result

| Metric | CPU | HIP | Delta |
| --- | ---: | ---: | ---: |
| Wall time | 88.38 s | 35.97 s | **2.46x** |
| Pipeline components after split | 42 | 42 | 0 |
| Final pipeline components | 7 | 7 | 0 |
| Final OBJ vertices | 1,455 | 1,455 | 0.000% |
| Final faces | 1,850 | 1,853 | +0.162% |
| Face-connected components | 19 | 19 | 0 |
| Closed boundary loops | 19 | 19 | 0 |
| Non-manifold edges | 0 | 0 | 0 |
| Euler characteristic | 19 | 19 | 0 |

VERDICT: Phase 4 passes the production topology bar. Topology invariants,
component parity, vertex count, and weld safety pass. The `+3/1850 = +0.162%`
face delta is accepted QEM-greediness under FP32, consistent with upstream's
published CUDA topology delta.

## Divergence location

OBSERVATION: Extraction, resplit, and split reach identical high-level counts:
2 extracted components, 9 after resplit, and 42 after split. Before QEM, hole
fill reconverges both paths to exactly 54,386 OBJ vertices, 104,933 faces, 42
face components, 43 closed boundary loops, Euler characteristic 41, and zero
non-manifold edges.

OBSERVATION: The pre-guard trim reports 1,453 vertices on both paths; the guard
pinch split produces 1,455 vertices in both final OBJs. One hole-fill decision
differs in the log, but the complete
post-hole-fill counts and topology match. QEM then chooses different
coordinate-sensitive edge collapses: CPU `5469/7754` versus HIP `5458/7732`
pipeline vertices/faces. Final trim reconverges vertex count and all topology
invariants but leaves the three-face count delta.

INFERENCE: The measured difference is a simplification/triangulation-count
delta, not a changed connected surface topology, and is accepted under the
production topology contract.

## Weld evidence

| Component | Input points | CPU post-weld | HIP post-weld | Delta |
| --- | ---: | ---: | ---: | ---: |
| comp001 | 331,013 | 139,917 | 139,915 | -2 (-0.00143%) |
| comp002 | 1,688 | 935 | 935 | 0 |

Both post-weld count deltas are inside `+-0.01%`. Real parity also keeps every
position below the 0.25-voxel weld epsilon at both 5 and 20 passes.

## Resource evidence

OBSERVATION: The operator benchmark reports a 98,000,896-byte
`hipMemGetInfo` before/after free-memory delta with 16,974,905,344 total bytes
visible. It is a retained-memory snapshot, not peak VRAM. `rocm-smi` is not used
under WSL/DXG.

OBSERVATION: Single-cube end-to-end speedup is 2.46x. Multi-cube throughput is
reported separately below.

## Sixteen-way throughput

The harness follows upstream `BENCHMARKS.md` section 4: 16 concurrent
`cube_mesh` processes, one CPU thread per process, `--halo 0 --no-qem
--no-timeout`, and identical CPU/HIP commands except for the binary. The local
cache contains only eight prediction TIFFs and only one previously validated
benchmark cube, so this controlled saturation run uses 16 concurrent instances
of `z16128_y02560_x07680.tif`. It is a 16-job concurrency measurement, not a
claim of 16 distinct source cubes.

| Metric | CPU 16-way | HIP 16-way | Result |
| --- | ---: | ---: | ---: |
| Successful jobs | 16/16 | 16/16 | PASS |
| Batch wall | 449.520 s | 77.329 s | **5.81x** |
| Throughput | 0.03559 cubes/s | 0.20691 cubes/s | **5.81x** |
| Per-job median | 435.456 s | 76.599 s | 5.69x |
| Per-job range | 422.356-449.512 s | 74.930-77.321 s | no straggler dependency |

OBSERVATION: A 20 ms `hipMemGetInfo` poll took 3,717 samples during the HIP
wave. Total visible memory was 16,974,905,344 bytes; minimum free memory was
15,090,835,456 bytes versus a 16,971,685,888-byte baseline. The measured
16-context high-water delta is 1,880,850,432 bytes (1.752 GiB).

VERDICT: The approximately 6x multi-cube target passes at **5.81x**, with all
workers successful and concurrent GPU memory well inside the 9070's capacity.

## Evidence paths

- CPU/HIP logs: `cpu.stderr.log`, `hip.stderr.log`
- Stage topology table: `topology_stages.txt`
- Full stage dumps: `cpu_dump/`, `hip_dump/`
- HIP weld diagnostics: `hip_diag_dump/`
- Controlled build: `Makefile`, `cube_mesh_cpu`, `cube_mesh_hip`
- Topology parser: `mesh_topology_audit.cpp`
- Throughput harness: `run_16way.sh`, `hip_mem_poll.cpp`
- Raw 16-way evidence: `multi16_cpu_final/`, `multi16_hip_final/`
