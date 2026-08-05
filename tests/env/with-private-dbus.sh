#!/usr/bin/env bash
# Runs its argument list against an isolated, private session dbus-daemon so
# the Avahi D-Bus discovery test never touches the host's real Avahi daemon.
#
# Usage: with-private-dbus.sh <command> [args...]
set -eu
info="$(dbus-daemon --session --fork --print-address=1 --print-pid=1)"
address="$(printf '%s\n' "$info" | sed -n '1p')"
pid="$(printf '%s\n' "$info" | sed -n '2p')"
trap 'kill "$pid" 2>/dev/null || true' EXIT
DBUS_SYSTEM_BUS_ADDRESS="$address" "$@"
