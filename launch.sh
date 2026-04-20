cmake --build build-asan -j32 && RA_TRACE_STARTUP=1 RA_TRACE_NETWORK=1 gdb -batch -ex run --args ./build-asan/redalert -gamedata "$(pwd)/GameData" -wol-server ws://127.0.0.1:8070/ws
