#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0
set -euo pipefail

mkdir -p results

ENV_FILE="results/env.txt"

{
    echo "=== KmodRing Environment Probe ==="
    echo "Date: $(date)"
    echo "Kernel Release: $(uname -r)"
    echo "Machine: $(uname -m)"
    echo "OS: $(uname -s)"
    
    KERNEL_HEADERS="/lib/modules/$(uname -r)/build"
    if [ -d "$KERNEL_HEADERS" ]; then
        echo "Kernel Headers: $KERNEL_HEADERS (found)"
    else
        echo "Kernel Headers: $KERNEL_HEADERS (NOT found)"
    fi

    echo -n "GCC Version: "
    if command -v gcc &>/dev/null; then
        gcc --version | head -n1
    else
        echo "NOT found"
    fi

    echo -n "Make Version: "
    if command -v make &>/dev/null; then
        make --version | head -n1
    else
        echo "NOT found"
    fi

    echo -n "QEMU x86_64: "
    if command -v qemu-system-x86_64 &>/dev/null; then
        qemu-system-x86_64 --version | head -n1
    else
        echo "NOT found"
    fi

    echo -n "Insmod Usable: "
    if command -v insmod &>/dev/null; then
        if [ "$EUID" -eq 0 ]; then
            echo "Yes (Running as root)"
        else
            echo "Maybe (Not root, sudo required)"
        fi
    else
        echo "NOT found"
    fi

    CONFIG_FILE=""
    if [ -f "/boot/config-$(uname -r)" ]; then
        CONFIG_FILE="/boot/config-$(uname -r)"
    elif [ -f "/proc/config.gz" ]; then
        CONFIG_FILE="/proc/config.gz"
    fi

    echo "Kernel Config Source: ${CONFIG_FILE:-None}"
    if [ -n "$CONFIG_FILE" ]; then
        get_config() {
            local key="$1"
            if [[ "$CONFIG_FILE" == *.gz ]]; then
                zgrep -E "^${key}=" "$CONFIG_FILE" || echo "${key}=n/a"
            else
                grep -E "^${key}=" "$CONFIG_FILE" || echo "${key}=n/a"
            fi
        }
        get_config "CONFIG_KASAN"
        get_config "CONFIG_PROVE_LOCKING"
        get_config "CONFIG_KUNIT"
        get_config "CONFIG_DEBUG_ATOMIC_SLEEP"
    else
        echo "CONFIG_KASAN=n/a"
        echo "CONFIG_PROVE_LOCKING=n/a"
        echo "CONFIG_KUNIT=n/a"
        echo "CONFIG_DEBUG_ATOMIC_SLEEP=n/a"
    fi
} | tee "$ENV_FILE"

echo "Environment probe written to $ENV_FILE"
