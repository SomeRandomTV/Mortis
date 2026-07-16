#!/usr/bin/env bash

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VNODE="$PROJECT_ROOT/build/m0rtis_node/m0rtis_node"

exec "$VNODE" "$@"
