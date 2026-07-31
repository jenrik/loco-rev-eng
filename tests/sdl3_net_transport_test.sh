#!/usr/bin/env bash
set -euo pipefail
binary="${1:?usage: $0 /path/to/sdl3_net_transport_test}"
port=$((43000 + ($$ % 10000)))
dir=$(mktemp -d)
server_pid=''
cleanup() {
    if [[ -n "$server_pid" ]]; then kill "$server_pid" 2>/dev/null || true; fi
    rm -rf "$dir"
}
trap cleanup EXIT

"$binary" --server "$port" >"$dir/server.log" 2>"$dir/server.err" &
server_pid=$!
for _ in $(seq 1 200); do
    if grep -q '^READY ' "$dir/server.log"; then break; fi
    if ! kill -0 "$server_pid" 2>/dev/null; then
        cat "$dir/server.err" >&2
        exit 1
    fi
    sleep 0.01
done
grep -q '^READY ' "$dir/server.log"
"$binary" --client "$port" >"$dir/client.log" 2>"$dir/client.err"
wait "$server_pid"
server_pid=''

grep -q '^JOIN id=2$' "$dir/server.log"
grep -q '^PAYLOAD sender=2$' "$dir/server.log"
grep -q '^LEAVE id=2$' "$dir/server.log"
grep -q '^PASS server handshake, route, and leave$' "$dir/server.log"
grep -q '^CONNECTED id=2$' "$dir/client.log"
grep -q '^ECHO bytes=6$' "$dir/client.log"
grep -q '^PASS client handshake, virtual ID, and framed payload$' "$dir/client.log"
echo 'PASS: two-process SDL_net loopback handshake, virtual ID, routing, and leave'
