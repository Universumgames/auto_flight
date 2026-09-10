# Run via `cmake -P` from a custom build step (see CMakeLists.txt) so this evaluates at
# actual build time, not CMake configure time — configure only reruns when CMakeLists.txt
# changes, which would otherwise freeze BUILD_EPOCH_TIMESTAMP at whatever it was on the
# first configure, stale across later incremental (re)builds.
string(TIMESTAMP epoch "%s" UTC)
file(WRITE "${OUT}" "#pragma once\n#define BUILD_EPOCH_TIMESTAMP ${epoch}ULL\n")
