#pragma once

#include <golos/protocol/authority.hpp>
#include <golos/protocol/steem_operations.hpp>

#include <golos/chain/steem_object_types.hpp>

#include <boost/multi_index/composite_key.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/interprocess/containers/flat_set.hpp>


namespace golos {
    namespace chain {

        namespace bip = boost::interprocess;
        using golos::protocol::asset;
        using golos::protocol::price;
        using golos::protocol::asset_symbol_type;

        /**
         *  This object is used to track pending requests to convert GBG to GOLOS (and vice-versa)
         */
        class convert_request_object
                : public object<convert_request_object_type, convert_request_object> {
        public:
            template<typename Constructor, typename Allocator>
            convert_request_object(Constructor &&c, allocator<Allocator> a) {
                c(*this);
            }

            convert_request_object() {
            }

            id_type id;

            account_name_type owner;
            uint32_t requestid = 0; ///< id set by owner, the owner,requestid pair must be unique
            asset amount;
            asset fee; //< in GOLOS-GBG case. Amount includes this fee. Separate field is for virtual operation
            time_point_sec conversion_date; ///< at this time the feed_history_median_price * amount
        };


        class escrow_object : public object<escrow_object_type, escrow_object> {
        public:
            template<typename Constructor, typename Allocator>
            escrow_object(Constructor &&c, allocator <Allocator> a) {
                c(*this);
            }

            escrow_object() {
            }

            id_type id;

            uint32_t escrow_id = 20;
            account_name_type from;
            account_name_type to;
            account_name_type agent;
            time_point_sec ratification_deadline;
            time_point_sec escrow_expiration;
            asset sbd_balance;
            asset steem_balance;
            asset pending_fee;
            bool to_approved = false;
            bool agent_approved = false;
            bool disputed = false;

            bool is_approved() const {
                return to_approved && agent_approved;
            }
        };


        class savings_withdraw_object
                : public object<savings_withdraw_object_type, savings_withdraw_object> {
            savings_withdraw_object() = delete;

        public:
            template<typename Constructor, typename Allocator>
            savings_withdraw_object(Constructor &&c, allocator <Allocator> a)
                    :memo(a) {
                c(*this);
            }

            id_type id;

            account_name_type from;
            account_name_type to;
            shared_string memo;
            uint32_t request_id = 0;
            asset amount;
            time_point_sec complete;
        };


        /**
         *  If last_update is greater than 1 week, then volume gets reset to 0
         *
         *  When a user is a maker, their volume increases
         *  When a user is a taker, their volume decreases
         *
         *  Every 1000 blocks, the account that has the highest volume_weight() is paid the maximum of
         *  1000 STEEM or 1000 * virtual_supply / (100*blocks_per_year) aka 10 * virtual_supply / blocks_per_year
         *
         *  After being paid volume gets reset to 0
         */
        class liquidity_reward_balance_object
                : public object<liquidity_reward_balance_object_type, liquidity_reward_balance_object> {
        public:
            template<typename Constructor, typename Allocator>
            liquidity_reward_balance_object(Constructor &&c, allocator <Allocator> a) {
                c(*this);
            }

            liquidity_reward_balance_object() {
            }

            id_type id;

            account_id_type owner;
            int64_t steem_volume = 0;
            int64_t sbd_volume = 0;
            uint128_t weight = 0;

            time_point_sec last_update = fc::time_point_sec::min(); /// used to decay negative liquidity balances. block num

            /// this is the sort index
            uint128_t volume_weight() const {
                return steem_volume * sbd_volume * is_positive();
            }

            uint128_t min_volume_weight() const {
                return std::min(steem_volume, sbd_volume) * is_positive();
            }

            void update_weight(bool hf9) {
                weight = hf9 ? min_volume_weight() : volume_weight();
            }

            inline int is_positive() const {
                return (steem_volume > 0 && sbd_volume > 0) ? 1 : 0;
            }
        };


        /**
         *  This object gets updated once per hour, on the hour
         */
        class feed_history_object
                : public object<feed_history_object_type, feed_history_object> {
            feed_history_object() = delete;

        public:
            template<typename Constructor, typename Allocator>
            feed_history_object(Constructor &&c, allocator <Allocator> a)
                    :price_history(a.get_segment_manager()) {
                c(*this);
            }

            id_type id;

            price current_median_history; ///< the current median of the price history, used as the base for convert operations
            price witness_median_history; ///< the current median of the price history not limited with min_price (raw median from witnesses)
            boost::interprocess::deque <price, allocator<price>> price_history; ///< tracks this last week of median_feed one per hour
        };


        /**
         *  @brief an offer to sell a amount of a asset at a specified exchange rate by a certain time
         *  @ingroup object
         *  @ingroup protocol
         *  @ingroup market
         *
         *  This limit_order_objects are indexed by @ref expiration and is automatically deleted on the first block after expiration.
         */
        class limit_order_object
                : public object<limit_order_object_type, limit_order_object> {
        public:
            template<typename Constructor, typename Allocator>
            limit_order_object(Constructor &&c, allocator <Allocator> a) {
                c(*this);
            }

            limit_order_object() {
            }

            id_type id;

            time_point_sec created;
            time_point_sec expiration;
            account_name_type seller;
            uint32_t orderid = 0;
            share_type for_sale; ///< asset id is sell_price.base.symbol
            asset_symbol_type symbol;
            price sell_price;

            pair <asset_symbol_type, asset_symbol_type> get_market() const {
                return sell_price.base.symbol < sell_price.quote.symbol ?
                       std::make_pair(sell_price.base.symbol, sell_price.quote.symbol)
                                                                        :
                       std::make_pair(sell_price.quote.symbol, sell_price.base.symbol);
            }

            asset amount_for_sale() const {
                return asset(for_sale, sell_price.base.symbol);
            }

            asset amount_to_receive() const {
                return amount_for_sale() * sell_price;
            }

            price buy_price() const {
                return ~sell_price;
            }
        };


        /**
         * @breif a route to send withdrawn vesting shares.
         */
        class withdraw_vesting_route_object
                : public object<withdraw_vesting_route_object_type, withdraw_vesting_route_object> {
        public:
            template<typename Constructor, typename Allocator>
            withdraw_vesting_route_object(Constructor &&c, allocator <Allocator> a) {
                c(*this);
            }

            withdraw_vesting_route_object() {
            }

            id_type id;

            account_id_type from_account;
            account_id_type to_account;
            uint16_t percent = 0;
            bool auto_vest = false;
        };


        class decline_voting_rights_request_object
                : public object<decline_voting_rights_request_object_type, decline_voting_rights_request_object> {
        public:
            template<typename Constructor, typename Allocator>
            decline_voting_rights_request_object(Constructor &&c, allocator <Allocator> a) {
                c(*this);
            }

            decline_voting_rights_request_object() {
            }

            id_type id;

            account_id_type account;
            time_point_sec effective_date;
        };

        class donate_object: public object<donate_object_type, donate_object> {
        public:
            donate_object() = delete;

            template<typename Constructor, typename Allocator>
            donate_object(Constructor&& c, allocator<Allocator> a) : target(a) {
                c(*this);
            }

            id_type id;

            account_name_type app;
            uint16_t version;
            shared_string target;
        };

        class asset_object: public object<asset_object_type, asset_object> {
        public:
            asset_object() = delete;

            template<typename Constructor, typename Allocator>
            asset_object(Constructor&& c, allocator<Allocator> a) : symbols_whitelist(a), json_metadata(a) {
                c(*this);
            }

            id_type id;

            account_name_type creator;
            asset max_supply;
            asset supply;
            bool allow_fee = false;
            bool allow_override_transfer = false;
            time_point_sec created;
            time_point_sec modified;
            time_point_sec marketed;
            using symbol_allocator_type = allocator<asset_symbol_type>;
            using symbol_set_type = bip::flat_set<asset_symbol_type, std::less<asset_symbol_type>, symbol_allocator_type>;
            symbol_set_type symbols_whitelist;
            uint16_t fee_percent = 0;
            shared_string json_metadata;

            asset_symbol_type symbol() const {
                return supply.symbol;
            }

            std::string symbol_name() const {
                return supply.symbol_name();
            }

            bool whitelists(asset_symbol_type symbol) const {
                return !symbols_whitelist.size() || symbols_whitelist.count(symbol);
            }
        };

        class market_pair_object: public object<market_pair_object_type, market_pair_object> {
        public:
            market_pair_object() = delete;

            template<typename Constructor, typename Allocator>
            market_pair_object(Constructor&& c, allocator<Allocator> a) {
                c(*this);
            }

            id_type id;
            asset base_depth;
            asset quote_depth;

            asset_symbol_type base() const {
                return base_depth.symbol;
            }

            asset_symbol_type quote() const {
                return quote_depth.symbol;
            }
        };

        class fix_me_object: public object<fix_me_object_type, fix_me_object> {
        public:
            fix_me_object() = delete;

            template<typename Constructor, typename Allocator>
            fix_me_object(Constructor&& c, allocator<Allocator> a) {
                c(*this);
            }

            id_type id;
            account_id_type account; // TODO: can be abstract string/int identifier
        };

        struct by_price;
        struct by_expiration;
        struct by_account;
        struct by_symbol;
        struct by_buy_price;
        using limit_order_index = multi_index_container<
            limit_order_object,
            indexed_by<
                ordered_unique<tag<by_id>,
                    member<limit_order_object, limit_order_id_type, &limit_order_object::id>
                >,
                ordered_non_unique<tag<by_expiration>,
                    member<limit_order_object, time_point_sec, &limit_order_object::expiration>
                >,
                ordered_unique<tag<by_price>, composite_key<limit_order_object,
                    member<limit_order_object, price, &limit_order_object::sell_price>,
                    member<limit_order_object, limit_order_id_type, &limit_order_object::id>
                >, composite_key_compare<
                    std::greater<price>,
                    std::less<limit_order_id_type>
                >>,
                ordered_unique<tag<by_account>, composite_key<limit_order_object,
                    member<limit_order_object, account_name_type, &limit_order_object::seller>,
                    member<limit_order_object, uint32_t, &limit_order_object::orderid>
                >>,
                ordered_non_unique<tag<by_symbol>,
                    member<limit_order_object, asset_symbol_type, &limit_order_object::symbol>
                >,
                ordered_unique<tag<by_buy_price>, composite_key<limit_order_object,
                    const_mem_fun<limit_order_object, price, &limit_order_object::buy_price>,
                    member<limit_order_object, limit_order_id_type, &limit_order_object::id>
                >, composite_key_compare<
                    std::less<price>,
                    std::less<limit_order_id_type>
                >>
            >,
            allocator<limit_order_object>
        >;

        struct by_owner;
        struct by_conversion_date;
        using convert_request_index = multi_index_container<
            convert_request_object,
            indexed_by<
                ordered_unique<tag<by_id>,
                    member<convert_request_object, convert_request_id_type, &convert_request_object::id>
                >,
                ordered_unique<tag<by_conversion_date>, composite_key<convert_request_object,
                    member<convert_request_object, time_point_sec, &convert_request_object::conversion_date>,
                    member<convert_request_object, convert_request_id_type, &convert_request_object::id>
                >>,
                ordered_unique<tag<by_owner>, composite_key<convert_request_object,
                    member<convert_request_object, account_name_type, &convert_request_object::owner>,
                    member<convert_request_object, uint32_t, &convert_request_object::requestid>
                >>
            >,
            allocator <convert_request_object>
        >;

        struct by_owner;
        struct by_volume_weight;

        typedef multi_index_container <
        liquidity_reward_balance_object,
        indexed_by<
                ordered_unique < tag <
                by_id>, member<liquidity_reward_balance_object, liquidity_reward_balance_id_type, &liquidity_reward_balance_object::id>>,
        ordered_unique <tag<by_owner>, member<liquidity_reward_balance_object, account_id_type, &liquidity_reward_balance_object::owner>>,
        ordered_unique <tag<by_volume_weight>,
        composite_key<liquidity_reward_balance_object,
                member <
                liquidity_reward_balance_object, fc::uint128_t, &liquidity_reward_balance_object::weight>,
        member<liquidity_reward_balance_object, account_id_type, &liquidity_reward_balance_object::owner>
        >,
        composite_key_compare <std::greater<fc::uint128_t>, std::less<account_id_type>>
        >
        >,
        allocator <liquidity_reward_balance_object>
        >
        liquidity_reward_balance_index;

        typedef multi_index_container <
        feed_history_object,
        indexed_by<
                ordered_unique < tag <
                by_id>, member<feed_history_object, feed_history_id_type, &feed_history_object::id>>
        >,
        allocator <feed_history_object>
        >
        feed_history_index;

        struct by_withdraw_route;
        struct by_destination;
        typedef multi_index_container <
        withdraw_vesting_route_object,
        indexed_by<
                ordered_unique < tag <
                by_id>, member<withdraw_vesting_route_object, withdraw_vesting_route_id_type, &withdraw_vesting_route_object::id>>,
        ordered_unique <tag<by_withdraw_route>,
        composite_key<withdraw_vesting_route_object,
                member <
                withdraw_vesting_route_object, account_id_type, &withdraw_vesting_route_object::from_account>,
        member<withdraw_vesting_route_object, account_id_type, &withdraw_vesting_route_object::to_account>
        >,
        composite_key_compare <std::less<account_id_type>, std::less<account_id_type>>
        >,
        ordered_unique <tag<by_destination>,
        composite_key<withdraw_vesting_route_object,
                member <
                withdraw_vesting_route_object, account_id_type, &withdraw_vesting_route_object::to_account>,
        member<withdraw_vesting_route_object, withdraw_vesting_route_id_type, &withdraw_vesting_route_object::id>
        >
        >
        >,
        allocator <withdraw_vesting_route_object>
        >
        withdraw_vesting_route_index;

        struct by_from_id;
        struct by_to;
        struct by_agent;
        struct by_ratification_deadline;
        struct by_sbd_balance;
        typedef multi_index_container <
        escrow_object,
        indexed_by<
                ordered_unique < tag <
                by_id>, member<escrow_object, escrow_id_type, &escrow_object::id>>,
        ordered_unique <tag<by_from_id>,
        composite_key<escrow_object,
                member <
                escrow_object, account_name_type, &escrow_object::from>,
        member<escrow_object, uint32_t, &escrow_object::escrow_id>
        >
        >,
        ordered_unique <tag<by_to>,
        composite_key<escrow_object,
                member < escrow_object, account_name_type, &escrow_object::to>,
        member<escrow_object, escrow_id_type, &escrow_object::id>
        >
        >,
        ordered_unique <tag<by_agent>,
        composite_key<escrow_object,
                member <
                escrow_object, account_name_type, &escrow_object::agent>,
        member<escrow_object, escrow_id_type, &escrow_object::id>
        >
        >,
        ordered_unique <tag<by_ratification_deadline>,
        composite_key<escrow_object,
                const_mem_fun <
                escrow_object, bool, &escrow_object::is_approved>,
        member<escrow_object, time_point_sec, &escrow_object::ratification_deadline>,
        member<escrow_object, escrow_id_type, &escrow_object::id>
        >,
        composite_key_compare <std::less<bool>, std::less<time_point_sec>, std::less<escrow_id_type>>
        >,
        ordered_unique <tag<by_sbd_balance>,
        composite_key<escrow_object,
                member < escrow_object, asset, &escrow_object::sbd_balance>,
        member<escrow_object, escrow_id_type, &escrow_object::id>
        >,
        composite_key_compare <std::greater<asset>, std::less<escrow_id_type>>
        >
        >,
        allocator <escrow_object>
        >
        escrow_index;

        struct by_from_rid;
        struct by_to_complete;
        struct by_complete_from_rid;
        typedef multi_index_container <
        savings_withdraw_object,
        indexed_by<
                ordered_unique < tag <
                by_id>, member<savings_withdraw_object, savings_withdraw_id_type, &savings_withdraw_object::id>>,
        ordered_unique <tag<by_from_rid>,
        composite_key<savings_withdraw_object,
                member <
                savings_withdraw_object, account_name_type, &savings_withdraw_object::from>,
        member<savings_withdraw_object, uint32_t, &savings_withdraw_object::request_id>
        >
        >,
        ordered_unique <tag<by_to_complete>,
        composite_key<savings_withdraw_object,
                member <
                savings_withdraw_object, account_name_type, &savings_withdraw_object::to>,
        member<savings_withdraw_object, time_point_sec, &savings_withdraw_object::complete>,
        member<savings_withdraw_object, savings_withdraw_id_type, &savings_withdraw_object::id>
        >
        >,
        ordered_unique <tag<by_complete_from_rid>,
        composite_key<savings_withdraw_object,
                member <
                savings_withdraw_object, time_point_sec, &savings_withdraw_object::complete>,
        member<savings_withdraw_object, account_name_type, &savings_withdraw_object::from>,
        member<savings_withdraw_object, uint32_t, &savings_withdraw_object::request_id>
        >
        >
        >,
        allocator <savings_withdraw_object>
        >
        savings_withdraw_index;

        struct by_account;
        struct by_effective_date;
        typedef multi_index_container <
        decline_voting_rights_request_object,
        indexed_by<
                ordered_unique < tag <
                by_id>, member<decline_voting_rights_request_object, decline_voting_rights_request_id_type, &decline_voting_rights_request_object::id>>,
        ordered_unique <tag<by_account>,
        member<decline_voting_rights_request_object, account_id_type, &decline_voting_rights_request_object::account>
        >,
        ordered_unique <tag<by_effective_date>,
        composite_key<decline_voting_rights_request_object,
                member <
                decline_voting_rights_request_object, time_point_sec, &decline_voting_rights_request_object::effective_date>,
        member<decline_voting_rights_request_object, account_id_type, &decline_voting_rights_request_object::account>
        >,
        composite_key_compare <std::less<time_point_sec>, std::less<account_id_type>>
        >
        >,
        allocator <decline_voting_rights_request_object>
        >
        decline_voting_rights_request_index;

        struct by_app_version;

        using donate_index = multi_index_container<
            donate_object,
            indexed_by<
                ordered_unique<tag<by_id>, member<donate_object, donate_object_id_type, &donate_object::id>>,
                ordered_non_unique<tag<by_app_version>, composite_key<donate_object,
                    member<donate_object, account_name_type, &donate_object::app>,
                    member<donate_object, uint16_t, &donate_object::version>
                >>>,
            allocator<donate_object>>;

        struct by_creator_symbol_name;
        struct by_creator_marketed;
        struct by_symbol_name;
        struct by_marketed;

        using asset_index = multi_index_container<
            asset_object,
            indexed_by<
                ordered_unique<tag<by_id>,
                    member<asset_object, asset_object_id_type, &asset_object::id>
                >,
                ordered_unique<tag<by_creator_symbol_name>, composite_key<asset_object,
                    member<asset_object, account_name_type, &asset_object::creator>,
                    const_mem_fun<asset_object, std::string, &asset_object::symbol_name>
                >>,
                ordered_unique<tag<by_creator_marketed>, composite_key<asset_object,
                    member<asset_object, account_name_type, &asset_object::creator>,
                    member<asset_object, time_point_sec, &asset_object::marketed>,
                    member<asset_object, asset_object_id_type, &asset_object::id>
                >, composite_key_compare<
                    std::less<account_name_type>,
                    std::greater<time_point_sec>,
                    std::greater<asset_object_id_type>
                >>,
                ordered_unique<tag<by_symbol>,
                    const_mem_fun<asset_object, asset_symbol_type, &asset_object::symbol>
                >,
                ordered_unique<tag<by_symbol_name>,
                    const_mem_fun<asset_object, std::string, &asset_object::symbol_name>
                >,
                ordered_unique<tag<by_marketed>, composite_key<asset_object,
                    member<asset_object, time_point_sec, &asset_object::marketed>,
                    member<asset_object, asset_object_id_type, &asset_object::id>
                >, composite_key_compare<
                    std::greater<time_point_sec>,
                    std::greater<asset_object_id_type>
                >>
            >, allocator<asset_object>>;

        struct by_base_quote;
        using market_pair_index = multi_index_container<
            market_pair_object,
            indexed_by<
                ordered_unique<tag<by_id>,
                    member<market_pair_object, market_pair_object_id_type, &market_pair_object::id>
                >,
                ordered_unique<tag<by_base_quote>, composite_key<market_pair_object,
                    const_mem_fun<market_pair_object, asset_symbol_type, &market_pair_object::base>,
                    const_mem_fun<market_pair_object, asset_symbol_type, &market_pair_object::quote>
                >>
            >,
            allocator<market_pair_object>
        >;

        using fix_me_index = multi_index_container<
            fix_me_object,
            indexed_by<
                ordered_unique<tag<by_id>,
                    member<fix_me_object, fix_me_object_id_type, &fix_me_object::id>
                >,
                ordered_unique<tag<by_account>,
                    member<fix_me_object, account_id_type, &fix_me_object::account>
                >
            >,
            allocator<fix_me_object>
        >;
    }
} // golos::chain

#include <golos/chain/comment_object.hpp>
#include <golos/chain/account_object.hpp>


FC_REFLECT((golos::chain::limit_order_object),
        (id)(created)(expiration)(seller)(orderid)(for_sale)(sell_price))
CHAINBASE_SET_INDEX_TYPE(golos::chain::limit_order_object, golos::chain::limit_order_index)

FC_REFLECT((golos::chain::feed_history_object),
        (id)(current_median_history)(witness_median_history)(price_history))
CHAINBASE_SET_INDEX_TYPE(golos::chain::feed_history_object, golos::chain::feed_history_index)

FC_REFLECT((golos::chain::convert_request_object),
        (id)(owner)(requestid)(amount)(fee)(conversion_date))
CHAINBASE_SET_INDEX_TYPE(golos::chain::convert_request_object, golos::chain::convert_request_index)

FC_REFLECT((golos::chain::liquidity_reward_balance_object),
        (id)(owner)(steem_volume)(sbd_volume)(weight)(last_update))
CHAINBASE_SET_INDEX_TYPE(golos::chain::liquidity_reward_balance_object, golos::chain::liquidity_reward_balance_index)

FC_REFLECT((golos::chain::withdraw_vesting_route_object),
        (id)(from_account)(to_account)(percent)(auto_vest))
CHAINBASE_SET_INDEX_TYPE(golos::chain::withdraw_vesting_route_object, golos::chain::withdraw_vesting_route_index)

FC_REFLECT((golos::chain::savings_withdraw_object),
        (id)(from)(to)(memo)(request_id)(amount)(complete))
CHAINBASE_SET_INDEX_TYPE(golos::chain::savings_withdraw_object, golos::chain::savings_withdraw_index)

FC_REFLECT((golos::chain::escrow_object),
        (id)(escrow_id)(from)(to)(agent)
                (ratification_deadline)(escrow_expiration)
                (sbd_balance)(steem_balance)(pending_fee)
                (to_approved)(agent_approved)(disputed))
CHAINBASE_SET_INDEX_TYPE(golos::chain::escrow_object, golos::chain::escrow_index)

FC_REFLECT((golos::chain::decline_voting_rights_request_object),
        (id)(account)(effective_date))
CHAINBASE_SET_INDEX_TYPE(golos::chain::decline_voting_rights_request_object, golos::chain::decline_voting_rights_request_index)

CHAINBASE_SET_INDEX_TYPE(golos::chain::donate_object, golos::chain::donate_index)

CHAINBASE_SET_INDEX_TYPE(golos::chain::asset_object, golos::chain::asset_index)

FC_REFLECT((golos::chain::market_pair_object),
        (base_depth)(quote_depth))
CHAINBASE_SET_INDEX_TYPE(golos::chain::market_pair_object, golos::chain::market_pair_index)

CHAINBASE_SET_INDEX_TYPE(golos::chain::fix_me_object, golos::chain::fix_me_index)
