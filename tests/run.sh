#!/bin/sh
# Build and run the core tests (no JUCE needed).
set -e
cd "$(dirname "$0")/.."
g++ -std=c++17 -O2 -Wall -Wextra -o /tmp/purr_test tests/test_core.cpp
/tmp/purr_test
