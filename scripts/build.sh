#!/usr/bin/env bash

set -e

TARGET=${1:-server}

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR"

cmake --build "$BUILD_DIR" --target "$TARGET"
