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

The legacy command contacts one explicitly known **supporting P2P peer**, not an RPC URL. Search mode instead starts with ordinary P2P seeds, follows their standard address responses under finite budgets, and reads a catalog only when it eventually connects to a supporting peer:

```sh
./build/programs/rpc_directory_client/golos-rpc-directory-client --search ./rpc-discovery-state 10
./build/programs/rpc_directory_client/golos-rpc-directory-client --search ./rpc-discovery-state 10 --seed seed.example.org:1776 --seed 8.8.8.8:1776
# Optional newline-delimited HOST:PORT file (at most 64 seeds): --seed-file ./seeds.txt
```

Examples are syntax illustrations, not advertised running providers. Without explicit seeds, the client uses the 12 ordinary numeric bootstrap endpoints pinned from `share/golosd/seednodes`. Explicit `--seed`/`--seed-file` entries replace those defaults. Numeric explicit seeds are trusted operator input (including owned-loopback tests); DNS answers, cached endpoints and referrals must always be public numeric IPv4 endpoints. IPv6 is not supported by this legacy FC transport. No CLI switch relaxes referral policy. Loopback/private/link-local, shared-address, multicast, documentation, benchmark, reserved and zero-port referrals are rejected.

The reusable controller deduplicates endpoints across the search: total queue 64 (22 cached supporters, 21 bootstrap, 21 referrals, with admission reserved for each class), seen 128, attempts 16, address responses 16, at most 64 addresses per response, at least 250 ms between attempts. Nonempty classes take turns cache → bootstrap → referral, beginning with cache and skipping empty classes; dead cached peers and referral floods cannot exhaust another class's admission or all its attempts. The CLI permits two simultaneous owned attempts with two-second attempt deadlines and a total duration of at most 60 seconds. The discovery network hook requires same-chain bilateral admission and rejects oversized address vectors before FC vector deserialization (64 entries / 16 KiB); it never restarts normal crawling or inserts referrals into the normal peer database. A peer response snapshot is drained rather than retained without bound.

`rpc_search_supporters.v2` is an optional supporter cache: at most 64 public numeric endpoints, an 8 KiB read limit, one-hour TTL, and validation on every read. Only an actual connected peer whose catalog extension was accepted can enter it; ordinary referrals are not persisted as supporters. Cached supporters receive the first attempt; subsequent attempts rotate fairly among nonempty cache, bootstrap and referral classes. Observed protocol support does not establish RPC ownership, reachability or correctness. Future timestamps, malformed or expired caches are ignored; the legacy `rpc_search_peers.v1` file is ignored rather than migrated. Publication uses a same-directory, no-copy atomic replacement; failure preserves the old cache and removes only the owned pending file. Catalog URLs are not persisted or probed by this cache. Referral reachability is not guaranteed: ordinary peers can return transient/inbound socket addresses, and a bounded search can legitimately finish empty. An ordinary seed supplies topology, **not** a catalog; catalog propagation itself remains hello-only.

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

The focused suite registers seven runtime tests (policy, state, production admission, search, duplicate-referral, overlap and L3 policy) and six actual-source syntax checks. Search tests cover public endpoint filtering, queue/attempt/rate/response caps, cycles, cancellation and cache expiry/read validation. A fresh valid referral can promote an already queued cached supporter within its existing class without adding an attempt or changing class reservations; this prevents an overlapping cached provider from remaining behind dead entries. See [adversarial test layers](../tests/rpc_directory/adversarial/README.md) for encrypted-wire, actual-client and explicitly nonshipping loopback-overlay coverage. Policy sanitizer results are not whole-transport sanitizer coverage. Earlier pre-search sanitizer results are historical gates, not sanitizer acceptance of the new crawler.

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

On older CTest versions, separately check that exactly one test was discovered; an empty suite may otherwise exit successfully. The encrypted fixture starts an ordinary peer with its catalog hook disabled, connects it outbound to a provider, then gives the search client only the ordinary peer. The provider is learned exclusively from the standard address response. Its owned-loopback referral policy is injected into the test controller only; production continues rejecting private referrals. It asserts unchanged expiry, zero chain-delegate calls and explicit close without a chain database. The fixture's `--serve` mode supports `tests/rpc_directory/cli_search_test.py CLIENT FIXTURE` for separate-process legacy/search, strict private-referral rejection, invalid-argument and cancellation checks.

The full Bionic `golosd` also passed help/version and isolated startup tests: with `plugin = rpc_directory`, the separate client received the announcement; without enabling it, the client returned `[]`. Both node processes shut down with exit 0 after SIGINT. These checks were repeated in a fresh network-disabled container. A separate `BUILD_RPC_DIRECTORY=OFF` configuration excluded feature targets; a separate OFF binary was not built.

The actual standalone client additionally passed announced/empty/unavailable-provider, usage, invalid-duration and cancellation cases. These are local tests, not a mainnet deployment, GUI or package acceptance, CI success, full financial audit, old-peer compatibility result or long-running synchronization test.

The native Windows standalone client was exercised with owned encrypted P2P fixtures and a Unicode state-directory path. Its client-only manifest requests UTF-8 activeCodePage on Windows 10 1903+ / Windows 11. That executable used a separately prepared legacy cross-toolchain with isolated dependency compatibility/build patches that are not included here. This is not evidence that an unmodified upstream checkout builds on Windows, nor a claim that the legacy dependencies are production-security ready. No application repository, GUI harness, binary or package is part of this change.
