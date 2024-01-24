#pragma once

#include <golos/chain/steem_object_types.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <golos/protocol/nft_operations.hpp>

namespace golos { namespace chain {

    using namespace golos::protocol;

    class nft_collection_object : public object<nft_collection_object_type, nft_collection_object> {
    public:
        template <typename Constructor, typename Allocator>
        nft_collection_object(Constructor&& c, allocator <Allocator> a) : json_metadata(a) {
            c(*this);
        };

        id_type id;

        account_name_type creator;
        asset_symbol_type name;

        shared_string json_metadata;
        time_point_sec created;
        uint32_t token_count = 0;
        uint32_t max_token_count = 0;
        uint32_t last_token_id = 0;

        asset last_buy_price{0, STEEM_SYMBOL};
        uint32_t buy_order_count = 0;
        uint32_t sell_order_count = 0;
        double market_depth = 0;
        double market_asks = 0;
        double market_volume = 0;

        double price_real() const {
            return last_buy_price.to_real();
        }

        std::string name_str() const {
            return nft_name_to_string(name);
        }
    };

    class nft_object : public object<nft_object_type, nft_object> {
    public:
        template <typename Constructor, typename Allocator>
        nft_object(Constructor&& c, allocator <Allocator> a) : title(a), image(a), json_metadata(a) {
            c(*this);
        };

        id_type id;

        account_name_type creator;
        asset_symbol_type name;

        account_name_type owner;
        uint32_t token_id = 0;
        bool burnt = false;

        shared_string title;
        shared_string image;

        asset issue_cost;
        asset last_buy_price{0, STEEM_SYMBOL};
        shared_string json_metadata;
        time_point_sec issued;
        time_point_sec last_update;
        bool selling = false;

        asset auction_min_price{0, STEEM_SYMBOL};
        time_point_sec auction_expiration;

        double price_real() const {
            return last_buy_price.to_real();
        }

        std::string name_str() const {
            return nft_name_to_string(name);
        }
    };

    class nft_order_object : public object<nft_order_object_type, nft_order_object> {
    public:
        nft_order_object() {
        }

        template <typename Constructor, typename Allocator>
        nft_order_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        };

        id_type id;

        account_name_type creator;
        asset_symbol_type name;
        uint32_t token_id = 0;

        account_name_type owner;
        uint32_t order_id = 0;
        asset price;
        bool selling = false;
        bool holds = false;

        time_point_sec created;

        double price_real() const {
            return price.to_real();
        }
    };

    class nft_bet_object : public object<nft_bet_object_type, nft_bet_object> {
    public:
        nft_bet_object() {
        }

        template <typename Constructor, typename Allocator>
        nft_bet_object(Constructor&& c, allocator <Allocator> a) {
            c(*this);
        };

        id_type id;

        account_name_type creator;
        asset_symbol_type name;
        uint32_t token_id = 0;

        account_name_type owner;
        asset price;

        double price_real() const {
            return price.to_real();
        }

        time_point_sec created;
    };

    struct by_name;
    struct by_name_str;
    struct by_creator_name;
    struct by_created;
    struct by_last_price;
    struct by_token_count;
    struct by_market_depth;
    struct by_market_asks;
    struct by_market_volume;

    using nft_collection_index = multi_index_container<
        nft_collection_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<nft_collection_object, nft_collection_object_id_type, &nft_collection_object::id>
            >,
            ordered_unique<
                tag<by_name>,
                member<nft_collection_object, asset_symbol_type, &nft_collection_object::name>
            >,
            ordered_non_unique<
                tag<by_name_str>,
                const_mem_fun<nft_collection_object, std::string, &nft_collection_object::name_str>
            >,
            ordered_unique<
                tag<by_creator_name>,
                composite_key<
                    nft_collection_object,
                    member<nft_collection_object, account_name_type, &nft_collection_object::creator>,
                    member<nft_collection_object, asset_symbol_type, &nft_collection_object::name>
                >
            >,
            ordered_non_unique<
                tag<by_created>,
                member<nft_collection_object, time_point_sec, &nft_collection_object::created>,
                std::greater<time_point_sec>
            >,
            ordered_non_unique<
                tag<by_last_price>,
                const_mem_fun<nft_collection_object, double, &nft_collection_object::price_real>,
                std::greater<double>
            >,
            ordered_non_unique<
                tag<by_token_count>,
                member<nft_collection_object, uint32_t, &nft_collection_object::token_count>,
                std::greater<uint32_t>
            >,
            ordered_non_unique<
                tag<by_market_depth>,
                member<nft_collection_object, double, &nft_collection_object::market_depth>,
                std::greater<double>
            >,
            ordered_non_unique<
                tag<by_market_asks>,
                member<nft_collection_object, double, &nft_collection_object::market_asks>,
                std::greater<double>
            >,
            ordered_non_unique<
                tag<by_market_volume>,
                member<nft_collection_object, double, &nft_collection_object::market_volume>,
                std::greater<double>
            >
        >,
        allocator<nft_collection_object>
    >;

    struct by_token_id;
    struct by_asset_owner;
    struct by_asset_str_owner;
    struct by_owner;
    struct by_issued;
    struct by_last_update;
    struct by_auction_expiration;

    using nft_index = multi_index_container<
        nft_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<nft_object, nft_object_id_type, &nft_object::id>
            >,
            ordered_unique<
                tag<by_token_id>,
                member<nft_object, uint32_t, &nft_object::token_id>,
                std::greater<uint32_t>
            >,
            ordered_non_unique<
                tag<by_asset_owner>,
                composite_key<
                    nft_object,
                    member<nft_object, asset_symbol_type, &nft_object::name>,
                    member<nft_object, account_name_type, &nft_object::owner>
                >
            >,
            ordered_non_unique<
                tag<by_asset_str_owner>,
                composite_key<
                    nft_object,
                    const_mem_fun<nft_object, std::string, &nft_object::name_str>,
                    member<nft_object, account_name_type, &nft_object::owner>
                >
            >,
            ordered_unique<
                tag<by_owner>,
                composite_key<
                    nft_object,
                    member<nft_object, account_name_type, &nft_object::owner>,
                    member<nft_object, uint32_t, &nft_object::token_id>
                >
            >,
            ordered_non_unique<
                tag<by_issued>,
                member<nft_object, time_point_sec, &nft_object::issued>,
                std::greater<time_point_sec>
            >,
            ordered_non_unique<
                tag<by_last_update>,
                member<nft_object, time_point_sec, &nft_object::last_update>,
                std::greater<time_point_sec>
            >,
            ordered_non_unique<
                tag<by_last_price>,
                const_mem_fun<nft_object, double, &nft_object::price_real>,
                std::greater<double>
            >,
            ordered_non_unique<
                tag<by_auction_expiration>,
                member<nft_object, time_point_sec, &nft_object::auction_expiration>,
                std::less<time_point_sec>
            >
        >,
        allocator<nft_object>
    >;

    struct by_owner_order_id;
    struct by_price;

    using nft_order_index = multi_index_container<
        nft_order_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<nft_order_object, nft_order_object_id_type, &nft_order_object::id>
            >,
            ordered_non_unique<
                tag<by_token_id>,
                member<nft_order_object, uint32_t, &nft_order_object::token_id>
            >,
            ordered_unique<
                tag<by_owner_order_id>,
                composite_key<
                    nft_order_object,
                    member<nft_order_object, account_name_type, &nft_order_object::owner>,
                    member<nft_order_object, uint32_t, &nft_order_object::order_id>
                >
            >,
            ordered_non_unique<
                tag<by_created>,
                member<nft_order_object, time_point_sec, &nft_order_object::created>,
                std::greater<time_point_sec>
            >,
            ordered_non_unique<
                tag<by_price>,
                const_mem_fun<nft_order_object, double, &nft_order_object::price_real>,
                std::greater<double>
            >
        >,
        allocator<nft_order_object>
    >;

    struct by_token_owner;
    struct by_token_price;

    using nft_bet_index = multi_index_container<
        nft_bet_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<nft_bet_object, nft_bet_object_id_type, &nft_bet_object::id>
            >,
            ordered_unique<
                tag<by_token_owner>,
                composite_key<
                    nft_bet_object,
                    member<nft_bet_object, uint32_t, &nft_bet_object::token_id>,
                    member<nft_bet_object, account_name_type, &nft_bet_object::owner>
                >
            >,
            ordered_unique<
                tag<by_token_price>,
                composite_key<
                    nft_bet_object,
                    member<nft_bet_object, uint32_t, &nft_bet_object::token_id>,
                    const_mem_fun<nft_bet_object, double, &nft_bet_object::price_real>
                >,
                composite_key_compare<
                    std::less<uint32_t>,
                    std::greater<int64_t>
                >
            >
        >,
        allocator<nft_bet_object>
    >;
} } // golos::chain

CHAINBASE_SET_INDEX_TYPE(
    golos::chain::nft_collection_object,
    golos::chain::nft_collection_index);

CHAINBASE_SET_INDEX_TYPE(
    golos::chain::nft_object,
    golos::chain::nft_index);

CHAINBASE_SET_INDEX_TYPE(
    golos::chain::nft_order_object,
    golos::chain::nft_order_index);

CHAINBASE_SET_INDEX_TYPE(
    golos::chain::nft_bet_object,
    golos::chain::nft_bet_index);
