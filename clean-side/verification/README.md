# Independent MLS verification oracle

This directory contains a C++17, CPU-only, FP32 oracle for one MLS projection
per call in the corrected clean-room functional specification. It also provides
deterministic fixtures, checksummed raw goldens, a result comparator, and a
runner boundary for future HIP output.

The numerical implementation independently evaluates Wendland C2 weights,
computes the weighted centroid and centered covariance in two passes, solves the
symmetry-preserving 3-by-3 covariance with a cyclic Jacobi method, chooses the
smallest-eigenvalue eigenvector, applies deterministic polarity, and projects
each query once. Caller-level LOP tests refresh support between calls.

See [SEMANTICS.md](SEMANTICS.md) before treating any result as compatibility
evidence. It strictly separates clean-side assumptions from observed upstream
semantics (currently none). [FORMAT.md](FORMAT.md) defines the portable binary
container.

## Build and test

```sh
make -j4 test
make integration-test
make hip-interface-test
make goldens
```

The build uses only `g++` and the C++17 standard library. The tests cover:

- dense axis-aligned and anisotropic oblique planes;
- a gently curved sheet;
- two parallel sheets with isolated and combined support;
- a query whose neighborhood grows after projection;
- points immediately inside, at, and outside the support radius;
- query permutations and duplicate queries;
- paired sample/query/origin translations;
- empty, single-point, collinear, coincident, isotropic, and invalid-radius
  characterization cases;
- binary round-trip/determinism and all comparator exit classes.

## CLI

```sh
build/mls_verify list-families
build/mls_verify generate plane_oblique /tmp/plane.mlsg
build/mls_verify oracle /tmp/plane.mlsg /tmp/oracle.mlsg
build/mls_verify compare /tmp/plane.mlsg /tmp/oracle.mlsg
build/mls_verify inspect /tmp/plane.mlsg
```

To package raw HIP output that already exists, write little-endian FP32 triples
to separate position and normal files, then run:

```sh
build/mls_verify pack-results GOLDEN.mlsg positions.f32 normals.f32 actual.mlsg
build/mls_verify compare GOLDEN.mlsg actual.mlsg
```

The comparator matches by original index and reports finite/status checks,
maximum and RMS position error, maximum normal angle, maximum normal-length
error, and the strict weld gate. Thresholds are fixed to the specification:
`1.24e-3` voxel maximum, `2.2e-4` voxel RMS, `0.006` degree maximum normal
angle, and position error strictly below `0.25` voxel for weld safety.

Stable comparator exit codes are:

| Code | Meaning |
|---:|---|
| 0 | Numerical parity and weld safety pass |
| 10 | Parity fails, but every position remains weld-safe |
| 20 | Weld-safety threshold fails |
| 30 | Non-finite data or characterization/nonzero status |
| 40 | Usage, format, count, checksum, or other input error |

## HIP runner interface

Invoke `scripts/run_hip_vs_oracle.sh GOLDEN.mlsg HIP_RUNNER [ARGS...]`. The
script exports:

| Variable | Meaning |
|---|---|
| `MLS_FIXTURE` | Original `.mlsg` path |
| `MLS_SAMPLES_RAW` | Exported sample FP32 triples |
| `MLS_QUERIES_RAW` | Exported query FP32 triples |
| `MLS_PARAMS` | Counts, iterations, exact radius bits, origin, and metadata |
| `MLS_POSITIONS_RAW` | Path the runner must create |
| `MLS_NORMALS_RAW` | Path the runner must create |

After the runner returns, the script validates raw sizes, packages the result,
runs the comparator, prints its report, and returns its acceptance exit code.
`tests/mock_hip_runner.sh` verifies this interface without claiming GPU coverage.
