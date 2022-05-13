#pragma once

#include <golos/chain/steem_objects.hpp>

namespace golos { namespace plugins { namespace social_network {
    using namespace golos::chain;

    #ifndef SOCIAL_NETWORK_SPACE_ID
    #define SOCIAL_NETWORK_SPACE_ID 10
    #endif

    enum social_network_types {
        comment_content_object_type = (SOCIAL_NETWORK_SPACE_ID << 8),
        comment_last_update_object_type = (SOCIAL_NETWORK_SPACE_ID << 8) + 1,
        comment_reward_object_type = (SOCIAL_NETWORK_SPACE_ID << 8) + 2,
        donate_data_object_type = (SOCIAL_NETWORK_SPACE_ID << 8) + 3
    };


    class comment_content_object
            : public object<comment_content_object_type, comment_content_object> {
    public:
        comment_content_object() = delete;

        template<typename Constructor, typename Allocator>
        comment_content_object(Constructor&& c, allocator <Allocator> a)
                :title(a), body(a), json_metadata(a) {
            c(*this);
        }

        id_type id;

        comment_id_type   comment;

        shared_string title;
        shared_string body;
        shared_string json_metadata;
        share_type net_rshares;
        asset donates = asset(0, STEEM_SYMBOL);
        share_type donates_uia = 0;

        uint32_t block_number;
    };

    using comment_content_id_type = object_id<comment_content_object>;

    struct by_comment;
    struct by_block_number;

    using comment_content_index = multi_index_container<
          comment_content_object,
          indexed_by<
             ordered_unique<tag<by_id>, member<comment_content_object, comment_content_id_type, &comment_content_object::id>>,
             ordered_unique<tag<by_comment>, member<comment_content_object, comment_id_type, &comment_content_object::comment>>,
             ordered_non_unique<tag<by_block_number>, member<comment_content_object, uint32_t, &comment_content_object::block_number>>>,
        allocator<comment_content_object>
    >;

    class comment_last_update_object: public object<comment_last_update_object_type, comment_last_update_object> {
    public:
        comment_last_update_object() = delete;

        template<typename Constructor, typename Allocator>
        comment_last_update_object(Constructor&& c, allocator<Allocator> a)
                : parent_permlink(a) {
            c(*this);
        }

        id_type id;

        comment_id_type comment;
        account_name_type parent_author;
        shared_string parent_permlink;
        account_name_type author;
        time_point_sec last_update; ///< the last time this post was "touched" by modify
        time_point_sec active; ///< the last time this post was "touched" by reply
        comment_id_type last_reply;
        uint16_t num_changes;

        uint32_t block_number;
    };

    using comment_last_update_id_type = object_id<comment_last_update_object>;

    struct by_last_update; /// parent_auth, last_update
    struct by_author_last_update;
    struct by_parent_active;

    using comment_last_update_index = multi_index_container<
        comment_last_update_object,
        indexed_by<
            ordered_unique<tag<by_id>, member<comment_last_update_object, comment_last_update_object::id_type, &comment_last_update_object::id>>,
            ordered_unique<tag<by_comment>, member<comment_last_update_object, comment_object::id_type, &comment_last_update_object::comment>>,
            ordered_unique<
                tag<by_last_update>,
                composite_key<comment_last_update_object,
                    member<comment_last_update_object, account_name_type, &comment_last_update_object::parent_author>,
                    member<comment_last_update_object, time_point_sec, &comment_last_update_object::last_update>,
                    member<comment_last_update_object, comment_last_update_id_type, &comment_last_update_object::id>>,
                composite_key_compare<std::less<account_name_type>, std::greater<time_point_sec>, std::less<comment_last_update_id_type>>>,
            ordered_unique<
                tag<by_author_last_update>,
                composite_key<comment_last_update_object,
                    member<comment_last_update_object, account_name_type, &comment_last_update_object::author>,
                    member<comment_last_update_object, time_point_sec, &comment_last_update_object::last_update>,
                    member<comment_last_update_object, comment_last_update_id_type, &comment_last_update_object::id>>,
                composite_key_compare<std::less<account_name_type>, std::greater<time_point_sec>, std::less<comment_last_update_id_type>>>,
            ordered_non_unique<
                tag<by_block_number>,
                member<comment_last_update_object, uint32_t, &comment_last_update_object::block_number>>,
            ordered_non_unique <
                tag<by_parent_active>,
                    composite_key<comment_last_update_object,
                    member <comment_last_update_object, account_name_type, &comment_last_update_object::parent_author>,
                    member<comment_last_update_object, shared_string, &comment_last_update_object::parent_permlink>,
                    member<comment_last_update_object, time_point_sec, &comment_last_update_object::active>>,
                composite_key_compare <std::less<account_name_type>, strcmp_less, std::greater<time_point_sec>> >
        >,
        allocator<comment_last_update_object>
    >;

    class comment_reward_object: public object<comment_reward_object_type, comment_reward_object> {
    public:
        comment_reward_object() = delete;

        template<typename Constructor, typename Allocator>
        comment_reward_object(Constructor&& c, allocator<Allocator> a) {
            c(*this);
        }

        id_type id;

        comment_id_type comment;
        asset total_payout_value{0, SBD_SYMBOL};
        share_type author_rewards = 0;
        asset author_gbg_payout_value{0, SBD_SYMBOL};
        asset author_golos_payout_value{0, STEEM_SYMBOL};
        asset author_gests_payout_value{0, VESTS_SYMBOL};
        asset beneficiary_payout_value{0, SBD_SYMBOL};
        asset beneficiary_gests_payout_value{0, VESTS_SYMBOL};
        asset curator_payout_value{0, SBD_SYMBOL};
        asset curator_gests_payout_value{0, VESTS_SYMBOL};
    };

    using comment_reward_id_type = object_id<comment_reward_object>;

    using comment_reward_index = multi_index_container<
        comment_reward_object,
        indexed_by<
            ordered_unique<tag<by_id>, member<comment_reward_object, comment_reward_object::id_type, &comment_reward_object::id>>,
            ordered_unique<tag<by_comment>, member<comment_reward_object, comment_object::id_type, &comment_reward_object::comment>>>,
        allocator<comment_reward_object>>;

    class donate_data_object
            : public object<donate_data_object_type, donate_data_object> {
    public:
        donate_data_object() = delete;

        template<typename Constructor, typename Allocator>
        donate_data_object(Constructor&& c, allocator <Allocator> a)
                : comment(a) {
            c(*this);
        }

        id_type id;

        donate_object_id_type donate;
        account_name_type from;
        account_name_type to;
        asset amount;
        bool uia;
        fc::sha256 target_id;
        shared_string comment;
        time_point_sec time;
        bool wrong;

        share_type get_amount() const {
            return (amount.amount / amount.precision());
        }
    };

    class donate_api_object {
    public:
        account_name_type from;
        account_name_type to;
        asset amount;
        bool uia;
        account_name_type app;
        uint16_t version;
        string target;
        fc::sha256 target_id;
        string comment;
        time_point_sec time;
        bool wrong;

        donate_api_object(const donate_data_object& don, const database& db) {
            from = don.from; to = don.to; amount = don.amount; uia = don.uia;
            auto donate = db.get_donate(don.donate);
            app = donate.app;
            version = donate.version;
            target = to_string(donate.target);
            target_id = don.target_id;
            comment = to_string(don.comment);
            time = don.time;
            wrong = don.wrong;
        }

        share_type get_amount() const {
            return (amount.amount / amount.precision());
        }
    };

    using donate_data_id_type = object_id<donate_data_object>;

    struct by_donate;
    struct by_from_to;
    struct by_to_from;
    struct by_target_from_to;
    struct by_target_amount;

    using donate_data_index = multi_index_container<
        donate_data_object,
        indexed_by<
            ordered_unique<tag<by_id>, member<donate_data_object, donate_data_id_type, &donate_data_object::id>>,
            ordered_unique<tag<by_donate>, member<donate_data_object, donate_object_id_type, &donate_data_object::donate>>,
            ordered_non_unique<tag<by_from_to>, composite_key<donate_data_object,
                member<donate_data_object, account_name_type, &donate_data_object::from>,
                member<donate_data_object, account_name_type, &donate_data_object::to>
            >>,
            ordered_non_unique<tag<by_to_from>, composite_key<donate_data_object,
                member<donate_data_object, account_name_type, &donate_data_object::to>,
                member<donate_data_object, account_name_type, &donate_data_object::from>
            >>,
            ordered_non_unique<tag<by_target_from_to>, composite_key<donate_data_object,
                member<donate_data_object, fc::sha256, &donate_data_object::target_id>,
                member<donate_data_object, account_name_type, &donate_data_object::from>,
                member<donate_data_object, account_name_type, &donate_data_object::to>
            >>,
            ordered_non_unique<tag<by_target_amount>, composite_key<donate_data_object,
                member<donate_data_object, fc::sha256, &donate_data_object::target_id>,
                member<donate_data_object, bool, &donate_data_object::uia>,
                const_mem_fun<donate_data_object, share_type, &donate_data_object::get_amount>
            >, composite_key_compare<std::less<fc::sha256>, std::less<bool>, std::greater<share_type>>>>,
        allocator<donate_data_object>
    >;
} } }


CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::social_network::comment_content_object,
    golos::plugins::social_network::comment_content_index
)

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::social_network::comment_last_update_object,
    golos::plugins::social_network::comment_last_update_index)

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::social_network::comment_reward_object,
    golos::plugins::social_network::comment_reward_index)

CHAINBASE_SET_INDEX_TYPE(
    golos::plugins::social_network::donate_data_object,
    golos::plugins::social_network::donate_data_index)
FC_REFLECT((golos::plugins::social_network::donate_api_object),
    (from)(to)(amount)(uia)(app)(version)(target)(target_id)(comment)(time)(wrong))
