# Clean-side semantics ledger

This verification package has one normative input: `functional-spec.md`. The
generated synthetic goldens are outputs of this independent oracle. They are not
captures of an upstream binary.

## Clean-side test assumptions

The following choices make the synthetic tests executable. They are recorded in
every generated file's JSON metadata and must not be presented as observed
upstream behavior.

| Topic | Clean-side choice |
|---|---|
| Triple order | `(x,y,z)` |
| Sample centers | Coordinates are already explicit; no half-voxel offset is added |
| Coordinate frame | Samples and queries are in one frame |
| `cell_origin` | Preserved as metadata and not applied by the numerical oracle |
| Neighborhood boundary | `d < R`; a point at `R` has zero weight either way |
| Normal eigendirection | Smallest covariance eigenvalue |
| Normal polarity | Flip for negative dot with `(1,1,1)` |
| Zero polarity tie | Make the largest-magnitude component positive |
| Emitted normal | The normal used by the call's single tangent-plane projection |
| Iteration count | Exactly one for generated fixtures |
| Degenerate fallback | Fewer than four weighted neighbors: keep the input, emit NaN normal and a nonzero status |
| Aliasing | Inputs and outputs are treated as disjoint |

The zero-tie rule and degenerate fallback are test-harness policy only. Valid
conformance fixtures avoid them. `cell_origin` translation fixtures test the
mathematical translation equivariance of the chosen common-frame model; they do
not settle how the opaque upstream arena uses its origin.

## Observed upstream semantics

The 2026-07-22 real-reference investigation established that upstream performs
one projection per `MLS_project_verts` call. `mesh_extract` calls it five times
and `mesh_resplit` may call it 20 times, passing the previous output as the next
support/query cloud. See `../adapter/ABI_MAPPING.md` and
`../../OPERATOR_CORRECTION.md` for attributed file:line evidence.

When legally permitted black-box captures become available, store them separately
from `goldens/` and mark their provenance in metadata. A captured result should be
compared against this oracle; it must not silently rewrite the assumptions above.
