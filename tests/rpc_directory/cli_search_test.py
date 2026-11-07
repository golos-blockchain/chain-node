#!/usr/bin/env python3
"""Owned-loopback process acceptance; no public connections or chain database."""
import json
import os
import select
import signal
import subprocess
import sys
import tempfile
import time

client, fixture = sys.argv[1:3]
with tempfile.TemporaryDirectory(prefix="rpc-search-", dir=os.getcwd()) as root:
    with open(os.path.join(root, "fixture.stderr"), "w+") as errors:
        peer = subprocess.Popen([fixture, "--serve"], stdout=subprocess.PIPE, stderr=errors, universal_newlines=True)
        try:
            assert select.select([peer.stdout], [], [], 12)[0], "fixture startup timeout"
            provider, ordinary = peer.stdout.readline().strip().split()
            def run(args, code, populated=False):
                state = tempfile.mkdtemp(dir=root)
                cmd = [client] + [state if arg == "STATE" else arg for arg in args]
                result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, universal_newlines=True, timeout=10)
                assert result.returncode == code, (cmd, result.returncode, result.stderr)
                if code in (0, 3):
                    entries = json.loads(result.stdout)
                    assert isinstance(entries, list) and bool(entries) == populated
                    if populated:
                        assert entries[0]["url"] == "https://rpc.example.org"
                        assert entries[0]["trust"] == "UNVERIFIED_PEER_ANNOUNCEMENT"
                return result
            run([provider, "STATE", "3"], 0, True)
            run(["--search", "STATE", "3", "--seed", provider], 0, True)
            # Production policy MUST reject the ordinary peer's private referrals.
            run(["--search", "STATE", "3", "--seed", ordinary], 3)
            run(["--search", "STATE", "1", "--seed", "localhost:" + provider.rsplit(":", 1)[1]], 3)
            seed_file = os.path.join(root, "seeds.txt")
            with open(seed_file, "w") as seeds:
                seeds.write(provider + "\n")
            run(["--search", "STATE", "3", "--seed-file", seed_file], 0, True)
            run(["--search", "STATE", "0", "--seed", provider], 2)
            run(["--search", "STATE", "1", "--bogus", provider], 2)
            state = tempfile.mkdtemp(dir=root)
            child = subprocess.Popen([client, "--search", state, "30", "--seed", ordinary], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            time.sleep(0.3)
            child.send_signal(signal.SIGINT)
            out, err = child.communicate(timeout=6)
            assert child.returncode == 130, (child.returncode, err)
            print("CLI legacy/search catalog, private-referral rejection, invalid arguments, cancellation: PASS")
        finally:
            if peer.poll() is None:
                peer.send_signal(signal.SIGTERM)
            peer.communicate(timeout=10)
            assert peer.returncode == 0, "fixture did not close cleanly"
