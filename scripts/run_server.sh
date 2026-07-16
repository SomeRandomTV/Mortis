#!/usr/bin/env bash

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MHUB="$PROJECT_ROOT/build/m0rtis_hub/m0rtis_hub"

exec "$MHUB" "$@"
