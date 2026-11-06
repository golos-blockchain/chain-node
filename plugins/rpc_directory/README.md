# RPC directory plugin

Optional, unverified RPC endpoint discovery over the existing P2P hello metadata. It changes no consensus rules and exposes no new RPC listener.

Build with `-DBUILD_RPC_DIRECTORY=ON` (default: OFF), then add to the existing node `config.ini` and restart:

```ini
plugin = rpc_directory
rpc-directory-announce = https://rpc.example.org
rpc-directory-ttl = 900
```

The URL is a placeholder; use your explicitly published HTTPS/WSS RPC endpoint. Omit `rpc-directory-announce` to learn/relay only. Runtime enabling uses `plugin`, not the legacy `enable-plugin` option.

A supporting initial P2P peer is required. Propagation is hello-only on new connections, not continuous gossip. Announcements are not proof of health, server ownership or response correctness.

See [build, configuration, standalone client, protocol limits and tests](../../documentation/rpc_directory.md).
