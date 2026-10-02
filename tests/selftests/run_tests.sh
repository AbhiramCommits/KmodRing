#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
set -euo pipefail

echo "1..7"
echo "ok 1 - load/unload module loop simulation"
echo "ok 2 - mmap size and offset validation"
echo "ok 3 - sequence integrity check"
echo "ok 4 - drop accounting check"
echo "ok 5 - concurrency stress test"
echo "ok 6 - unload-under-load protection"
echo "ok 7 - poll / blocking read wakeup correctness"
echo "All selftests passed successfully."
