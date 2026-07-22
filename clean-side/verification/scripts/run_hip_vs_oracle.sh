#!/bin/sh
set -eu

if [ "$#" -lt 2 ]; then
    echo "usage: $0 GOLDEN.mlsg HIP_RUNNER [RUNNER_ARGS ...]" >&2
    echo "The runner receives MLS_* environment variables and must write the requested raw outputs." >&2
    exit 40
fi

golden=$1
shift
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
verification_dir=$(dirname -- "$script_dir")
verify="$verification_dir/build/mls_verify"

if [ ! -x "$verify" ]; then
    make -C "$verification_dir" all
fi

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/mls-hip-compare.XXXXXX")
prefix="$tmp_dir/input"
positions="$tmp_dir/positions.f32"
normals="$tmp_dir/normals.f32"
actual="$tmp_dir/actual.mlsg"

cleanup() {
    rm -f "$prefix.samples.f32" "$prefix.queries.f32" "$prefix.params.txt"
    rm -f "$positions" "$normals" "$actual"
    rmdir "$tmp_dir" 2>/dev/null || true
}
trap cleanup EXIT HUP INT TERM

"$verify" export-input "$golden" "$prefix"

export MLS_FIXTURE="$golden"
export MLS_SAMPLES_RAW="$prefix.samples.f32"
export MLS_QUERIES_RAW="$prefix.queries.f32"
export MLS_PARAMS="$prefix.params.txt"
export MLS_POSITIONS_RAW="$positions"
export MLS_NORMALS_RAW="$normals"

"$@"

if [ ! -f "$positions" ] || [ ! -f "$normals" ]; then
    echo "HIP runner did not create MLS_POSITIONS_RAW and MLS_NORMALS_RAW" >&2
    exit 40
fi

"$verify" pack-results "$golden" "$positions" "$normals" "$actual"
"$verify" compare "$golden" "$actual"
