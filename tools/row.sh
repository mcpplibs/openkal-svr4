#!/usr/bin/env bash
# THE ROW FROM WORKING TREES: writes the platform package of tests/row into
# <out>, naming this repository, openkal-musl and openkal-llvm-runtime by their
# trees and the ibcs-us to run under.
#
#   row.sh <out> <openkal-musl> <openkal-llvm-runtime> [<ibcs-us>]
#
# <ibcs-us> unset is the one on PATH.
set -euo pipefail
mkdir -p "${1:?the directory to write}"
out="$(cd "$1" && pwd)"
musl="$(cd "${2:?openkal-musl}" && pwd)"
runtime="$(cd "${3:?openkal-llvm-runtime}" && pwd)"
ibcs="${4:-ibcs-us}"
case "$ibcs" in */*) ibcs="$(cd "$(dirname "$ibcs")" && pwd)/$(basename "$ibcs")" ;; esac
here="$(cd "$(dirname "$0")/.." && pwd)"
sed -e "s|@SVR4@|$here|g" -e "s|@MUSL@|$musl|g" -e "s|@RUNTIME@|$runtime|g" -e "s|@IBCS@|$ibcs|g" \
  "$here/tests/row/mcpp.toml.in" > "$out/mcpp.toml"
cp "$here/tests/row/svr4.ld" "$out/svr4.ld"
echo "$out"
