# ScrollFiesta MLS -> HIP clean-room validation notes

Status: independent dirty-side review; 2026-07-22

This document records what is known about the required behaviour, what is not
yet supported by evidence, and the black-box observations needed before a clean
implementation can honestly claim drop-in parity. It contains no implementation
source or reconstruction of upstream source.

## Executive finding

The numerical outline is sufficient to prototype a generic planar MLS projector,
but it is **not yet sufficient for a drop-in replacement of
`MLS_project_verts`**. Four P0 ambiguities can change every result or make the
replacement ABI-incompatible:

1. The signature contains no explicit surface-voxel/point-cloud argument.
   Therefore the contents and role of `arena`—or whether `verts` doubles as both
   query set and support set—must be established before the clean side can know
   what points are searched.
2. The dossier says the "dominant eigenvector" of the covariance is the surface
   normal. Standard local plane fitting uses the eigenvector of the **smallest**
   covariance eigenvalue; the larger two span the tangent plane. "Dominant" is
   therefore either nonstandard terminology or a material contradiction.
3. `cell_origin` has no defined layout, coordinate order, units, nullability, or
   effect. It may be merely an indexing origin, or it may participate in a
   local/world coordinate transformation.
4. Five iterations are specified, but the support set, update visibility, and
   timing of the returned normal are not. Several plausible interpretations
   produce different floating-point outputs and potentially different topology.

These are not implementation preferences. They are missing functional contract.
They must be resolved on the dirty side and written as observed behaviour before
the implementer receives a final spec.

## Evidence labels

- **Dossier-confirmed**: explicitly stated in the supplied clean-room dossier.
- **README-confirmed**: stated in the public top-level README, without opening
  implementation source.
- **Literature-confirmed**: a standard mathematical result, not evidence of the
  upstream program's exact choice.
- **Observed**: established by a repeatable black-box run and retained evidence.
- **Unsupported**: plausible but not established for the target ABI.

No item labelled literature-confirmed should be promoted to upstream behaviour
without a black-box observation when more than one convention is possible.

## Confirmed facts

### Functional scope and ABI

- **Dossier-confirmed:** only the MLS-midpoint projection stage is in scope, not
  normal propagation, ball pivoting, sheet split, hole filling, QEM decimation,
  trimming, or welding.
- **Dossier-confirmed:** the required exported C ABI is:

      extern "C"
      void MLS_project_verts(void* arena,
                             const float* verts, size_t nv,
                             float radius_vox,
                             const float* cell_origin,
                             float* out_verts,
                             float* out_normals);

- **Dossier-confirmed:** symbol and signature compatibility is required so the
  replacement can link where `common/mls_project.o` is normally linked.
- **Dossier-confirmed:** Linux build integration is through `src/Makefile`; the
  target HIP architecture is `gfx1201`.
- **README-confirmed:** the public pipeline begins from binary per-cube CT masks,
  converts voxel centres to a point cloud, then applies MLS smoothing before
  later orientation and meshing stages.
- **README-confirmed:** the public README describes API-boundary coordinates as
  `(x,y,z)` voxel units, while warning that some internals and CLI OBJ output use
  `(z,y,x)`. This strongly motivates a coordinate-order test, but does not by
  itself prove this internal ABI's order.

### Stated numerical behaviour

- **Dossier-confirmed:** each input vertex undergoes five projection iterations.
- **Dossier-confirmed:** neighbours are surface voxels within a radius, normally
  `R = 12` voxels.
- **Dossier-confirmed:** upstream's host-built counting-sort grid uses cell size
  `R`, but a particular spatial-index implementation is not part of the required
  clean-room design.
- **Dossier-confirmed:** neighbour weights are called Wendland C2 radial-basis
  weights.
- **Dossier-confirmed:** the calculation is described as a weighted-centroid
  pass followed by a weighted 3x3 covariance pass.
- **Dossier-confirmed:** a 3x3 symmetric Jacobi eigendecomposition is used.
- **Dossier-confirmed, but internally ambiguous:** the dossier calls the
  "dominant eigenvector" the surface normal.
- **Dossier-confirmed:** normal orientation follows a `(1,1,1)` sign convention.
- **Dossier-confirmed:** the vertex is projected onto the local tangent plane.
- **Dossier-confirmed:** the target implementation uses FP32 and maps one thread
  to each vertex.
- **Literature-confirmed:** a commonly published compactly-supported Wendland C2
  weight is
  `w(d) = (1 - d/h)^4 (4d/h + 1)` for `0 <= d <= h`, and zero outside support.
  See the [MLS survey by Cheng et al.](https://kevinkaixu.net/papers/cheng_vgpbg08_survey.pdf).
  This is a candidate interpretation, not yet observed target behaviour.
- **Literature-confirmed:** for PCA/least-squares fitting of a local plane, the
  covariance eigenvector associated with the *smallest* eigenvalue is the plane
  normal. The other two eigenvectors span the tangent plane. See
  [Hoppe et al.](https://hhoppe.com/recon.pdf) and the
  [PCL normal-estimation explanation](https://pointclouds.org/documentation/tutorials/normal_estimation.html).

### Acceptance targets supplied by the dossier

- **Dossier-confirmed target:** maximum projected-vertex deviation no greater
  than `1.24e-3` voxel and RMS deviation no greater than `2.2e-4` voxel against
  the CPU golden output, with the weld margin (`0.25` voxel) as the wider
  downstream safety boundary.
- **Dossier-confirmed target:** angular normal error no greater than `0.006°`.
- **Dossier-confirmed target:** downstream component counts agree; vertex/face
  ratios stay within `+-0.01%`; seam-weld vertex count differs by approximately
  one or less.
- **Dossier-confirmed target:** approximately `20x+` MLS-kernel speedup over the
  16-thread OpenMP reference, approximately `1.47x` single-cube end-to-end, and
  approximately `6x` multi-cube batch throughput.
- **Dossier-confirmed, ambiguous accounting:** VRAM is stated as approximately
  `1.5 GB at 16 concurrent workers`; the dossier does not explicitly say whether
  this is aggregate, incremental aggregate, peak device allocation, or a
  per-worker figure.
- **Dossier-confirmed:** the golden reference is to be behaviour captured by
  running the CPU build on a sample cube, not copied implementation material.

## Unsupported assumptions and parity risks

### 1. `arena`: P0 ABI blocker

Nothing in the allowed material defines what `arena` points to. The following
possibilities are all consistent with the signature and lead to different clean
contracts:

- an allocator/scratch arena used only for temporary storage;
- an object that owns the fixed surface-voxel point cloud;
- an object that owns a prebuilt cell grid and its point ordering;
- a compound per-cube context containing point data, bounds, and workspace;
- an optional/cache context whose null behaviour has meaning.

Because the signature has no other explicit support-point array, it is also
unsupported to assume either that:

- `verts[0..nv)` is both the set of projection queries and the fixed set of
  surface voxels; or
- `verts` is only the query set and the support points live in `arena`.

Required dirty-side facts include the arena object's public construction path,
minimum alignment, ownership, lifetime, mutability, thread-safety, whether it is
valid across repeated calls, whether a replacement may ignore it, and whether
the caller expects allocations or cache state to remain in it after return. A
drop-in test must pass the *real caller-created object*, not a clean-side object
with a newly invented layout.

### 2. Array shape, ownership, and aliasing

The natural guess is packed AoS triples (`x,y,z`) of length `3*nv` for all three
float arrays, but the signature alone does not prove it. Also unsupported:

- whether `verts` may equal `out_verts` for in-place projection;
- whether `out_normals` may overlap either vertex array;
- whether partially overlapping ranges are forbidden or handled;
- whether output buffers must be preinitialised;
- whether outputs are completely untouched on early failure (the ABI returns
  `void`, so no failure channel is specified);
- the required alignment and whether pinned/unified/device pointers are accepted;
- whether `nv == 0` permits null array pointers;
- overflow behaviour for `3*nv` and internal byte counts.

These are functional ABI concerns, not merely defensive-programming questions.

### 3. `cell_origin`: P0 coordinate blocker

The pointer's extent is not declared. It is unsupported to assume it points to
three floats or that those floats are `(x,y,z)`. Unknowns include:

- number and type of elements;
- `(x,y,z)` versus `(z,y,x)` ordering;
- voxel units versus cell indices or world coordinates;
- integer-valued versus arbitrary floating origin;
- whether it is only used to map coordinates into grid cells;
- whether it is subtracted before processing and added back to output;
- whether it offsets the support points, the query points, or both;
- whether changing it while holding `verts` fixed should change numerical output;
- nullability and default meaning;
- treatment of negative positions and floor/truncation at cell boundaries.

A translation-equivariance test alone is insufficient: two implementations can
both be translation equivariant while assigning `cell_origin` different roles.

### 4. Support-point identity and iteration semantics: P0 numerical blocker

The phrase "gather neighbouring surface voxels" does not establish:

- where those voxel centres are stored;
- whether the fixed original surface voxels are searched on every iteration;
- whether projected vertices replace the support set between global iterations;
- whether each vertex sees earlier updates in the same iteration (asynchronous)
  or all queries see the prior iteration (synchronous);
- whether neighbours are gathered about the current projected position or the
  original input position;
- whether a final sixth neighbourhood evaluation is made to calculate returned
  normals at the final position;
- whether the normal returned is from the plane used for the fifth projection
  (evaluated at the pre-fifth-update query) or a plane refit after that update;
- whether `verts` remains immutable throughout a call.

The stated one-thread-per-vertex mapping makes independent queries against a
fixed support cloud plausible, but that is an inference, not evidence.

### 5. Neighbour membership and enumeration order

Unresolved details that can affect both support and FP32 reductions:

- Euclidean-ball membership versus an axis-aligned cube or another metric;
- strict `distance < R` versus inclusive `distance <= R`;
- whether the cutoff is decided from squared distance before `sqrt`;
- whether the query point itself is included when query and support sets coincide;
- duplicate points and zero-distance samples;
- whether zero-weight points exactly on the support boundary are counted;
- cell-search extent and treatment of points exactly on cell boundaries;
- negative-coordinate cell mapping (`floor` versus truncation);
- deterministic traversal order of neighbouring cells;
- ordering within a counting-sort cell, including whether the sort is stable;
- deduplication, maximum-neighbour caps, or early truncation;
- halo clipping and behaviour where the local cube lacks support outside a seam.

The dossier permits a different spatial-index data structure, but it does not
remove the need to reproduce the same mathematical neighbour set. Furthermore,
FP32 summation order may need to be made deliberately stable to meet the tight
golden tolerances even when the set is identical.

### 6. Exact Wendland C2 convention

"Wendland C2" is not enough to identify all operational details. The standard
candidate above uses normalized distance `q = d/R`, support `q <= 1`, and
`(1-q)^4(4q+1)`. Other published communities call `h` a smoothing length while
using support `2h`, and normalization constants vary by dimension. Still unknown:

- whether `radius_vox` is the support radius or half/support scale;
- whether `q` uses `sqrt(distance_squared)/radius_vox` or another parameterisation;
- whether the compact-support clamp occurs before or after the polynomial;
- whether the radius boundary is included;
- whether a dimension-dependent multiplicative normalisation is used;
- whether weights are evaluated once about the query and reused in both passes;
- whether weights are recomputed about the weighted centroid for covariance;
- FP32 versus wider intermediate evaluation and use of fused operations.

A common constant factor mathematically cancels from a normalized centroid and
covariance eigenvectors, but it can still affect finite-precision thresholds and
degenerate/fallback branches. Support scaling does not cancel and is material.

### 7. Centroid and covariance definition

The two-pass wording supports, but does not fully define, the following choices:

- centroid denominator: sum of weights, point count, or another normalization;
- covariance centred on the weighted centroid versus the current query;
- covariance terms weighted by `w`, `w^2`, or another robust weight;
- division by sum of weights, count, or no division;
- symmetric-term accumulation order and explicit symmetrisation;
- zero/near-zero total-weight threshold;
- FP32/FP64 accumulation, FMA contraction, reassociation, and fast-math settings.

Ideal eigenvectors are invariant to a positive scalar covariance normalization,
but Jacobi stopping and FP32 rounding need not be.

### 8. Eigensolver selection, ordering, and degeneracy

The dossier's "dominant eigenvector" wording conflicts with standard plane fitting,
where the minimum-eigenvalue vector is normal. A synthetic plane is required to
resolve whether "dominant" means largest eigenvalue, the selected/primary normal,
or something other than the ordinary point covariance.

Even after selecting minimum versus maximum, Jacobi behaviour is underdefined:

- matrix storage and eigenvector row/column convention;
- fixed sweep count versus tolerance-based convergence;
- pivot order and tie selection;
- eigenvalue sort order and treatment of nearly equal eigenvalues;
- normalisation frequency and final renormalisation;
- fallback for zero covariance, one point, two points, a line, or an isotropic
  neighbourhood;
- NaN/Inf propagation;
- whether the previous-iteration normal breaks eigenvalue ties;
- reproducibility expectations across CPU and HIP transcendental/FMA behaviour.

On a perfect plane the normal eigenvalue is uniquely smallest, but papyrus edges,
single-voxel structures, and sparse halo regions can be rank-deficient. Tie rules
therefore affect real seam behaviour, not only pathological tests.

### 9. `(1,1,1)` orientation convention

The likely rule is to flip a unit normal when its dot product with `(1,1,1)` is
negative, but this has not been observed. Unknowns include:

- dot-product sign versus lexicographic component signs;
- use of normalised `(1,1,1)` (mathematically same sign, different edge rounding);
- treatment of an exact zero dot product, signed zero, or non-finite values;
- whether orientation is applied on every iteration or only to final output;
- whether orientation affects projection (it should not for an exact plane) or
  only returned normals;
- whether a later Hoppe stage expects these normals merely seeded consistently,
  rather than finally oriented.

Tie cases such as `(1,-1,0)` must be captured explicitly.

### 10. Projection equation and "midpoint" meaning

"Project onto the local tangent plane" most naturally means the orthogonal
projection of the current query onto a plane through the weighted centroid, but
the following remain unsupported:

- plane anchor: weighted centroid, support midpoint, original vertex, or another
  MLS point;
- full orthogonal step versus relaxed/half step;
- projection of current iterate versus original input each time;
- whether the selected normal is guaranteed unit length before the step;
- clamping of displacement magnitude;
- fallback when the plane fit is invalid;
- whether "MLS-midpoint" names this simple plane projection or an upstream-specific
  construction involving two sides of a binary sheet.

A curved and intentionally off-surface synthetic point cloud is needed; a perfect
plane alone cannot distinguish many of these choices.

### 11. Zero-neighbour and low-support behaviour

No behaviour is stated for zero total weight, fewer than three non-collinear
support points, or a failed eigensolve. Plausible behaviours include retaining the
current/original vertex, writing a zero/default normal, retaining a prior normal,
writing NaNs, or treating the situation as impossible under the production halo.
The ABI returns `void`, so a failure cannot be reported conventionally. The exact
vertex and normal outputs, and whether remaining iterations continue, require
black-box capture.

### 12. Parameter and side-effect contract

Unspecified cases include `radius_vox` equal to zero, negative, subnormal, NaN,
or infinity; null pointers; invalid arena state; and extremely large `nv`. These
may be outside the caller's promised preconditions. The dirty-side spec should
state whether they are forbidden inputs rather than asking the clean implementer
to invent behaviour.

Also unknown are synchronisation on return, device-context selection, HIP error
handling despite a `void` ABI, process/thread concurrency, reuse after an error,
logging, and environment-variable effects. A drop-in caller normally expects host
outputs to be ready when the function returns; this expectation must be observed.

### 13. Determinism and golden-reference provenance

The golden artefact must record:

- exact upstream commit/build identity and compiler flags;
- CPU model, thread count, operating system, and relevant math mode;
- exact input cube and its cryptographic hash;
- radius, halo, cell origin, and every caller-side option;
- raw float outputs before later geometry stages, not only rounded OBJ text;
- output hashes plus per-vertex arrays in a lossless format;
- whether repeated CPU runs are bitwise stable;
- whether point-order permutations are expected to preserve values within tolerance.

Without raw MLS-stage output, later QEM, trimming, and welding can hide or amplify
projection differences and cannot isolate this kernel's correctness.

### 14. Performance measurement contract

The numerical speed targets are not reproducible until the measured interval is
defined. In particular:

- "MLS kernel" may mean device kernel time alone or the whole ABI call including
  grid construction, allocations, host-device copies, and synchronisation;
- warmup, repetition count, statistic, cube size/density, and CPU affinity are
  unspecified;
- single-cube end-to-end must identify all included stages and I/O cache state;
- multi-cube batch must identify worker count and whether CPU and GPU work overlap;
- peak VRAM must distinguish reserved/runtime memory from live allocations and
  clarify whether `1.5 GB` is aggregate at 16 workers.

Correctness gates must be passed before speed results are accepted.

## Black-box experiments required

All experiments below are observations of documented interfaces or executable
behaviour. They must not inspect, disassemble, decompile, or transcribe forbidden
implementation source. Raw inputs, raw outputs, commands, executable hashes, and
environment metadata should be retained.

### A. Establish a callable black-box fixture (blocking)

1. Obtain a documented or author-supplied way to construct the exact object passed
   as `arena`, or a public test harness that already calls the function.
2. Establish the packed shape, coordinate order, alignment, and ownership of every
   argument.
3. Capture the real caller's preconditions for aliasing, nullability, and lifetime.
4. Run the CPU reference through that public boundary and export raw MLS-stage
   `out_verts` and `out_normals` before any later pipeline stage.

If no allowed public construction contract exists, exact object-level drop-in
replacement is blocked. End-to-end CLI output can still validate a separately
integrated replacement, but it cannot establish the opaque ABI contract by itself.

### B. Identify support data and iteration behaviour

1. Use different query and support sets if the public fixture permits it. Move one
   while holding the other fixed to establish where support points live.
2. Use a curved support patch and one displaced query for which one through five
   repeated projections are numerically distinct. Record output after separately
   configured 1, 2, 3, 4, and 5 iterations if a public option exists; otherwise
   construct cases with analytically distinguishable fixed-versus-updated support.
3. Permute `verts` while preserving geometry. After undoing the permutation,
   compare results to expose whether `verts` is the support set, whether updates
   are asynchronous, and how much accumulation order matters.
4. Compare the returned normal with a plane refit at the pre-fifth and post-fifth
   positions to establish final-normal timing.

### C. Resolve eigenvector selection and orientation

1. Axis-aligned planar grids in `xy`, `xz`, and `yz`, each with queries displaced
   along the known normal. A largest-eigenvalue choice will project along an
   in-plane direction; the smallest-eigenvalue choice will project to the plane.
2. An oblique plane whose unit normal has positive `(1,1,1)` dot product, then the
   same samples in reverse order, to establish sign stability independent of
   enumeration.
3. A plane with normal exactly or nearly orthogonal to `(1,1,1)` to capture the
   zero/tie rule.
4. Rank-deficient supports: one point, two distinct points, collinear points,
   duplicate points, and an isotropic symmetric neighbourhood. Record vertex and
   normal outputs bit-for-bit.

### D. Resolve neighbourhood and Wendland conventions

1. Place individually identifiable support points at distances `R-epsilon`, `R`,
   and `R+epsilon`, including exact FP32 predecessor/successor values, to determine
   strictness and cutoff rounding.
2. Place points inside the radius cube but outside the Euclidean sphere to identify
   the distance metric.
3. Use a minimal asymmetric weighted configuration with analytically distinct
   outcomes for support `R` versus `2R`, standard Wendland C2 versus common
   alternatives, and covariance weights evaluated about query versus centroid.
4. Repeat across positive and negative cell boundaries and at exact multiples of
   `R` to detect floor/truncation and duplicate/missed buckets.
5. Repeat with duplicate support points and with the query exactly equal to a
   support point to determine self inclusion and multiplicity.

### E. Resolve `cell_origin` and coordinate order

1. Hold all vertices fixed and vary one component of `cell_origin` at a time.
2. Translate vertices and origin together by a non-symmetric vector such as
   `(17, -31, 43)`; expected output should reveal whether origin is an indexing
   aid or a coordinate transform.
3. Translate vertices without origin, then origin without vertices.
4. Use geometry with deliberately different x/y/z extents and a non-symmetric
   oblique plane to distinguish `(x,y,z)` from `(z,y,x)`.
5. Test fractional, negative, very large, and—only if documented as valid—null
   origins.

### F. Resolve aliasing and output semantics

1. Compare disjoint buffers with exact `verts == out_verts` in-place operation.
2. If the public preconditions allow it, test `out_normals` overlap separately.
3. Surround all buffers with canaries and verify only `3*nv` floats are written.
4. Run `nv == 0` with legal pointer variants and verify side effects.
5. Pre-fill outputs with signalling patterns; use zero-neighbour and invalid-fit
   cases to distinguish untouched values from explicit zero/default writes.
6. Confirm that output host memory is complete and stable immediately on return.

### G. Golden sample and downstream topology

1. Select and hash a shipped sample cube plus its exact halo/support data.
2. Capture raw CPU MLS arrays for the production radius and five iterations.
3. Run the reference at least five times to establish its own repeatability envelope.
4. Store per-vertex correspondence, not only unordered geometry, unless the public
   contract explicitly permits reordering.
5. After numerical parity passes, run both outputs through the unchanged downstream
   pipeline and capture component count, vertices, faces, seam-weld count, and audit
   reports.
6. Include at least one seam/halo case; a central cube alone cannot validate the
   acceptance criteria that matter to welding.

### H. Performance and memory

1. Freeze CPU/GPU models, clocks/power mode, compiler flags, dataset, and worker
   count. Warm both implementations and report median plus spread over repeated runs.
2. Measure separately: spatial-index build, transfers/allocation, each projection
   iteration/device kernel, full ABI wall time, full cube, and batch wall time.
3. Synchronise explicitly at measurement boundaries.
4. Measure peak live/reserved VRAM for worker counts 1, 2, 4, 8, and 16 to determine
   scaling and interpret the `1.5 GB` target.

## Acceptance matrix

| ID | Gate | Evidence required | Pass condition | Priority |
|---|---|---|---|---|
| ABI-01 | Exported symbol | `nm`/link log from clean build and unchanged caller link | Exact unmangled `MLS_project_verts` resolves with the stated signature | P0 |
| ABI-02 | Real arena compatibility | Unchanged upstream caller creates and passes its normal arena object | Clean replacement runs without caller/source changes, corruption, or invented adapter | P0 |
| ABI-03 | Buffer contract | Canary and documented-alias tests | Exactly the documented output ranges are written; allowed aliases match CPU reference | P0 |
| ABI-04 | Return readiness | Immediate host read plus repeated-run check | Outputs are complete when the call returns; no latent write/error | P0 |
| NUM-01 | Support-set identity | Controlled query/support experiment | Clean spec names the observed support data source and clean output matches it | P0 |
| NUM-02 | Five-iteration semantics | Curved/displaced synthetic fixture | Fixed/updated support, query centre, update visibility, and final-normal timing all match observed CPU behaviour | P0 |
| NUM-03 | Eigenvector selection | Three axis planes and one oblique plane | Projection and normal match CPU; smallest/largest ambiguity is explicitly resolved | P0 |
| NUM-04 | Wendland/support convention | Minimal asymmetric and radius-boundary fixtures | CPU outputs select one exact support scale, polynomial/cutoff rule, and weighting centre | P0 |
| NUM-05 | Origin/order convention | Non-symmetric axis and translation fixtures | `cell_origin` extent/order/role and vertex coordinate order are explicitly resolved and matched | P0 |
| NUM-06 | Degenerate fallback | Empty, singleton, duplicate, line, isotropic fixtures | Vertex/normal values and continuation behaviour match CPU exactly or within a predeclared bit/ULP rule | P0 |
| NUM-07 | Golden sample vertices | Lossless raw CPU and HIP arrays with hashes | Max Euclidean vertex deviation `<= 1.24e-3` voxel and RMS `<= 2.2e-4` voxel | P0 |
| NUM-08 | Golden sample normals | Lossless paired normal arrays | Maximum angular error `<= 0.006°`; orientation rule also agrees | P0 |
| NUM-09 | Finite and bounded output | Full-array scan | No new NaN/Inf and no unexplained displacement outside the reference envelope | P0 |
| DET-01 | Repeatability | At least five identical runs | Variation stays inside the CPU reference's measured repeatability envelope | P1 |
| DET-02 | Point-order sensitivity | Multiple deterministic permutations | After inverse permutation, error stays inside declared parity tolerance; any non-invariance is documented | P1 |
| TOP-01 | Component parity | Unchanged downstream audit | Exact component counts | P0 |
| TOP-02 | Mesh-size parity | Vertex/face totals from unchanged downstream pipeline | Vertex and face ratios each within `+-0.01%` | P0 |
| TOP-03 | Seam-weld parity | At least one multi-cube seam fixture | Welded vertex count differs by no more than the agreed approximately-one-vertex allowance | P0 |
| PERF-01 | MLS acceleration | Warm, synchronised, same-data wall/device timings | `>=20x` over the frozen 16-thread OpenMP baseline for the precisely defined MLS interval | P1 |
| PERF-02 | Single-cube wall time | Repeated end-to-end run, I/O state declared | Approximately `>=1.47x` speedup using the agreed tolerance/statistic | P1 |
| PERF-03 | Batch throughput | Frozen multi-cube set and worker schedule | Approximately `>=6x` throughput using the agreed tolerance/statistic | P1 |
| MEM-01 | Sixteen-worker VRAM | Peak live and reserved allocation trace | Approximately `1.5 GB` under the clarified aggregate/per-worker definition, with no OOM | P1 |
| BUILD-01 | Target architecture | Build log and runtime device query | HIP object targets and runs on `gfx1201` | P0 |

The words "approximately" and "about" in the dossier must be converted into
explicit lower/upper bounds before PERF-02, PERF-03, TOP-03, and MEM-01 can be
used as automatic gates.

## Clean-room boundary audit

### Material used in this validation pass

- The user-supplied clean-room dossier.
- The publicly indexed top-level ScrollFiesta README, used only for high-level
  pipeline and coordinate-convention statements.
- Published/general MLS and point-cloud-normal literature, specifically the
  Cheng et al. MLS survey, Hoppe et al. surface-reconstruction paper, and PCL's
  public mathematical normal-estimation documentation.
- Filesystem discovery limited to locating allowed documents and a runnable
  black-box fixture. No local clone or fixture was present in this workspace.

### Material not accessed

- `src/mls_project_cuda.cu` was not opened, searched, displayed, copied, or
  otherwise inspected.
- `src/common/mls_project.c` was not opened, searched, displayed, copied, or
  otherwise inspected.
- No equivalent upstream MLS implementation file was opened.
- No upstream object or binary was disassembled or decompiled.
- No implementation code has been written in this document.

### Licensing discrepancy noted without changing the boundary

The dossier describes the relevant upstream reference as unlicensed/all-rights-
reserved. The currently indexed public README says the current ScrollFiesta code
is MIT and the repository listing exposes a `LICENSE` entry. This may reflect a
later commit, different branch, or changed publication state. It is a provenance
question for the project owner/legal review, not a reason for this analyst to
weaken the requested clean-room boundary. The clean-room process remains in force.

### Information permitted to cross to the clean side

Only a final functional specification containing:

- the externally required C symbol/signature and build target;
- mathematical behaviour established from literature **and confirmed where
  ambiguous by black-box observation**;
- raw numerical golden inputs/outputs and tolerances;
- documented preconditions, fallback behaviour, and performance gates;
- no upstream source excerpts, source-derived structure, naming, control flow,
  or data layout unless that layout is independently documented as part of the
  public ABI.

Unsupported alternatives in this validation note must not be converted into
clean-side requirements by choosing whichever seems most convenient. Each P0
ambiguity needs either an observed resolution or an explicit, owner-approved
change from "drop-in parity" to a newly defined integration contract.

### Transfer gate

The implementer should not be told to begin a parity implementation until the
following dirty-side facts have been resolved and recorded as observations:

1. exact `arena` role and caller-compatible construction/layout contract;
2. support-point source and five-iteration dataflow;
3. smallest/largest eigenvector selection;
4. exact Wendland support/weight convention;
5. `cell_origin` extent, order, units, and role;
6. zero/low-support fallback and final-normal timing;
7. array layout and permitted aliasing.

Without those seven facts, a clean implementation can be a reasonable MLS
projector, but its claimed object-level drop-in compatibility would be unproven.
