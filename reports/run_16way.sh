#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 || ( "$1" != cpu && "$1" != hip ) ]]; then
    echo "usage: $0 {cpu|hip} RUN_DIR" >&2
    exit 2
fi

cd "$(dirname "$0")"
backend="$1"
run_dir="$2"
mkdir -p "$run_dir/logs" "$run_dir/placeholders"

input=/mnt/f/Alan/Projects/Herculaneum/cache/real-surface-mesh/scrollfiesta_grid_t050/cubes_PRED/z16128_y02560_x07680.tif
binary="./cube_mesh_${backend}"
export VESUVIUS_THREADS=1
export OMP_NUM_THREADS=1
export HSA_ENABLE_DXG_DETECTION=1
export ROCPROFILER_REGISTER_ENABLED=0

poll_pid=""
stop_file="$run_dir/stop-memory-poll"
if [[ "$backend" == hip ]]; then
    ./hip_mem_poll "$stop_file" >"$run_dir/hip_memory.txt" 2>"$run_dir/hip_memory.err" &
    poll_pid=$!
fi

batch_start=$(date +%s%N)
pids=()
for job in $(seq -w 1 16); do
    (
        start=$(date +%s%N)
        set +e
        "$binary" "$input" "$run_dir/placeholders/job_${job}.placeholder" \
            --halo 0 --no-qem --no-timeout \
            >"$run_dir/logs/job_${job}.stdout" \
            2>"$run_dir/logs/job_${job}.stderr"
        status=$?
        set -e
        end=$(date +%s%N)
        awk -v job="$job" -v status="$status" -v start="$start" -v end="$end" \
            'BEGIN { printf "%s\t%s\t%.6f\n", job, status, (end-start)/1e9 }' \
            >"$run_dir/job_${job}.tsv"
        exit "$status"
    ) &
    pids+=("$!")
done

failures=0
for pid in "${pids[@]}"; do
    if ! wait "$pid"; then
        failures=$((failures + 1))
    fi
done
batch_end=$(date +%s%N)

if [[ "$backend" == hip ]]; then
    touch "$stop_file"
    wait "$poll_pid"
fi

{
    echo -e "job\tstatus\twall_seconds"
    cat "$run_dir"/job_*.tsv
} >"$run_dir/jobs.tsv"

awk -v backend="$backend" -v failures="$failures" \
    -v start="$batch_start" -v end="$batch_end" '
    BEGIN { printf "backend=%s\njobs=16\nfailures=%d\nbatch_wall_seconds=%.6f\n", backend, failures, (end-start)/1e9 }
' >"$run_dir/summary.txt"
cat "$run_dir/summary.txt"
exit "$failures"
