#pragma once

#include <limits>
#include <cstdint>
#include <string>

#include <fc/exception/exception.hpp>
#include <fc/crypto/city.hpp>

#ifdef _MSC_VER
#pragma warning (push)
#pragma warning (disable : 4244)
#endif //// _MSC_VER

namespace fc {
    class bigint;

    /**
     *  @brief same as fc::uint128_t, but has correct order of lo and hi fields in binary serialization
     *
     */
    class uint128lh_t {


    public:
        uint128lh_t() : lo(0), hi(0) {
        }

        uint128lh_t(uint32_t l) : lo(l), hi(0) {
        }

        uint128lh_t(int32_t l) : lo(l), hi(-(l < 0)) {
        }

        uint128lh_t(int64_t l) : lo(l), hi(-(l < 0)) {
        }

        uint128lh_t(uint64_t l) : lo(l), hi(0) {
        }

        uint128lh_t(const std::string &s);

        uint128lh_t(uint64_t _h, uint64_t _l) : lo(_l), hi(_h) {
        }

        uint128lh_t(const fc::bigint &bi);

        operator std::string() const;

        operator fc::bigint() const;

        bool operator==(const uint128lh_t& o) const {
            return hi == o.hi && lo == o.lo;
        }

        bool operator!=(const uint128lh_t& o) const {
            return hi != o.hi || lo != o.lo;
        }

        bool operator<(const uint128lh_t& o) const {
            return (hi == o.hi) ? lo < o.lo : hi < o.hi;
        }

        bool operator<(const int64_t& o) const {
            return *this < uint128lh_t(o);
        }

        bool operator!() const {
            return !(hi != 0 || lo != 0);
        }

        uint128lh_t operator-() const {
            return ++uint128lh_t(~hi, ~lo);
        }

        uint128lh_t operator~() const {
            return uint128lh_t(~hi, ~lo);
        }

        uint128lh_t &operator++() {
            hi += (++lo == 0);
            return *this;
        }

        uint128lh_t &operator--() {
            hi -= (lo-- == 0);
            return *this;
        }

        uint128lh_t operator++(int) {
            auto tmp = *this;
            ++(*this);
            return tmp;
        }

        uint128lh_t operator--(int) {
            auto tmp = *this;
            --(*this);
            return tmp;
        }

        uint128lh_t& operator|=(const uint128lh_t& u) {
            hi |= u.hi;
            lo |= u.lo;
            return *this;
        }

        uint128lh_t& operator&=(const uint128lh_t& u) {
            hi &= u.hi;
            lo &= u.lo;
            return *this;
        }

        uint128lh_t& operator^=(const uint128lh_t& u) {
            hi ^= u.hi;
            lo ^= u.lo;
            return *this;
        }

        uint128lh_t& operator<<=(const uint128lh_t& u);

        uint128lh_t& operator>>=(const uint128lh_t& u);

        uint128lh_t& operator+=(const uint128lh_t& u) {
            const uint64_t old = lo;
            lo += u.lo;
            hi += u.hi + (lo < old);
            return *this;
        }

        uint128lh_t& operator-=(const uint128lh_t& u) {
            return *this += -u;
        }

        uint128lh_t& operator*=(const uint128lh_t& u);

        uint128lh_t& operator/=(const uint128lh_t& u);

        uint128lh_t& operator%=(const uint128lh_t& u);


        friend uint128lh_t operator+(const uint128lh_t& l, const uint128lh_t& r) {
            return uint128lh_t(l) += r;
        }

        friend uint128lh_t operator-(const uint128lh_t& l, const uint128lh_t& r) {
            return uint128lh_t(l) -= r;
        }

        friend uint128lh_t operator*(const uint128lh_t& l, const uint128lh_t& r) {
            return uint128lh_t(l) *= r;
        }

        friend uint128lh_t operator/(const uint128lh_t& l, const uint128lh_t& r) {
            return uint128lh_t(l) /= r;
        }

        friend uint128lh_t operator%(const uint128lh_t& l, const uint128lh_t& r) {
            return uint128lh_t(l) %= r;
        }

        friend uint128lh_t operator|(const uint128lh_t& l, const uint128lh_t& r) {
            return uint128lh_t(l) = (r);
        }

        friend uint128lh_t operator&(const uint128lh_t& l, const uint128lh_t& r) {
            return uint128lh_t(l) &= r;
        }

        friend uint128lh_t operator^(const uint128lh_t& l, const uint128lh_t& r) {
            return uint128lh_t(l) ^= r;
        }

        friend uint128lh_t operator<<(const uint128lh_t& l, const uint128lh_t& r) {
            return uint128lh_t(l) <<= r;
        }

        friend uint128lh_t operator>>(const uint128lh_t& l, const uint128lh_t& r) {
            return uint128lh_t(l) >>= r;
        }

        friend bool operator>(const uint128lh_t& l, const uint128lh_t& r) {
            return r < l;
        }

        friend bool operator>(const uint128lh_t& l, const int64_t &r) {
            return uint128lh_t(r) < l;
        }

        friend bool operator>(const int64_t &l, const uint128lh_t& r) {
            return r < uint128lh_t(l);
        }

        friend bool operator>=(const uint128lh_t& l, const uint128lh_t& r) {
            return l == r || l > r;
        }

        friend bool operator>=(const uint128lh_t& l, const int64_t &r) {
            return l >= uint128lh_t(r);
        }

        friend bool operator>=(const int64_t &l, const uint128lh_t& r) {
            return uint128lh_t(l) >= r;
        }

        friend bool operator<=(const uint128lh_t& l, const uint128lh_t& r) {
            return l == r || l < r;
        }

        friend bool operator<=(const uint128lh_t& l, const int64_t &r) {
            return l <= uint128lh_t(r);
        }

        friend bool operator<=(const int64_t &l, const uint128lh_t& r) {
            return uint128lh_t(l) <= r;
        }

        friend std::size_t hash_value(const uint128lh_t& v) {
            return city_hash_size_t((const char *) &v, sizeof(v));
        }

        uint32_t to_integer() const {
            FC_ASSERT(hi == 0);
            uint32_t lo32 = (uint32_t) lo;
            FC_ASSERT(lo == lo32);
            return lo32;
        }

        uint64_t to_uint64() const {
            FC_ASSERT(hi == 0);
            return lo;
        }

        uint32_t low_32_bits() const {
            return (uint32_t) lo;
        }

        uint64_t low_bits() const {
            return lo;
        }

        uint64_t high_bits() const {
            return hi;
        }

        static uint128lh_t max_value() {
            const uint64_t max64 = std::numeric_limits<uint64_t>::max();
            return uint128lh_t(max64, max64);
        }

        static void full_product(const uint128lh_t& a, const uint128lh_t& b, uint128lh_t& result_hi, uint128lh_t& result_lo);

        uint8_t popcount() const;

        // fields must be public for serialization
        uint64_t lo;
        uint64_t hi;
    };

    static_assert(sizeof(uint128lh_t) == 2 * sizeof(uint64_t), "validate packing assumptions");

    class variant;

    void to_variant(const uint128lh_t& var, variant &vo);

    void from_variant(const variant &var, uint128lh_t& vo);

    namespace raw {
        template<typename Stream>
        inline void pack(Stream &s, const uint128lh_t& u) {
            s.write((char *) &u, sizeof(u));
        }

        template<typename Stream>
        inline void unpack(Stream &s, uint128lh_t& u, uint32_t) {
            s.read((char *) &u, sizeof(u));
        }
    }

    size_t city_hash_size_t(const char *buf, size_t len);
} // namespace fc

namespace std {
    template<>
    struct hash<fc::uint128lh_t> {
        size_t operator()(const fc::uint128lh_t& s) const {
            return fc::city_hash_size_t((char *) &s, sizeof(s));
        }
    };
}

FC_REFLECT((fc::uint128lh_t), (lo)(hi))

#ifdef _MSC_VER
#pragma warning (pop)
#endif ///_MSC_VER
