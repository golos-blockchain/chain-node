#include <golos/network/rpc_directory.hpp>
#include <cassert>
#include <iostream>
using namespace golos::network::rpc_directory;
int main() {
    catalog a, b, c;
    assert(a.announce("https://rpc.example.org", 1600, 1000));
    assert(b.receive("A", a.hello(1000), 1000));
    assert(c.receive("B", b.hello(1100), 1100));
    auto result = c.snapshot(1100);
    assert(result.size() == 1 && result[0].expires == 1600);
    assert(c.snapshot(1600).empty());
    std::cout << "hello-only A -> B -> C / unchanged absolute expiry: PASS\n";
    catalog urls;
    for (const auto& bad : {"http://rpc.example.org", "https://u@rpc.example.org", "https://rpc.example.org?a", "wss://rpc.example.org#x", "https://127.0.0.1", "https://10.1.1.1", "https://169.254.169.254", "https://[::1]", "https://[fc00::1]", "https://2130706433", "https://0177.0.0.1", "https://localhost", "https://rpc.local", "https://rpc.example.org/%0a", "https://rpc.example.org\\evil"})
        assert(!urls.announce(bad, 1600, 1000));
    for (const auto& bad_ipv6 : {"https://[2606:4700::1111:]", "https://[:2606:4700::1111]", "https://[2002:7f00:1::]", "https://rpc.example.org/<script>"})
        assert(!urls.announce(bad_ipv6, 1600, 1000));
    assert(urls.announce("wss://rpc.example.org:443/ws", 1600, 1000));
    assert(urls.announce("https://8.8.8.8/rpc", 1600, 1000));
    assert(urls.announce("https://[2606:4700:4700::1111]/", 1600, 1000));
    assert(!urls.announce(std::string("https://rpc.example.org/") + std::string(512, 'a'), 1600, 1000));
    std::cout << "hostile URL policy: PASS\n";
    catalog bounded;
    std::string too_many = "1\n";
    for (int i = 0; i < 33; ++i) too_many += "1600 https://rpc" + std::to_string(i) + ".example.org\n";
    assert(!bounded.receive("peer", too_many, 1000));
    assert(bounded.snapshot(1000).empty());
    assert(!bounded.receive("peer", "1\n1600 https://valid.example.org\nmalformed\n", 1000));
    assert(bounded.snapshot(1000).empty());
    assert(!bounded.receive("peer", "1\n1844674407370955161600 https://rpc.example.org\n", 1000));
    assert(!bounded.receive("peer", std::string(16385, 'a'), 1000));
    assert(!bounded.receive("peer", "2\n", 1000));
    assert(!bounded.receive("peer", "1\n4601 https://rpc.example.org\n", 1000));
    assert(bounded.receive("peer", "1\n1600 https://rpc.example.org\n", 1000));
    assert(bounded.receive("relay", "1\n1700 https://rpc.example.org\n", 1100));
    assert(bounded.snapshot(1100)[0].expires == 1600);
    for (int i = 0; i < 1000; ++i) bounded.receive("peer", "1\n1600 https://rpc" + std::to_string(i) + ".example.org\n", 1000);
    assert(bounded.snapshot(1000).size() == 32);
    for (int i = 0; i < 1000; ++i) bounded.receive("peer" + std::to_string(i), "1\n1600 https://other" + std::to_string(i) + ".example.org\n", 1000);
    assert(bounded.snapshot(1000).size() == 256);
    assert(bounded.hello(1000).size() <= 16384);
    const auto selected = bounded.hello(1000);
    assert(std::count(selected.begin(), selected.end(), '\n') <= 33);
    std::cout << "atomic wire validation / TTL / quotas / cache bounds: PASS\n";
    catalog bytes;
    for (int i = 0; i < 256; ++i) {
        const auto prefix = "https://large" + std::to_string(i) + ".example.org/";
        const auto url = prefix + std::string(512 - prefix.size(), 'a');
        assert(catalog::public_url(url));
        assert(bytes.receive("peer" + std::to_string(i), "1\n4600 " + url + "\n", 1000));
    }
    const auto retained = bytes.snapshot(1000);
    std::size_t url_bytes = 0;
    for (const auto& r : retained) url_bytes += r.url.size() * 2;
    assert(!retained.empty() && retained.size() < 256 && url_bytes <= 128 * 1024);
    assert(bytes.hello(1000).size() <= 16384);
    assert(!bytes.receive("peer", "1\n4601 https://rpc.example.org\n", 1000));
    assert(bytes.snapshot(4600).empty());
    assert(bytes.receive("peer", "1\n5200 https://rpc.example.org\n", 4600));
    assert(bytes.snapshot(4600).size() == 1);
    std::cout << "512-byte URLs / 128-KiB cache cap / exact TTL limit / expired reclamation: PASS\n";
}
