#!/bin/bash
set -e

[[ -x test ]] ||  g++ main.cpp --std=c++11 -pthread -Ofast -march=native -o test

# Simulate threaded MIDI/MT-32 operation using the following conditions:
# - one thread synthesizes 16 milliseconds of audio per-pass
# - one thread consumes one millisecond of audio per-pass

threads=2
reads_per_write=16
iterations=10000000
./test $threads $iterations $reads_per_write


