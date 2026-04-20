#!/usr/bin/env bash
# Integration test runner for the WOL WebSocket networking.
# Builds all transport-level tests, spins up the Zig server, and runs them.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "=== building zig server ==="
(cd server && zig build)

echo "=== building transport tests ==="
CXX="${CXX:-g++}"
mkdir -p build-tests
$CXX -std=c++17 -pthread -I CODE tests/ws_smoke.cpp   CODE/WSCLIENT.CPP                 CODE/SOCKETS_LINUX.CPP -o build-tests/ws_smoke
$CXX -std=c++17 -pthread -I CODE tests/wsmgr_e2e.cpp  CODE/WSMGR.CPP CODE/WSCLIENT.CPP tests/udpaddr_stub.cpp CODE/SOCKETS_LINUX.CPP -o build-tests/wsmgr_e2e
$CXX -std=c++17 -pthread -I CODE tests/wsmgr_multi.cpp CODE/WSMGR.CPP CODE/WSCLIENT.CPP tests/udpaddr_stub.cpp CODE/SOCKETS_LINUX.CPP -o build-tests/wsmgr_multi

PORT=$((19200 + RANDOM % 100))
URL="ws://127.0.0.1:${PORT}/ws"
LOG="$(mktemp /tmp/ra-wol-test-server.XXXXXX.log)"

echo "=== starting server on port ${PORT} ==="
./server/zig-out/bin/ra-wol-server --host 127.0.0.1 --port "${PORT}" --gamedata ./GameData > "${LOG}" 2>&1 &
SRV=$!
trap 'kill ${SRV} 2>/dev/null || true; rm -f "${LOG}"' EXIT

READY=0
for _ in $(seq 1 50); do
	if ./build-tests/ws_smoke "${URL}" >/dev/null 2>&1; then
		READY=1
		break
	fi
	sleep 0.1
done

if [[ ${READY} -ne 1 ]]; then
	echo "server did not become ready, log follows:"
	cat "${LOG}" || true
	exit 1
fi

RC=0
echo "--- ws_smoke ---";      ./build-tests/ws_smoke   "${URL}"      || { RC=$?; echo "FAILED ws_smoke";   }
echo "--- wsmgr_e2e ---";     ./build-tests/wsmgr_e2e  "${URL}"      || { RC=$?; echo "FAILED wsmgr_e2e";  }
echo "--- wsmgr_multi (4) ---"; ./build-tests/wsmgr_multi "${URL}" 4 || { RC=$?; echo "FAILED wsmgr_multi-4"; }
echo "--- wsmgr_multi (6) ---"; ./build-tests/wsmgr_multi "${URL}" 6 || { RC=$?; echo "FAILED wsmgr_multi-6"; }

if [[ ${RC} -eq 0 ]]; then
	echo "=== all transport tests passed ==="
else
	echo "=== FAILURES (rc=${RC}), server log: ==="
	cat "${LOG}" || true
fi
exit ${RC}
