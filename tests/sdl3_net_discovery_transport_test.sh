#!/usr/bin/env bash
set -euo pipefail
binary="${1:?usage: $0 /path/to/test}"
port=$((54000 + ($$ % 1000)))
dir=$(mktemp -d)
server=''
cleanup() {
  if [[ -n "$server" ]]; then kill "$server" 2>/dev/null || true; fi
  rm -rf "$dir"
}
trap cleanup EXIT
"$binary" --server "$port" >"$dir/server.log" 2>"$dir/server.err" &
server=$!
for _ in $(seq 1 400); do
  grep -q '^PUBLISHED ' "$dir/server.log" && break
  if ! kill -0 "$server" 2>/dev/null; then cat "$dir/server.err" >&2; exit 1; fi
  sleep 0.01
done
grep -q '^PUBLISHED ' "$dir/server.log"
"$binary" --client >"$dir/client.log" 2>"$dir/client.err"
wait "$server"
server=''
grep -q '^PASS published listener admitted discovered client$' "$dir/server.log"
grep -q '^DISCOVERED ' "$dir/client.log"
grep -q '^PASS discovered endpoint completed authoritative handshake$' "$dir/client.log"
echo 'PASS: DNS-SD advertised only a live SDL_net endpoint and discovery joined it'
