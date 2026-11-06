# RPC discovery adversarial tests

These fixtures use owned peers and synthetic announcements. They do not need signing keys, a chain database or public RPC access. Never run them with a real wallet profile.

## Policy and duplicate-referral regressions

The focused CMake project in the parent directory registers `duplicate_referral_test.cpp`, `overlap_policy_test.cpp` and `l3_policy_test.cpp` alongside the existing directory tests. With a GNU-compatible compiler, the pinned FC headers and CMake/CTest 3.20 or newer:

```sh
cmake -S tests/rpc_directory -B build-rpc-tests -DRPC_DIRECTORY_SYNTAX_CHECKS=OFF
cmake --build build-rpc-tests --parallel 1
ctest --test-dir build-rpc-tests --no-tests=error --output-on-failure
```

Disabling syntax checks here isolates the runtime policy tests; it does not verify production translation units. Use the default `RPC_DIRECTORY_SYNTAX_CHECKS=ON` separately with the genuine repository dependencies available.

The overlapping-referral regression requires a queued cached provider to benefit from a fresh recommendation without receiving a second connection attempt. Other cases cover class reservations, distinct dead caches, cancellation, private-address rejection and unchanged resource limits. Public-looking addresses in policy fixtures are strings only, not connections to those services.

## Transport and CLI layers

- `wire_test.cpp` sends real encrypted control messages to owned peers, exercising admission ordering and malformed/count/flood limits.
- `cli_runtime_test.py` exercises the actual client process against owned stalling TCP listeners and checks its attempt/concurrency/cancellation limits.
- `cli_files_test.py` checks cache input rejection and sparse seed-file memory use. Its address-space/RSS assertions are Linux-specific.
- `cache_lifecycle_test.py` compiles an explicitly **NONSHIPPING** temporary loopback-policy overlay of the actual CLI translation unit. It tests persistence, restart, dead-cache recovery and referral overlap without adding a shipping private-network bypass.
- `cache_atomic_test.cpp` exercises the exact publication primitive; POSIX open-descriptor assertions must not be assumed to describe Windows open-handle behavior.

The linked harnesses currently expect the isolated legacy container layout `/source` and `/build-on`; inspect `build_wire.py` and the CLI test argument usage before running them. They require genuine built network/FC archives, not fabricated dependency stubs. Keep the container egress disabled and use only its owned loopback fixtures. Policy sanitizer results are not whole-transport sanitizer coverage.

Generated `artifacts/` and local `*_RESULTS.md`/`RESULTS.md` reports are intentionally ignored. They can contain host paths and compiler diagnostics and must not be included in a public change or application package.
