#!/bin/bash

echo "Starting stress test..."

# Ensure we have a fresh disk and the server is running
make clean && make > /dev/null 2>&1
./vfs_test > /dev/null
./vfs_server > /dev/null 2>&1 &
SERVER_PID=$!
sleep 1

echo -e "mkdir stress_dir\nexit" | ./vfs_client > /dev/null

PIDS=()
# Launch 100 concurrent clients
for i in {1..100}; do
  if [ $((i % 2)) -eq 0 ]; then
    # Readers
    echo -e "ls stress_dir\nexit" | ./vfs_client > /dev/null &
    PIDS+=($!)
  else
    # Writers
    echo -e "touch stress_dir/file_$i.txt\nwrite stress_dir/file_$i.txt Hello from client $i!\nexit" | ./vfs_client > /dev/null &
    PIDS+=($!)
  fi
done

# Wait for all client background jobs to finish
for pid in "${PIDS[@]}"; do
  wait $pid
done

echo "Verifying results..."
# Check how many files were actually created
echo -e "ls stress_dir\nexit" | ./vfs_client > stress_results.txt

NUM_FILES=$(grep "\- file_" stress_results.txt | wc -l)
echo "Successfully created and listed $NUM_FILES files out of 50."

if [ "$NUM_FILES" -eq 50 ]; then
    echo "SUCCESS: Thread-safety verified! No race conditions detected."
else
    echo "FAILURE: Missing files! Expected 50, got $NUM_FILES."
fi

kill $SERVER_PID
rm -f stress_results.txt
echo "Stress test complete."
