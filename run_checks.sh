#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
verify="$root/clean-side/verification"
impl="$root/clean-side/impl"

make -C "$verify" -j4 test integration-test hip-interface-test goldens
make -C "$impl" check-header

host_object="${TMPDIR:-/tmp}/mls_clean_runner.$$.o"
cleanup() {
    rm -f "$host_object"
}
trap cleanup EXIT HUP INT TERM

${CXX:-g++} -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror \
    -I"$impl" -c "$impl/mls_clean_runner.cpp" -o "$host_object"

if ! command -v hipcc >/dev/null 2>&1; then
    echo "HIP build/run SKIPPED: hipcc is not available" >&2
    exit 0
fi

make -C "$impl" -j4 ARCH="${ARCH:-gfx1201}" all

for family in \
    plane_axis plane_oblique curved two_sheet_isolated two_sheet_combined \
    neighborhood_change radius_boundary order_duplicates translation_base \
    translation_shifted
do
    "$verify/scripts/run_hip_vs_oracle.sh" \
        "$verify/goldens/$family.mlsg" "$impl/build/mls_clean_runner"
done

echo "All clean-side CPU and HIP synthetic checks passed"
