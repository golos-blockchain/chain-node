#pragma once

#include <chainbase/chainbase.hpp>
#include <golos/plugins/json_rpc/utility.hpp>
#include <golos/plugins/json_rpc/plugin.hpp>
#include <golos/chain/steem_object_types.hpp>
#include <golos/chain/steem_objects.hpp>
#include <golos/protocol/types.hpp>
#include <golos/protocol/asset.hpp>
#include <golos/protocol/steem_virtual_operations.hpp>

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//
#ifndef MARKET_HISTORY_SPACE_ID
#define MARKET_HISTORY_SPACE_ID 7
#endif

namespace golos {
    namespace plugins {
        namespace market_history {

            using namespace golos::protocol;
            using namespace boost::multi_index;
            using namespace chainbase;
            using namespace golos::plugins;

            enum market_history_object_types {
                bucket_object_type = (MARKET_HISTORY_SPACE_ID << 8),
                order_history_object_type = (MARKET_HISTORY_SPACE_ID << 8) + 1
            };

            // Api params
            struct market_ticker {
                double latest = 0;
                double lowest_ask = 0;
                double highest_bid = 0;
                double percent_change = 0;
                asset asset1_volume;
                asset asset2_volume;
                asset asset1_depth;
                asset asset2_depth;
            };

            struct market_volume {
                asset asset1_volume;
                asset asset2_volume;
            };

            struct market_depth {
                asset asset1_depth;
                asset asset2_depth;
            };

            struct order {
                double price;

                share_type asset1;
                share_type asset2;
            };


            struct order_extended {
                price order_price;
                double real_price; // dollars per steem

                share_type asset1;
                share_type asset2;

                fc::time_point_sec created;
            };

            struct order_book_extended {
                vector <order_extended> bids;
                vector <order_extended> asks;
            };


            struct order_book {
                vector <order> bids;
                vector <order> asks;
            };

            struct market_trade {
                time_point_sec date;
                asset current_pays;
                asset open_pays;
            };

            typedef golos::chain::limit_order_object limit_order_api_object;

            struct limit_order : public limit_order_api_object {
                limit_order() {
                }

                limit_order(const limit_order_object &o)
                        : limit_order_api_object(o) {
                }

                double real_price = 0;
                bool rewarded = false;
            };
          
            struct bucket_object
                    : public object<bucket_object_type, bucket_object> {
                template<typename Constructor, typename Allocator>
                bucket_object(Constructor &&c, allocator <Allocator> a) {
                    c(*this);
                }

                bucket_object() {
                }

                id_type id;

                asset_symbol_type sym1 = 0;
                asset_symbol_type sym2 = 0;
                fc::time_point_sec open;
                uint32_t seconds = 0;
                share_type high_asset1;
                share_type high_asset2;
                share_type low_asset1;
                share_type low_asset2;
                share_type open_asset1;
                share_type open_asset2;
                share_type close_asset1;
                share_type close_asset2;
                share_type asset1_volume;
                share_type asset2_volume;

                golos::protocol::price high() const {
                    return asset(high_asset2, sym2) /
                           asset(high_asset1, sym1);
                }

                golos::protocol::price low() const {
                    return asset(low_asset2, sym2) /
                           asset(low_asset1, sym1);
                }
            };

            typedef object_id <bucket_object> bucket_id_type;


            struct order_history_object
                    : public object<order_history_object_type, order_history_object> {
                template<typename Constructor, typename Allocator>
                order_history_object(Constructor &&c, allocator <Allocator> a) {
                    c(*this);
                }

                id_type id;

                fc::time_point_sec time;
                golos::protocol::fill_order_operation op;
            };

            typedef object_id <order_history_object> order_history_id_type;

            struct by_id;
            struct by_bucket;

            using bucket_index = multi_index_container<
                bucket_object,
                indexed_by<
                    ordered_unique<tag<by_id>,
                        member<bucket_object, bucket_id_type, &bucket_object::id>
                    >,
                    ordered_unique<tag<by_bucket>, composite_key<bucket_object,
                        member<bucket_object, asset_symbol_type, &bucket_object::sym1>,
                        member<bucket_object, asset_symbol_type, &bucket_object::sym2>,
                        member<bucket_object, uint32_t, &bucket_object::seconds>,
                        member<bucket_object, fc::time_point_sec, &bucket_object::open>
                    >>
                >, allocator<bucket_object>>;

            struct by_time;

            using order_history_index = multi_index_container<
                order_history_object,
                indexed_by<
                    ordered_unique<tag<by_id>,
                        member<order_history_object, order_history_id_type, &order_history_object::id>
                    >,
                    ordered_non_unique<tag<by_time>,
                        member<order_history_object, time_point_sec, &order_history_object::time>
                    >
                >, allocator<order_history_object>>;
        }
    }
} // golos::plugins::market_history

FC_REFLECT((golos::plugins::market_history::market_ticker),
           (latest)(lowest_ask)(highest_bid)(percent_change)(asset1_volume)(asset2_volume)(asset1_depth)(asset2_depth));
FC_REFLECT((golos::plugins::market_history::market_volume),
           (asset1_volume)(asset2_volume));
FC_REFLECT((golos::plugins::market_history::market_depth),
           (asset1_depth)(asset2_depth));
FC_REFLECT((golos::plugins::market_history::order),
           (price)(asset1)(asset2));
FC_REFLECT((golos::plugins::market_history::order_book),
           (bids)(asks));
FC_REFLECT((golos::plugins::market_history::market_trade),
           (date)(current_pays)(open_pays));

FC_REFLECT_DERIVED((golos::plugins::market_history::limit_order),((golos::plugins::market_history::limit_order_api_object)) ,(real_price)(rewarded));

FC_REFLECT((golos::plugins::market_history::order_extended), (order_price)(real_price)(asset1)(asset2)(created));
FC_REFLECT((golos::plugins::market_history::order_book_extended), (asks)(bids));


FC_REFLECT((golos::plugins::market_history::bucket_object),
           (id)
                   (open)(seconds)
                   (high_asset1)(high_asset2)
                   (low_asset1)(low_asset2)
                   (open_asset1)(open_asset2)
                   (close_asset1)(close_asset2)
                   (asset1_volume)(asset2_volume))
CHAINBASE_SET_INDEX_TYPE(golos::plugins::market_history::bucket_object, golos::plugins::market_history::bucket_index)

FC_REFLECT((golos::plugins::market_history::order_history_object),(id)(time)(op))
CHAINBASE_SET_INDEX_TYPE(golos::plugins::market_history::order_history_object, golos::plugins::market_history::order_history_index)
