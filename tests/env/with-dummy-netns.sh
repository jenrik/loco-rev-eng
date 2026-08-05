#!/usr/bin/env bash
# Runs its argument list inside a private user/network namespace with a
# multicast-capable dummy interface, so the mDNS/DNS-SD tests it wraps get
# sole control of UDP 5353 without touching the host's Avahi/systemd-resolved.
#
# Usage: with-dummy-netns.sh <command> [args...]
set -eu
exec unshare --user --map-root-user --net sh -c '
  set -eu
  ip link set lo up
  ip link add dummy0 type dummy
  ip link set dummy0 multicast on
  ip addr add 192.0.2.1/24 dev dummy0
  ip link set dummy0 up
  exec "$@"
' -- "$@"
