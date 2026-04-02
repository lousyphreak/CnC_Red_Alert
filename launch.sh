ls -la GameData/redalert ; cmake --build build-asan -j32 && cp build-asan/redalert GameData && RA_TRACE_STARTUP=1 gdb -batch -ex run ./GameData/redalert
