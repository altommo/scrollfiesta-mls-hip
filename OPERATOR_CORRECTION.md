# MLS operator correction

Date: 2026-07-22

## Corrected contract

`MLS_project_verts` performs one MLS tangent-plane projection per call. Its
`verts` array is both support and query. Production LOP iteration belongs to the
caller: each pass reads the complete previous cloud, writes a separate output
cloud, and the next pass rebuilds the spatial index from that output.

- Extract uses 5 passes.
- Resplit accepts a caller-supplied count and uses 20 in the production path.
- A fixed-support multi-update call is a different operator and is not a valid
  replacement for either workflow.

## Evidence

OBSERVATION: `~/scrollfiesta-rocm/src/common/pipeline_constants.h:73-77`
defines `MLS_PROJECT_ITERS=5` and `MLS_RESPLIT_ITERS=20`.

OBSERVATION: `~/scrollfiesta-rocm/src/extract/mesh_extract.c:1019-1028`
calls `MLS_project_verts` once per pass, swaps separate source/destination
buffers, and passes the previous result into the next call.

OBSERVATION: `~/scrollfiesta-rocm/src/extract/mesh_resplit.c:147-153` uses the
same moving-support pattern for its `lop_iters` argument.

OBSERVATION: `~/scrollfiesta-rocm/src/common/mls_project.c:204` saves the
upstream arena as scratch; `:223-229` builds cells from the current `verts`
array; `:444` restores the scratch arena before return.

OBSERVATION: The original clean kernel looped five times against one arena in
`clean-side/impl/mls_project_hip.hip:445-473` before this correction. The
original oracle accepted the same iteration count in
`clean-side/verification/src/mls_oracle.cpp:255-266`.

INFERENCE: Synthetic kernel/oracle parity passed because both independent
clean-side components faithfully implemented the same incorrect specification.
That evidence established implementation agreement, not upstream operator
agreement.

## Applied correction

1. The normative spec and semantics ledger now define one projection per call.
2. The CPU oracle has no iteration-count argument; generated fixtures record
   `iterations=1`, and all goldens were regenerated.
3. The HIP kernel takes an explicit internal iteration parameter. The required
   `MLS_project_verts` ABI fixes it to one.
4. The adapter exports `mls_scrollfiesta_lop_project(..., passes, ...)`, which
   rebuilds the clean arena from the previous output on every pass. The ordinary
   ABI wrapper remains one pass so unchanged upstream callers do not multiply
   the pass count.
5. Neighborhoods with fewer than four samples preserve the input and emit a
   zero normal. Repeated eigenvalues no longer trigger the clean-only zero-normal
   failure path.
6. The clean arena uses upstream cell/point ordering, and real-reference
   comparison uses cube-local coordinates with the original world cell origin.

## Revalidation

`./run_checks.sh` passes all host tests, regenerated one-pass goldens, interface
checks, HIP build/symbol checks, and all 10 valid synthetic HIP/oracle families
on `gfx1201`.

The real 5-pass operator passes every fixed threshold on component 2 of cube
`z16128_y02560_x07680` (1,688 points). Initial component-1 characterization
found 131 normal outliers in three ambiguity-correlated neighborhoods.
Compensated FP32 centroid accumulation removed the large pattern and reduced the
result to 2 position and 20 normal outliers while preserving weld safety. See
`../parity-real/REPORT.md`.

## Validation lesson

An independently implemented oracle cannot detect a wrong normative operator
when both implementations consume the same wrong spec. Synthetic parity must be
paired with a real upstream-reference gate that does not share the spec. Phase 3
served exactly that purpose and prevented a fixed-support operator from reaching
downstream topology testing.

A second lesson is that a strict synthetic oracle can still share numerical
weaknesses with its implementation. Real moving-support traces exposed
spatially correlated FP32 accumulation drift that small synthetic fixtures did
not. Per-pass support capture and downstream topology checks are required in
addition to final-array synthetic parity.
