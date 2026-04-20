#!/bin/bash

PORT="/dev/ttyACM0"
DURATION="3s"
ERROR_MSG="mpu9250: First transaction failed. Retrying.."
MAX_ALLOWED=10
LOG_FILE="stress_test_session.txt"
ITERATION=0
RECOVERED_COUNT=0  # Counter for tests that needed a retry

trap "echo -e '\nStopped. Total: $ITERATION | Recovered: $RECOVERED_COUNT'; exit" INT

echo "Starting I2C Stress Test. Monitoring for 'stuck' events..."

while true; do
    ((ITERATION++))
    
    # 1. Run the monitor and capture output
    script -q -c "timeout --foreground $DURATION idf.py -p $PORT monitor" /dev/null | tee "$LOG_FILE"

    # 2. Check how many retries happened in this specific run
    OCCURRENCES=$(grep -o "$ERROR_MSG" "$LOG_FILE" | wc -l)
    
    # 3. If at least 1 retry happened (but below max), it's a "Recovered" event
    if [ "$OCCURRENCES" -gt 0 ] && [ "$OCCURRENCES" -le "$MAX_ALLOWED" ]; then
        ((RECOVERED_COUNT++))
    fi

    # 4. Fail if retries exceed your limit
    if [ "$OCCURRENCES" -gt "$MAX_ALLOWED" ]; then
        echo -e "\n[FATAL] Iteration $ITERATION: $OCCURRENCES retries! Exceeded limit."
        exit 1
    fi

    # 5. Display the stats in one line
    echo -e "\n>>> STATS | Total Tests: $ITERATION | Recovered (Stuck) Events: $RECOVERED_COUNT <<<"
    echo "Waiting 3 seconds..."
    sleep 1
done
