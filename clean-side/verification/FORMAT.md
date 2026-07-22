# MLSGOLD v1 binary format

All integers and IEEE-754 binary32 values are little-endian. Arrays contain
packed `(x,y,z)` float triples with no padding. The fixed header is 128 bytes,
followed by the sections below without alignment padding.

| Offset | Type | Field |
|---:|---|---|
| 0 | `char[8]` | `MLSGOLD\0` |
| 8 | `u32` | version (`1`) |
| 12 | `u32` | header bytes (`128`) |
| 16 | `u32` | endian marker (`0x01020304`) |
| 20 | `u32` | section flags (`0x1f` in v1) |
| 24 | `u64` | sample count |
| 32 | `u64` | query/output count |
| 40 | `u32` | iteration count |
| 44 | `u32` | reserved, zero |
| 48 | `f32` | radius |
| 52 | `f32[3]` | cell origin |
| 64 | `u64` | metadata byte count |
| 72 | `u64` | sample-section bytes |
| 80 | `u64` | query-section bytes |
| 88 | `u64` | projected-position-section bytes |
| 96 | `u64` | normal-section bytes |
| 104 | `u64` | status-section bytes |
| 112 | `u64` | FNV-1a-64 of the complete payload |
| 120 | `u64` | complete file byte count |

Payload section order:

1. UTF-8 JSON metadata, without a trailing NUL.
2. Sample triples (`sample_count * 12` bytes).
3. Query triples (`query_count * 12` bytes).
4. Projected-position triples (`query_count * 12` bytes).
5. Normal triples (`query_count * 12` bytes).
6. Per-query `u32` status values (`query_count * 4` bytes).

Status `0` is valid. Values `1` through `5` identify clean-side
characterization outcomes: invalid radius, empty neighborhood, non-finite input,
non-unique normal, and numerical failure. These statuses are not part of the
void upstream ABI.

FNV protects fixture integrity against accidental damage; it is not a
cryptographic provenance claim. The reader rejects inconsistent counts, section
lengths, status values, file length, magic/version, and checksum.
