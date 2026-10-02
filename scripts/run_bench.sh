#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
set -euo pipefail

cd user
make clean && make
./kmodring_bench
cd ..

echo "Benchmarks executed successfully."
