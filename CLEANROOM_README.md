# ScrollFiesta MLS -> HIP clean-room kit

This kit contains a HIP implementation of one MLS midpoint projection per ABI
call, plus an independent CPU oracle, moving-support adapter, and acceptance
harness. The corrected contract is in `OPERATOR_CORRECTION.md`.

The upstream arena and coordinate contracts have been mapped from the MIT
source. `clean-side/adapter/` bridges upstream `(z,y,x)` arrays and scratch arena
semantics to the clean HIP arena and provides a caller-level 5/20-pass LOP loop.

## Contents

- `dirty-side/functional-spec.md` — normative behavioural handoff.
- `dirty-side/validation-notes.md` — ambiguity and black-box test ledger; this
  was not given to either clean-side agent.
- `clean-side/impl/` — HIP kernel, public C ABI, clean arena API, Makefile, and
  real verifier runner.
- `clean-side/verification/` — independent C++17 FP32 oracle, 15 fixture
  families, checksummed golden format, comparator, and HIP runner boundary.
- `OPERATOR_CORRECTION.md` — real-reference spec correction and validation lesson.
- `CLEAN_ROOM_MANIFEST.md` — provenance, boundary attestations, and exact evidence.
- `run_checks.sh` — CPU verification everywhere; HIP build and synthetic parity
  automatically when `hipcc` is available.

## Run

From this directory in the target WSL/ROCm environment:

```sh
./run_checks.sh
```

The HIP build uses `hipcc --offload-arch=gfx1201`. Successful synthetic tests
establish that the clean implementation matches the independent clean oracle;
they do not substitute for the upstream CPU golden, unchanged downstream
topology run, or real batch benchmark.

## Present status

- Clean numerical core: implemented.
- Exact `MLS_project_verts` C symbol in source: implemented.
- Independent CPU oracle/tests: passing in this environment.
- C header and runner strict host compilation: passing.
- HIP compilation and all valid synthetic families: passing on ROCm 7.2.4,
  `gfx1201`.
- Upstream arena/coordinate adapter: implemented and smoke-tested.
- Real component 2 strict CPU-reference parity: passing.
- Real component 1: RMS and weld safety pass; 8 max-position and 131 normal
  outliers keep strict Phase 3 open.
- Corrected 331,013-point 5-pass operator benchmark: 18.70x vs 16-thread CPU.
- Topology/end-to-end Phase 4: gated on the remaining strict Phase-3 divergence.
