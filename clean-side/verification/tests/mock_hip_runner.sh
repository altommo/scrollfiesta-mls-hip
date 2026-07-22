#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
verification_dir=$(dirname -- "$script_dir")
prefix="$MLS_POSITIONS_RAW.mock"

"$verification_dir/build/mls_verify" export-output "$MLS_FIXTURE" "$prefix"
mv "$prefix.positions.f32" "$MLS_POSITIONS_RAW"
mv "$prefix.normals.f32" "$MLS_NORMALS_RAW"
