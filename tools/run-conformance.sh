#!/usr/bin/env bash
# THE SPECIFICATION'S SUITE ON `i686-svr4-musl': built from the specification's
# working tree with the platform given (tools/row.sh writes one whose
# `kernel-abi' layer is this repository), and run under ibcs-us.
#
#   run-conformance.sh <openkal> <platform> <ibcs-us> [<interface>]
#
# <interface> is one of ibcs-us's (per_uw7, per_solaris, per_osr5); unset, the
# emulation's own choice for the executable. ibcs-us is started in a user
# namespace, where it has the identity it asks for.
set -euo pipefail
spec="$(cd "${1:?the specification}" && pwd)"
platform="$(cd "${2:?the platform}" && pwd)"
ibcs="$(cd "$(dirname "${3:?the ibcs-us to run under}")" && pwd)/$(basename "$3")"
interface="${4:-}"
here="$(cd "$(dirname "$0")/.." && pwd)"
unshare -Ur true 2>/dev/null || { echo "this host does not let an unprivileged user make a user namespace" >&2; exit 2; }
work="$(mktemp -d)"; trap 'rm -rf "$work"' EXIT
cp -R "$spec/tests/cts" "$work/conformance"
rm -rf "$work/conformance/target"
printf '\n[target.i686-svr4-musl]\nplatform = { path = "%s" }\n' "$platform" >> "$work/conformance/mcpp.toml"
cd "$work/conformance"
mcpp build --target i686-svr4-musl --features env,time,fs,terminal
binary="$(find "$PWD/target" -type f -name openkal-conformance | head -1)"
[ -n "$binary" ] || { echo "the suite was not produced" >&2; exit 2; }
export OPENKAL_CONFORMANCE_EMPTY=""
export OPENKAL_CONFORMANCE_PROVIDES="$(awk '/^[[:space:]]*provides-interfaces[[:space:]]*=/ {on=1} on {line=line $0; if ($0 ~ /\]/) on=0} END {gsub(/.*\[/,"",line); gsub(/\].*/,"",line); gsub(/[" \t]/,"",line); gsub(/,$/,"",line); gsub(/,/,";",line); print line}' "$here/mcpp.toml")"
mkdir run && cd run
unshare -Ur "$ibcs" ${interface:+-p "$interface"} "$binary"
