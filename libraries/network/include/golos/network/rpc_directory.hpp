#pragma once
// No chain, FC, RPC, resolver or HTTP dependency. All entries are UNVERIFIED.
#include <algorithm>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace golos { namespace network { namespace rpc_directory {
static constexpr const char* hello_key = "golos.rpc_directory.v1";
static constexpr std::size_t max_extension = 16 * 1024;
struct record {
    std::string url;
    std::uint64_t expires;
};
class catalog {
    mutable std::mutex mutex_;
    struct entry { record value; std::string source; };
    std::map<std::string, entry> records_;
    void prune(std::uint64_t now) {
        for (auto it = records_.begin(); it != records_.end();) {
            if (it->second.value.expires <= now) it = records_.erase(it); else ++it;
        }
    }
    bool insert(const record& value, const std::string& source, bool local) {
        auto existing = records_.find(value.url);
        if (existing != records_.end()) {
            // Relays cannot extend a cached announcement's absolute lifetime.
            existing->second.value.expires = local ? value.expires : std::min(existing->second.value.expires, value.expires);
            return true;
        }
        std::size_t bytes = 0, peer_count = 0;
        for (const auto& e : records_) {
            bytes += e.first.size() * 2 + e.second.source.size() + sizeof(entry);
            if (e.second.source == source) ++peer_count;
        }
        const auto cost = value.url.size() * 2 + source.size() + sizeof(entry);
        if (records_.size() >= 256 || peer_count >= 32 || bytes + cost > 128 * 1024) return false;
        records_.emplace(value.url, entry{value, source});
        return true;
    }
public:
    static bool public_url(const std::string& url) {
        if (url.empty() || url.size() > 512) return false;
        for (unsigned char ch : url)
            if (ch <= 32 || ch >= 127 || ch == '\\' || ch == '%' || ch == '@' || ch == '?' || ch == '#') return false;
        const std::size_t prefix = url.compare(0, 8, "https://") == 0 ? 8 :
                                   url.compare(0, 6, "wss://") == 0 ? 6 : 0;
        if (!prefix) return false;
        const auto path = url.find('/', prefix);
        if (path != std::string::npos) for (std::size_t i = path; i < url.size(); ++i) {
            const char ch = url[i];
            if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                  std::string("/-._~!$&'()*+,;=:").find(ch) != std::string::npos)) return false;
        }
        auto authority = url.substr(prefix, path - prefix);
        if (authority.empty()) return false;
        std::string host, port;
        if (authority[0] == '[') {
            const auto end = authority.find(']');
            if (end == std::string::npos) return false;
            host = authority.substr(1, end - 1);
            if (end + 1 < authority.size()) {
                if (authority[end + 1] != ':') return false;
                port = authority.substr(end + 2);
                if (port.empty()) return false;
            }
            // Strict hexadecimal IPv6, globally routable 2000::/3 only; no mapped IPv4/zone IDs.
            if (host.empty() || host.find('.') != std::string::npos) return false;
            if ((host.front() == ':' && host.compare(0, 2, "::")) ||
                (host.back() == ':' && (host.size() < 2 || host.compare(host.size() - 2, 2, "::")))) return false;
            const auto compression = host.find("::");
            if (compression != std::string::npos && host.find("::", compression + 2) != std::string::npos) return false;
            std::vector<unsigned> groups;
            std::size_t pos = 0, left = 0;
            while (pos < host.size()) {
                if (host[pos] == ':') { ++pos; continue; }
                const auto end_group = host.find(':', pos);
                auto token = host.substr(pos, end_group - pos);
                if (token.size() > 4) return false;
                unsigned value = 0;
                for (char ch : token) {
                    unsigned digit = ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : 16;
                    if (digit > 15) return false;
                    value = value * 16 + digit;
                }
                groups.push_back(value);
                if (compression != std::string::npos && pos < compression) ++left;
                if (end_group == std::string::npos) break;
                pos = end_group + 1;
            }
            if (compression == std::string::npos) {
                if (groups.size() != 8 || host.front() == ':' || host.back() == ':') return false;
            } else {
                if (groups.size() >= 8 || host.find(":::") != std::string::npos) return false;
                groups.insert(groups.begin() + left, 8 - groups.size(), 0);
            }
            if (groups.empty() || (groups[0] & 0xe000) != 0x2000 || groups[0] == 0x2002) return false;
            // Documentation, Teredo and protocol-assignment ranges are not RPC candidates.
            if (groups[0] == 0x2001 && (groups[1] < 0x200 || groups[1] == 0xdb8)) return false;
        } else {
            auto sep = authority.find(':');
            host = authority.substr(0, sep);
            if (sep != std::string::npos) {
                port = authority.substr(sep + 1);
                if (port.empty()) return false;
            }
            if (host.empty() || host.size() > 253) return false;
            const bool numeric = std::all_of(host.begin(), host.end(), [](char ch) { return (ch >= '0' && ch <= '9') || ch == '.'; });
            if (numeric) {
                std::vector<unsigned> octets;
                std::size_t pos = 0;
                while (pos < host.size()) {
                    const auto end = host.find('.', pos);
                    const auto token = host.substr(pos, end - pos);
                    if (token.empty() || token.size() > 3 || (token.size() > 1 && token[0] == '0')) return false;
                    unsigned value = 0;
                    for (char ch : token) value = value * 10 + ch - '0';
                    if (value > 255) return false;
                    octets.push_back(value);
                    if (end == std::string::npos) break;
                    pos = end + 1;
                }
                if (octets.size() != 4 || host.back() == '.') return false;
                const auto a = octets[0], b = octets[1];
                if (a == 0 || a == 10 || a == 127 || a >= 224 || (a == 100 && b >= 64 && b <= 127) ||
                    (a == 169 && b == 254) || (a == 172 && b >= 16 && b <= 31) ||
                    (a == 192 && (b == 168 || b == 0)) || (a == 198 && (b == 18 || b == 19 || b == 51)) ||
                    (a == 203 && b == 0 && octets[2] == 113)) return false;
            } else {
                if (host.find('.') == std::string::npos || host.back() == '.') return false;
                std::size_t pos = 0;
                while (pos < host.size()) {
                    auto end = host.find('.', pos);
                    auto label = host.substr(pos, end - pos);
                    if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-') return false;
                    for (char ch : label) if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-')) return false;
                    if (end == std::string::npos) break;
                    pos = end + 1;
                }
                auto tld = host.substr(host.rfind('.') + 1);
                if (tld == "local" || tld == "localhost" || tld == "internal" || tld == "invalid" || tld == "test" || tld == "onion") return false;
                if (!std::all_of(tld.begin(), tld.end(), [](char ch) { return ch >= 'a' && ch <= 'z'; })) return false;
            }
        }
        if (!port.empty()) {
            if (port.size() > 5 || (port.size() > 1 && port[0] == '0')) return false;
            unsigned value = 0;
            for (char ch : port) { if (ch < '0' || ch > '9') return false; value = value * 10 + ch - '0'; }
            if (!value || value > 65535) return false;
        }
        return true;
    }
    bool announce(const std::string& url, std::uint64_t expires, std::uint64_t now) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!public_url(url) || expires <= now || expires - now > 3600) return false;
        prune(now);
        return insert({url, expires}, "", true);
    }
    std::vector<record> snapshot(std::uint64_t now) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<record> out;
        for (const auto& entry : records_) if (entry.second.value.expires > now) out.push_back(entry.second.value);
        return out;
    }
    std::string hello(std::uint64_t now) const {
        std::string out = "1\n";
        unsigned count = 0;
        for (const auto& entry : snapshot(now)) {
            if (count++ == 32) break;
            const auto line = std::to_string(entry.expires) + " " + entry.url + "\n";
            if (out.size() + line.size() > max_extension) break;
            out += line;
        }
        return out;
    }
    bool receive(const std::string& source, const std::string& wire, std::uint64_t now) {
        if (source.empty() || source.size() > 128 || wire.size() > max_extension || wire.compare(0, 2, "1\n")) return false;
        // Parse the complete bounded extension before mutating the cache (atomic hard rejection).
        std::vector<record> parsed;
        std::size_t pos = 2;
        while (pos < wire.size()) {
            const auto sep = wire.find(' ', pos), end = wire.find('\n', pos);
            if (sep == std::string::npos || end == std::string::npos || sep >= end || sep == pos || sep - pos > 20 || parsed.size() >= 32) return false;
            if (sep - pos > 1 && wire[pos] == '0') return false;
            std::uint64_t expiry = 0;
            for (auto i = pos; i < sep; ++i) {
                if (wire[i] < '0' || wire[i] > '9') return false;
                const auto digit = static_cast<unsigned>(wire[i] - '0');
                if (expiry > (UINT64_MAX - digit) / 10) return false;
                expiry = expiry * 10 + digit;
            }
            const auto url = wire.substr(sep + 1, end - sep - 1);
            if (!public_url(url) || (expiry > now && expiry - now > 3600)) return false;
            parsed.push_back({url, expiry});
            pos = end + 1;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        prune(now);
        for (const auto& value : parsed) if (value.expires > now) insert(value, source, false);
        return true;
    }
};
}}}
