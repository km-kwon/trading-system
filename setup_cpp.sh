#!/usr/bin/env bash
set -euo pipefail

if command -v g++ >/dev/null 2>&1 && command -v cmake >/dev/null 2>&1; then
    g++ --version | head -n 1
    cmake --version | head -n 1
    echo "C++ build tools are already installed."
    exit 0
fi

if ! command -v sudo >/dev/null 2>&1; then
    echo "sudo is required to install g++ on Ubuntu/WSL." >&2
    exit 1
fi

sudo apt-get update
sudo apt-get install -y g++ cmake libgtest-dev

g++ --version | head -n 1
cmake --version | head -n 1
echo "C++ build tools installed."
