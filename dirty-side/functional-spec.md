# ScrollFiesta MLS-midpoint projection: dirty-side functional specification

**Document role:** clean-room behavioural specification (WHAT, not implementation code)  
**Scope:** one MLS projection per ABI call and the caller-level moving-support LOP loop  
**Status:** corrected after real-reference Phase 3 on 2026-07-22; this document supersedes the original fixed-support/five-internal-update contract  
**Prepared:** 2026-07-22

## 1. Clean-room boundary and normative language

This document describes externally observable behaviour and published mathematics. It does not reproduce or paraphrase either prohibited implementation file.

The dirty-side writer did **not** access, inspect, quote, summarize, hash, or otherwise read:

- `src/mls_project_cuda.cu`
- `src/common/mls_project.c`
- any implementation-equivalent source for the MLS projection operation

The clean-side implementer must not access those materials. The implementer may consume this document and independently create a HIP implementation.

In this document, **must** denotes a required compatibility property, **should** denotes an acceptance target, and **unknown** denotes behaviour that cannot safely be inferred and must be settled by black-box observation or a separately supplied public ABI description.

## 2. Operation and exclusions

One `MLS_project_verts` call takes an ordered point cloud that is both support and query, performs exactly one MLS tangent-plane projection per point, and returns one projected position and local normal per input point. The production caller performs a LOP loop by passing each call's output as the next call's support/query cloud: five passes in `mesh_extract` and up to 20 in `mesh_resplit`.

The following are outside scope:

- TIFF decoding and binary-mask extraction
- creation of the complete upstream point cloud except as needed to satisfy the arena contract
- Hoppe/global normal orientation
- ball-pivoting, sheet splitting, hole filling, QEM decimation, trimming, OBJ emission, grid welding, and unwrapping
- changes to downstream mesh topology algorithms

The projected positions are nevertheless consumed by those downstream stages, so end-to-end topology and seam-weld parity are acceptance gates for this operation.

## 3. Required external ABI

The replacement must export the following unmangled symbol with C linkage and the platform's ordinary C calling convention:

```c
extern "C"
void MLS_project_verts(void* arena,
                       const float* verts, size_t nv,
                       float radius_vox,
                       const float* cell_origin,
                       float* out_verts,
                       float* out_normals);
```

The symbol name, return type, parameter order, pointer constness, and native `size_t` width are fixed. The Linux object must link in place of `common/mls_project.o` through `src/Makefile` without caller changes. The target build is HIP for `gfx1201`.

### 3.1 Parameter-level behavioural contract

| Parameter | Required meaning | Known shape / units | Unresolved compatibility details |
|---|---|---|---|
| `arena` | Upstream scratch allocator; it does not own MLS support points | `Arena_T`, saved/restored within each CPU call | The HIP adapter may ignore it and construct clean scratch from `verts` |
| `verts` | Ordered support and query point cloud for this one call | Contiguous FP32 `(z,y,x)` triples, at least `3*nv` values; voxel-coordinate units | Inputs and outputs may alias upstream; the adapter uses private buffers |
| `nv` | Number of query vertices | Native `size_t`; one output triple per input vertex | Practical maximum and zero-length behaviour are unknown |
| `radius_vox` | Compact-support radius used by every update | FP32 scalar in voxel units; production value is 12 | Behaviour for zero, negative, non-finite, or non-12 values is unknown; the argument must not be silently ignored |
| `cell_origin` | World-aligned grid offset used only for cell lookup | `(z,y,x)` FP32 voxel coordinates | Upstream computes `floorf((coord + origin) / R)`; adapter negates/reorders it for the clean convention |
| `out_verts` | Projected positions, preserving input order | Writable contiguous FP32 `(z,y,x)` triples, at least `3*nv` values | Upstream supports aliasing; adapter uses private buffers |
| `out_normals` | Oriented local normals used by this call's projection, preserving input order | Writable contiguous FP32 `(z,y,x)` triples, at least `3*nv` values; unit length when defined | May be null upstream; production extract supplies it on every pass |

Because the function returns `void`, the compatible call has no status channel. For valid inputs, all `2*3*nv` output values must be available to the caller when the function returns. Whether the historical function's return is host-synchronous by an explicit device synchronization or by blocking copies is not relevant to the observable contract; output readiness on return is.

### 3.2 Coordinate conventions

Known behavioural facts:

- Distances, positions, support radii, positional tolerances, and weld epsilon are measured in **voxel units**, not millimetres.
- Surface samples originate at centres of nonzero voxels in a binary recto-surface mask.
- The MLS neighborhood is spherical under ordinary three-dimensional Euclidean distance.
- Output vertex `i` and output normal `i` correspond to input vertex `i`; this operation does not insert, delete, merge, sort, or deduplicate vertices.

The private ABI and extract caller use `(z,y,x)`. Extracted voxel-center
coordinates are cube-local and `cube_world_origin` aligns only spatial-hash
cells. World-shifting captured OBJ data before FP32 inference is not
numerically equivalent and must not be used for strict parity.

### 3.3 Aliasing and object lifetime

Until characterized, the conformance-test precondition is that `verts`, `cell_origin`, `out_verts`, and `out_normals` point to valid storage and that all four referenced ranges are mutually non-overlapping. The implementation must not claim in-place compatibility until `out_verts == verts` has been tested against the CPU reference. The caller retains ownership of all supplied and output buffers. The operation must not retain query/output pointers beyond return.

The upstream arena is scratch-only and is saved/restored within one CPU call.
The adapter ignores that allocator and constructs a clean arena from the current
`verts` cloud on every pass.

## 4. Mathematical behaviour for valid, non-degenerate inputs

Let the call's input cloud be \(P=\{p_j\}\), with query \(x_i=p_i\), and let \(R=\mathtt{radius\_vox}\). One ABI call performs exactly one projection. A caller-level LOP pass count controls repeated calls; support is rebuilt from the complete previous output between passes, with no convergence-based early exit in the production extract loop.

### 4.1 Neighborhood in one call

The neighborhood is re-evaluated around the current iterate:

\[
N_i^{(k)}=\{p_j\in P:\lVert p_j-x_i^{(k)}\rVert_2<R\}.
\]

Upstream uses strict `d < R`; a point at exactly `R` has zero weight.

The sample set \(P\) is fixed only for this one call. On the next LOP pass, the caller supplies the previous output as a new support/query cloud and the adapter rebuilds its spatial index.

### 4.2 Wendland C2 weights

For neighbor distance \(d_j=\lVert p_j-x_i^{(k)}\rVert_2\), define normalized radius \(q_j=d_j/R\). The published three-dimensional Wendland C2 function, normalized to one at the query point, is:

\[
w_j =
\begin{cases}
(1-q_j)^4(4q_j+1), & 0\le q_j<1,\\
0, & q_j\ge 1.
\end{cases}
\]

An overall positive constant multiplier on every weight in one neighborhood is immaterial to the centroid, covariance eigendirections, and projection; normalization \(w(0)=1\) is used here to make the expected values unambiguous. The radius scales the argument, not the returned position.

### 4.3 Two-pass centroid and covariance

First compute total weight and weighted centroid:

\[
W=\sum_j w_j,\qquad
c_i^{(k)}=\frac{1}{W}\sum_j w_jp_j.
\]

Then make a second pass over the same weighted neighborhood and compute the symmetric weighted covariance about that already-computed centroid:

\[
C_i^{(k)}=\frac{1}{W}\sum_jw_j(p_j-c_i^{(k)})(p_j-c_i^{(k)})^T.
\]

Omitting the final division by \(W\) scales all eigenvalues equally and therefore does not alter the required eigenvectors or projected result. Centering the covariance at the query rather than at the weighted centroid is **not** equivalent and does not meet this specification.

All six independent entries must describe the same real symmetric 3-by-3 matrix. The requested numerical precision for positions, weights, centroid, covariance, eigensolution, normal, and projection is FP32. Integer arithmetic used solely to locate candidate samples does not change this requirement. Fast-math transformations are acceptable only if the final parity gates remain satisfied.

### 4.4 Local plane normal

The required normal is the unit vector perpendicular to the best-fitting local tangent plane. For an ordinary covariance matrix of point offsets, this is the eigenvector associated with the **smallest eigenvalue**, because it is the direction of least local variance:

\[
C_i^{(k)}n_i^{(k)}=\lambda_{\min}n_i^{(k)},\qquad
\lVert n_i^{(k)}\rVert_2=1.
\]

The dossier's phrase “dominant eigenvector = surface normal” is inconsistent with the standard covariance definition if “dominant” means largest eigenvalue: the largest-eigenvalue vector lies along the sampled sheet rather than normal to it. The published MLS literature and real-reference source both select the smallest eigenvalue. Anisotropic synthetic and real checks confirm that direction.

The eigensolution must be that of the symmetric covariance. The benchmark records a 3-by-3 Jacobi solution upstream, but number of sweeps, rotation ordering, and stopping criteria are implementation details rather than behavioural requirements. They matter only insofar as FP32 reference parity is affected.

### 4.5 Deterministic sign convention

An eigenvector has arbitrary polarity. The normal must be made deterministic using reference direction \(u=(1,1,1)\). The natural interpretation, provisionally normative, is:

\[
n_i^{(k)}\cdot u\ge 0,
\]

flipping the eigensolver result when the dot product is below `-1e-9`. For a near-zero dot, the largest-magnitude component is made positive. This is applied on every call and to `out_normals`.

This local sign convention is applied independently on every call. It is not the later Hoppe/global orientation stage; that downstream stage is outside scope.

### 4.6 Tangent-plane projection and call result

The local plane passes through \(c_i^{(k)}\) and has unit normal \(n_i^{(k)}\). Project the current query onto that plane:

\[
x_i^{(k+1)}=x_i^{(k)}-
\left(n_i^{(k)}\cdot(x_i^{(k)}-c_i^{(k)})\right)n_i^{(k)}.
\]

Equivalently, only the normal component between the query and weighted centroid is changed; tangential coordinates are retained for that update.

After one call, `out_verts[i]` is the single tangent-plane projection of `verts[i]`, and `out_normals[i]` is the normal used for that projection. Five-pass extract output is obtained by calling this operator five times with moving support.

## 5. Valid-domain, edge-case, and failure semantics

### 5.1 Normative valid domain

The first conformance implementation may define the following as caller preconditions until black-box results expand them:

- `arena` is non-null and is a valid arena created by the existing pipeline.
- For `nv > 0`, all array pointers are non-null, suitably aligned for ordinary FP32 access, and reference the lengths stated in Section 3.
- All input vertex, origin, radius, and arena sample values are finite.
- `radius_vox` is finite and strictly positive; production acceptance uses `12.0f`.
- Every query in the conformance domain has at least four positive-weight neighbors and a finite covariance/eigendirection.
- Input/output ranges do not overlap.

No exception, signal, process termination, or out-of-bounds access is acceptable for valid-domain calls.

### 5.2 Behaviour that must be characterized, not invented

The `void` ABI supplies no clean error channel. The following cases have no established observable contract and must not be assigned fabricated behaviour in the compatibility claim:

- `nv == 0`, including which pointers may then be null
- null pointers with nonzero `nv`
- `radius_vox <= 0`, NaN, or infinity
- no positive-weight neighbors or `W == 0`
- one or two effective samples
- coincident samples
- isotropic covariance or repeated smallest eigenvalues
- a zero or non-finite eigenvector after numerical failure
- `(1,1,1)` sign dot product exactly zero
- allocation/device failure and oversized inputs

For clean-side robustness tests, these cases may be rejected before invoking the ABI or handled in a documented fail-safe manner, but such behaviour is not upstream parity until black-box observation establishes it.

## 6. Determinism, independence, and call completion

- A vertex's result depends on its own input triple, the complete input support/query cloud, `radius_vox`, `cell_origin`, and numerical semantics. Jacobi-style caller iteration reads one complete pass and writes another; it must not update support in place within a pass.
- Input order must be preserved. Permuting query vertices and undoing that permutation must reproduce the same per-vertex results within FP32 tolerance.
- Fixed inputs on the same supported GPU/software build should produce repeatable outputs. Bit-for-bit identity with the FP64 CPU reference is neither expected nor required.
- Neighbor enumeration order can perturb FP32 summation. It is not externally prescribed, but the chosen order must remain stable enough to satisfy the parity and repeatability gates.
- All output writes must be complete and visible to the host before return.
- Concurrent calls, reuse of one arena across threads, and process-level serialization are unknown. They require black-box/API characterization; multi-process throughput acceptance does not by itself prove in-process thread safety.

## 7. Numerical acceptance

Every numerical comparison must match vertices by original index, not nearest neighbor.

For each vertex, define positional error

\[
e_i=\lVert x_{HIP,i}-x_{CPU,i}\rVert_2.
\]

Report both \(\max_i e_i\) and \(\sqrt{\frac{1}{n}\sum_i e_i^2}\). For unit normals under the same deterministic polarity, define angular error as

\[
\theta_i=\arccos(\operatorname{clamp}(n_{HIP,i}\cdot n_{CPU,i},-1,1)).
\]

### 7.1 Kernel/golden target

Against CPU-double outputs for the caller-level five-pass moving-support LOP workload at `R = 12`:

- maximum positional deviation should be no greater than **1.24e-3 voxel**;
- RMS positional deviation should be no greater than **2.2e-4 voxel**;
- maximum normal angular error should be no greater than **0.006 degrees**;
- no output may be NaN or infinite.

The permitted CUDA benchmark achieved those values, so they are demonstrated FP32 feasibility targets rather than aspirational estimates.

### 7.2 Integrated real-data target

Across all components of the benchmark's post-LOP point cloud (2,009,798 points), the demonstrated comparison was maximum position delta **1.48e-3 voxel** and RMS **1.6e-4 voxel**. The HIP path should be no worse on the same capture unless an AMD-versus-NVIDIA arithmetic difference is isolated and still passes all topology gates.

### 7.3 Hard weld-safety gate

Independently of the tighter parity target, every finite positional delta must remain below the **0.25-voxel weld epsilon**. Passing only this hard gate is not enough to claim numerical parity; it establishes only that the deviation remains inside the downstream weld margin.

## 8. Downstream topology and weld acceptance

Using identical downstream CPU stages and settings for the CPU-reference positions and HIP positions:

- component count must match exactly;
- total and per-component vertex and face counts must be within **±0.01%**;
- component bounding boxes, centroids, and inter-component gap structure must give the same topology verdict;
- no new non-manifold edges, pinch vertices, or same-direction edge pairs may be introduced;
- the seam-welded vertex count for the recorded adjacent-cube halo test should differ by at most approximately **one vertex** (benchmark precedent: 521 CPU versus 522 GPU);
- triangle-quality distributions should remain materially identical;
- tiny FP32-induced BPA boundary-loop changes are tolerated only where downstream judges still classify no fillable hole and all hard topology verdicts match. The CUDA precedent was +2 boundary loops / +6 boundary edges on roughly 10.8k boundary edges.

The golden adjacent-cube test must include a surface crossing the seam, matching halo loading on both sides, and the same weld settings. The permitted benchmark used two adjacent 128-cubed cubes with `--halo 16` and the `step11_kibble` weld stage.

## 9. Performance acceptance

Performance is measured against a 16-thread OpenMP CPU build on the same host, dataset, pipeline flags, and concurrency policy as the HIP run. Report cold-start separately; acceptance uses warmed steady-state medians and must state whether index construction and host/device transfers are inside the timed MLS stage.

Targets:

- MLS operation: **at least 20x** faster than 16-thread OpenMP for the representative single-cube workload;
- single-cube end to end: approximately **1.47x or better** over the 16-thread OpenMP pipeline;
- independent multi-cube steady-state throughput: **6x or better** over the honest 16-way CPU process baseline without CPU oversubscription;
- total GPU memory at 16 concurrent workers: approximately **1.5 GB or less** for the benchmark workload.

The source CUDA benchmark was run on an RTX 5070 12 GB plus Ryzen 9800X3D (16 threads), Ubuntu 24.04, CUDA 12.9, with real Vesuvius surface data. Its demonstrated results were about 22x MLS, 1.47x single-cube end to end, 6.0–6.7x multi-cube throughput, and 1.5 GB at 16 workers. HIP/gfx1201 results must record complete target GPU, ROCm, compiler, CPU, thread, clock/power, dataset, and process-concurrency metadata so the relative claims remain auditable.

Performance failure does not relax any numerical or topology requirement.

## 10. Required golden captures and test vectors

### 10.1 Behavioural golden capture

Generate the authoritative golden by running the permitted upstream **CPU binary**, not by reading its source. A complete capture must preserve:

- exact CPU binary identity/build provenance and compiler options;
- source cube identifier and exact binary-mask bytes;
- cube/halo origin and axis-order metadata;
- the complete arena sample coordinates or a lossless reconstruction of them;
- input query vertex triples in their original order as raw FP32 bytes;
- `nv`, exact FP32 `radius_vox`, and exact `cell_origin` triple;
- output projected triples and output normal triples as raw bytes plus a portable numeric form;
- pipeline flags and stage at which inputs/outputs were captured;
- counts and hashes for capture-integrity checking (hashes apply to generated data, never to prohibited source files);
- downstream component/topology reports and performance logs.

The benchmark cube `118632705` is the primary real-data candidate: it contains about 2.01 million surface voxels and eight components in the reported no-halo/no-QEM profiling run. The repository's public sample outputs provide additional fixed mesh artifacts, but they are final/downstream outputs and are not substitutes for an ABI-level MLS input/output capture.

### 10.2 Minimum numerical test families

Each family must preserve sample coordinates, query vertices, radius, origin, CPU outputs, and HIP outputs:

1. Dense axis-aligned plane, with queries offset on both sides.
2. Dense oblique plane whose least-variance direction is unmistakable; this disambiguates smallest versus largest covariance eigenvector.
3. Gently curved sheet sampled densely enough for a unique local normal.
4. Two nearby parallel sheets, with support chosen both to isolate one sheet and to include both.
5. A moving-support fixture whose neighborhood changes between caller passes, proving that the arena is rebuilt from each previous output.
6. Translated copies paired with matching `cell_origin` changes, revealing whether coordinates are local or world-relative.
7. Axis-permuted copies, revealing triple ordering.
8. Samples just inside, exactly on, and just outside radius support.
9. Query-order permutations and duplicate query vertices, testing independence and ordering.
10. Real per-component capture, full real cube, and adjacent-cube halo/weld capture.

Degenerate tests remain in a separate characterization suite. Observed upstream
behavior is: fewer than four positive-weight neighbors preserve the point and
emit a zero normal; repeated covariance eigenvalues still emit a deterministic
unit eigendirection.

## 11. Integration ledger

Real-reference Phase 3 resolved the former release blockers: arena is scratch,
arrays and origin are `(z,y,x)`, `cell_origin` is an additive grid-index offset,
the smallest eigenvector is selected, polarity uses `(1,1,1)` with the
largest-component tie fallback, and one normal is emitted per one-pass call.
See `../clean-side/adapter/ABI_MAPPING.md` for file:line observations.

Remaining characterization items are generic invalid-input behavior, concurrent
calls, device/capacity failure reporting through the upstream void ABI, and
strict numerical behavior in repeated/near-repeated eigenspaces. These are not
operator-contract blockers, but the latter currently prevents a full Phase-3
strict-normal pass on the large real component.

## 12. Evidence log (allowed sources and observations only)

1. **User-supplied clean-room dossier**, received 2026-07-22. It supplied the original five-internal-iteration claim, which real-reference Phase 3 disproved and this revision corrects.
2. **CUDA branch public benchmark:** [pscamillo/scrollfiesta_public, `cuda-mls`, `BENCHMARKS.md`](https://github.com/pscamillo/scrollfiesta_public/blob/cuda-mls/BENCHMARKS.md), accessed 2026-07-22. Permitted behavioural source. It records the hotspot profile, five iterations at radius 12, Wendland C2 / two-pass covariance / 3-by-3 eigensolution / sign convention / tangent projection description, FP32 accuracy, topology deltas, hardware, performance, concurrency, and VRAM measurements.
3. **Upstream public README:** [Hob3rMallow/scrollfiesta_public `README.txt`](https://github.com/Hob3rMallow/scrollfiesta_public), accessed 2026-07-22. Permitted behavioural source. It records voxel-centre to MLS pipeline order, 128-cubed binary-mask input, voxel-space origins, halo requirement relative to MLS radius, CLI OBJ `z y x` convention, current public API `x y z` convention, and topology priority.
4. **Public sample-output metadata:** [Hob3rMallow/scrollfiesta_public `sample_outputs/README.md`](https://github.com/Hob3rMallow/scrollfiesta_public/tree/main/sample_outputs), accessed 2026-07-22. Permitted output metadata. It records curated final mesh identities and component/vertex/face counts. No implementation source was accessed.
5. **Published MLS survey:** Z.-Q. Cheng et al., [“A Survey of Methods for Moving Least Squares Surfaces”](https://kevinkaixu.net/papers/cheng_vgpbg08_survey.pdf), Eurographics Symposium on Volume and Point-Based Graphics, 2008. It gives the weighted-average plane, covariance normal as the smallest-eigenvalue eigenvector, iterative tangent-plane projection, and the Wendland support formula in the MLS context.
6. **Published Wendland reference notes:** G. Fasshauer, [“Compactly Supported Radial Basis Functions,” Chapter 4](https://www.math.iit.edu/~fass/603_ch4.pdf). It gives the 3D C2 Wendland form, up to an irrelevant positive constant, as `(1-r)^4_+ (4r+1)` with unit support.
7. **Local real-reference observation:** the MIT upstream source at
   `$HOME/scrollfiesta-rocm` was linked into the Phase-3 harness under a renamed
   CPU symbol and compared directly to HIP on cube-local arrays.

### Evidence discrepancy note

The dossier states that the relevant upstream had no license / all rights reserved. The current main GitHub README viewed on 2026-07-22 says the current ScrollFiesta code is MIT licensed. This may reflect a later repository state, different revision, or branch. This specification makes no legal conclusion from that discrepancy and retains the requested clean-room boundary. Exact repository revision and license status should be recorded separately by the project owner; neither changes the behavioural requirements above.

## 13. Exit criteria for the dirty-side specification

The numerical core and drop-in call contract are now specified. Final release
acceptance still requires strict real-reference parity or an explicitly revised
normal policy, followed by downstream topology, weld, and end-to-end gates on
preserved captures.
