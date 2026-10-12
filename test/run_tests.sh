#!/bin/bash
# Host-side tests: the sketches run against a mocked Arduino API and a simulated
# DC motor + quadrature encoder (first order lag, stiction, 360 counts/rev).
# The motor model is made up, so this checks the control LOGIC, not the final gains.
set -e
cd "$(dirname "$0")"
g++ -std=c++17 -O1 -w -I. pid_step_test.cpp -o /tmp/pid_step && echo "== PID step tests" && /tmp/pid_step
g++ -std=c++17 -O1 -w -I. -DWEAK=0.85 pid_sync_test.cpp -o /tmp/pid_sync && echo "== PID sync test (motor 2 is 15% weaker)" && /tmp/pid_sync
g++ -std=c++17 -O1 -w -I. px_test.cpp -o /tmp/px_test && echo "== PX test" && /tmp/px_test
