#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
set -euo pipefail

mkdir -p results

SUMMARY="results/summary.md"

{
    echo "# KmodRing Summary Report"
    echo ""
    echo "## Environment"
    if [ -f "results/env.txt" ]; then
        cat "results/env.txt"
    else
        echo "Environment info not found."
    fi
    echo ""
    echo "## Sanitizers & Verification"
    if [ -f "results/sanitizers.txt" ]; then
        cat "results/sanitizers.txt"
    else
        echo "Sanitizer report not found."
    fi
    echo ""
    echo "## Benchmark Results"
    if [ -f "results/throughput.csv" ]; then
        echo "\`\`\`csv"
        cat "results/throughput.csv"
        echo "\`\`\`"
    fi
    echo ""
    if [ -f "results/latency_summary.json" ]; then
        echo "\`\`\`json"
        cat "results/latency_summary.json"
        echo "\`\`\`"
    fi
} > "$SUMMARY"

echo "Summary report generated at $SUMMARY"
