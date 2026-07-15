#!/usr/bin/env bash

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVER="$PROJECT_ROOT/build/server/server"

exec "$SERVER" "$@"
