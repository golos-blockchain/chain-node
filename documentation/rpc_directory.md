# Optional P2P RPC directory

`rpc_directory` is an optional appbase plugin for discovering RPC endpoint announcements through the existing Golos P2P handshake. A separate client can retrieve the directory **without an initial RPC connection, a chain database or blockchain synchronization**.

This is unverified endpoint discovery, not an authenticated registry, health check or light-client proof. An announcement does not prove URL ownership, witness identity or the correctness of RPC replies. No advertised endpoint is contacted, and no application RPC selection is changed automatically.

## Build

The feature is **OFF by default**. With the repository's supported dependencies, using the newer CMake command-line interface:

```sh
cmake -S . -B build -DBUILD_RPC_DIRECTORY=ON
cmake --build build --parallel 1 --target golosd golos-rpc-directory-client
```

For the verified Bionic environment's **CMake 3.10 / Unix Makefiles**, use the equivalent legacy-compatible commands from the repository root:

```sh
mkdir -p build
cd build
cmake .. -DBUILD_RPC_DIRECTORY=ON
make -j1 golosd golos-rpc-directory-client
cd ..
```

Building the plugin does not enable it at runtime. With `BUILD_RPC_DIRECTORY=OFF`, the plugin and standalone client are not built or registered.

The complete build and encrypted-loopback test were verified using the upstream Dockerfile's `phusion/baseimage:bionic-1.0.0` environment: Ubuntu 18.04.4, GCC 7.5.0, CMake 3.10.2, Boost 1.65.1 and OpenSSL 1.1.1. A single compilation job completed under a 6 GiB memory cap; two concurrent jobs exceeded it. This legacy environment is a compatibility-test environment, not a production-security recommendation.

## Enable the plugin

Add the following to the node's existing `config.ini`, preserving any other plugins and settings:

```ini
plugin = rpc_directory

# Optional: explicitly advertise your public RPC service.
# Example URLs below are placeholders, not running RPC services.
rpc-directory-announce = https://rpc.example.org
rpc-directory-announce = wss://rpc.example.org/ws
rpc-directory-ttl = 900
```

Then restart the node using that configuration. Current appbase-based `golosd` uses **`plugin`**, not the legacy `enable-plugin` option described in older plugin documentation. The plugin automatically depends on `p2p`, which depends on `chain`; the full-node plugin does not make `golosd` lightweight.

- With no `rpc-directory-announce` entries, the enabled plugin only learns and relays.
- Configure only endpoints that you intend to publish. Addresses are never inferred from local server bindings.
- TTL defaults to 900 seconds and must be in `1..3600`; at most 32 local URLs are accepted.
- URLs must use public `https://` or `wss://` syntax. HTTP, userinfo, queries, fragments, ambiguous encodings and local/private IP literals are rejected.
- Disabling this plugin does not require replay: it creates no chain index and changes no consensus state.

No new RPC method or listener is exposed. In-process consumers can read bounded copies through `rpc_directory::plugin::get_directory()`.

## Standalone discovery client

```sh
./build/programs/rpc_directory_client/golos-rpc-directory-client \
  127.0.0.1:1776 ./rpc-discovery-state 5
```

Replace the example IP:port with an explicitly known **supporting P2P peer**, not an RPC HTTP/WebSocket address. At least one reachable peer running this feature is required; old seed nodes cannot supply a catalog they do not implement.

The duration is `1..60` seconds (default 5). The state directory contains only P2P configuration/peer bookkeeping, not blockchain storage. Each JSON result contains `url`, absolute Unix `expires`, and `trust: "UNVERIFIED_PEER_ANNOUNCEMENT"`.

Exit codes: `0` for a nonempty catalog, `3` for an empty result, `2` for invalid arguments, `130` for cancellation, and `1` for an exception. SIGINT/SIGTERM requests cleanup. The client does not contact discovered RPC URLs or follow them automatically.

Discovery mode requests no blocks or transactions, starts no chain-fetch/inventory/reconnect loops, and drops chain/application messages before application deserialization. Outgoing chain messages are filtered at queue admission and final send. Updated peers honor its discovery-only capability. Old or hostile peers may still send unsolicited bytes; this is not a claim that such traffic cannot arrive or that old peers keep the connection open indefinitely.

## Protocol and propagation limits

The directory uses the existing extensible `hello_message.user_data` field. There are no new message IDs, consensus rules or block/transaction serialization changes. Core transport hooks are necessary for the plugin and the chain-free client; this is not a plugin-only change to the source tree.

**This MVP is hello-only, not continuous gossip.** If B learns a record from A, B can relay it to C only in a later new connection's hello. Already-connected peers are not refreshed. There is no directory-driven reconnection or eventual-delivery guarantee. Stable lexicographic selection can starve records beyond the first 32.

Optional metadata is delivered once, only after existing signature, chain, duplicate, allowlist and capacity checks and bilateral connection acceptance. Rejected or wrong-chain peers cannot populate the directory. Unknown optional fields remain ignorable by old peers; old-binary interoperability has not yet been executed.

Wire key: `golos.rpc_directory.v1`. The value is a string with version line `1\n`, followed by `<absolute-expiry> <URL>\n` records. At most 32 records and 16 KiB are accepted per extension. Malformed framing, invalid URLs, numeric overflow, unknown versions or expiry more than one hour ahead reject the whole extension before mutation. Expired records are ignored. Relaying never renews expiry; only local explicit announcements can be renewed on a new hello. Correct clocks are required.

The cache is bounded to 256 distinct exact URLs, 32 per immediate peer public key and 128 KiB of logical retained-entry accounting. Full caches refuse new entries; repeating an entry cannot extend its cached expiry. The immediate peer is a quota source, not an authenticated origin. Sybil resistance and protection against bounded-cache poisoning are not provided.

URL checks are conservative ASCII syntax checks, not DNS resolution. An accepted hostname can later resolve to a private address. Any future consumer that elects to contact an endpoint must implement its own DNS/rebinding policy and independent endpoint/state validation. CID, staking, witness eligibility, endpoint health and on-chain directory indexing are outside this feature.

## Tests and verified scope

Focused tests (CMake/CTest 3.20 or newer for the CLI options below and `FindPython3` support):

```sh
cmake -S tests/rpc_directory -B build-rpc-tests
cmake --build build-rpc-tests
ctest --test-dir build-rpc-tests --no-tests=error --output-on-failure
python3 tests/rpc_directory/transport_contract_test.py
```

The focused suite passed 9/9: three linked policy/state/production-admission-predicate tests and six actual-source syntax checks. Source contracts passed 5/5. AddressSanitizer passed 3/3; combined AddressSanitizer/UndefinedBehaviorSanitizer passed 2/2 dependency-free tests. The reduced FC admission harness did not link with combined instrumentation because its minimal linkage omits FC promise RTTI; it is not counted as a combined-sanitizer pass.

Real encrypted integration with the full legacy toolchain:

```sh
mkdir -p build
cd build
cmake .. -DBUILD_RPC_DIRECTORY=ON -DBUILD_RPC_DIRECTORY_LOOPBACK_TEST=ON
make -j1 rpc_directory_loopback_test
ctest -N -R '^rpc_directory_loopback$'
# Confirm that exactly one test is listed before running it.
ctest -R '^rpc_directory_loopback$' --timeout 60 --output-on-failure
cd ..
```

On older CTest versions, separately check that exactly one test was discovered; an empty suite may otherwise exit successfully. The actual linked fixture passed 1/1: encrypted A -> B -> C relay, unchanged expiry, zero chain-delegate calls and explicit close.

The full Bionic `golosd` also passed help/version and isolated startup tests: with `plugin = rpc_directory`, the separate client received the announcement; without enabling it, the client returned `[]`. Both node processes shut down with exit 0 after SIGINT. These checks were repeated in a fresh network-disabled container. A separate `BUILD_RPC_DIRECTORY=OFF` configuration excluded feature targets; a separate OFF binary was not built.

The actual standalone client additionally passed announced/empty/unavailable-provider, usage, invalid-duration and cancellation cases. These are local Linux tests, not a mainnet deployment, Windows package test, full financial audit, old-peer compatibility result or long-running synchronization test.
