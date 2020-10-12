#pragma once

#include <golos/protocol/types.hpp>
#include <golos/protocol/config.hpp>

namespace golos { namespace protocol {

        typedef uint128lh_t asset_symbol_type;

        struct asset {
            asset(share_type a = 0, asset_symbol_type id = STEEM_SYMBOL)
                    : amount(a), symbol(id) {
            }

            share_type amount;
            asset_symbol_type symbol;

            double to_real() const {
                return double(amount.value) / precision();
            }

            uint8_t decimals() const;

            std::string symbol_name() const;

            int64_t precision() const;

            void set_decimals(uint8_t d);

            static asset from_string(const string &from);

            string to_string() const;

            asset &operator+=(const asset &o) {
                FC_ASSERT(symbol == o.symbol);
                amount += o.amount;
                return *this;
            }

            asset &operator-=(const asset &o) {
                FC_ASSERT(symbol == o.symbol);
                amount -= o.amount;
                return *this;
            }

            asset& operator*=(int x) {
                amount *= x;
                return *this;
            }
            asset& operator/=(int x) {
                amount /= x;
                return *this;
            }
            asset operator*(int x) const {
                return asset(amount * x, symbol);
            }
            asset operator/(int x) const {
                return asset(amount / x, symbol);
            }

            asset operator-() const {
                return asset(-amount, symbol);
            }

            friend bool operator==(const asset &a, const asset &b) {
                FC_ASSERT(a.symbol == b.symbol);
                return a.amount == b.amount;
            }

            friend bool operator<(const asset &a, const asset &b) {
                FC_ASSERT(a.symbol == b.symbol);
                return a.amount < b.amount;
            }

            friend bool operator<=(const asset &a, const asset &b) {
                return (a == b) || (a < b);
            }

            friend bool operator!=(const asset &a, const asset &b) {
                return !(a == b);
            }

            friend bool operator>(const asset &a, const asset &b) {
                return !(a <= b);
            }

            friend bool operator>=(const asset &a, const asset &b) {
                return !(a < b);
            }

            friend asset operator-(const asset &a, const asset &b) {
                FC_ASSERT(a.symbol == b.symbol);
                return asset(a.amount - b.amount, a.symbol);
            }

            friend asset operator+(const asset &a, const asset &b) {
                FC_ASSERT(a.symbol == b.symbol);
                return asset(a.amount + b.amount, a.symbol);
            }

            friend asset operator*(int l, const asset& r) {
                return r * l;
            }

        };

        struct price {
            price(const asset &base = asset(), const asset quote = asset())
                    : base(base), quote(quote) {
            }

            asset base;
            asset quote;

            static price max(asset_symbol_type base, asset_symbol_type quote);

            static price min(asset_symbol_type base, asset_symbol_type quote);

            price max() const {
                return price::max(base.symbol, quote.symbol);
            }

            price min() const {
                return price::min(base.symbol, quote.symbol);
            }

            double to_real() const {
                return base.to_real() / quote.to_real();
            }

            bool is_null() const;

            void validate() const;
        };

        price operator/(const asset &base, const asset &quote);

        inline price operator~(const price &p) {
            return price{p.quote, p.base};
        }

        bool operator<(const asset &a, const asset &b);

        bool operator<=(const asset &a, const asset &b);

        bool operator<(const price &a, const price &b);

        bool operator<=(const price &a, const price &b);

        bool operator>(const price &a, const price &b);

        bool operator>=(const price &a, const price &b);

        bool operator==(const price &a, const price &b);

        bool operator!=(const price &a, const price &b);

        asset operator*(const asset &a, const price &b);
        inline asset operator*(const price& l, const asset& r) {
            return r * l;
        }

} } // golos::protocol

namespace fc {
    inline void to_variant(const golos::protocol::asset &var, fc::variant &vo) {
        vo = var.to_string();
    }

    inline void from_variant(const fc::variant &var, golos::protocol::asset &vo) {
        vo = golos::protocol::asset::from_string(var.as_string());
    }

    namespace raw {
        template<typename Stream>
        void pack(Stream& s, const golos::protocol::asset& a) {
            fc::raw::pack(s, a.amount);
            auto precision = ((char*)&a.symbol)[0];
            if (precision < 100) {
                fc::raw::pack(s, a.symbol.to_uint64());
            } else {
                fc::raw::pack(s, a.symbol);
            }
        }

        template<typename Stream>
        void unpack(Stream& s, golos::protocol::asset& a, uint32_t depth) {
            fc::raw::unpack(s, a.amount);
            uint64_t low;
            fc::raw::unpack(s, low);
            auto low_a = (char*)&low;
            auto precision = low_a[0];
            if (precision < 100) {
                a.symbol = low;
            } else {
                uint64_t high;
                fc::raw::unpack(s, high);
                a.symbol = uint128lh_t(high, low);
            }
        }
    }
}

//FC_REFLECT((golos::protocol::asset), (amount)(symbol))
FC_REFLECT((golos::protocol::price), (base)(quote))
