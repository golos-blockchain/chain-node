#!/usr/bin/env python3
"""Targeted regression contracts; runtime cache/atomic tests accompany these."""
import pathlib,sys
root=pathlib.Path(__file__).resolve().parents[3]
s=(root/'programs/rpc_directory_client/main.cpp').read_text()
h=(root/'libraries/network/include/golos/network/rpc_search.hpp').read_text()
checks={
 'referrals-never-persisted':'for(const auto& ep:addresses)' not in s,
 'version-migration-failclosed':'rpc_search_supporters.v2' in s and 'supporters-v2\\n' in h,
 'observed-remote-endpoint':'set_hello_endpoint_observer' in s and 'catalog.receive(peer, wire' in s,
 'bounded-seed-read':'file.get(' in s and 'std::getline(file,line)' not in s,
 'no-fc-copy-fallback':'atomic_replace' in s and 'fc::rename' not in s,
 'cache-before-ordinary':s.find('cache_decode') < s.find('for(const auto& seed:seeds)'),
}
for k,v in checks.items(): print(k, 'PASS' if v else 'RED')
sys.exit(not all(checks.values()))
