#pragma once

#include <golos/protocol/authority.hpp>
#include <golos/protocol/steem_operations.hpp>

#include <golos/chain/steem_object_types.hpp>

#include <boost/multi_index/composite_key.hpp>

namespace golos { namespace chain {

    using golos::protocol::digest_type;
    using golos::protocol::public_key_type;
    using golos::protocol::version;
    using golos::protocol::hardfork_version;
    using golos::protocol::price;
    using golos::protocol::asset;
    using golos::protocol::asset_symbol_type;

    using chain_properties = golos::protocol::chain_properties_30;

    /**
     *  All witnesses with at least 1% net positive approval and
     *  at least 2 weeks old are able to participate in block
     *  production.
     */
    class witness_object
            : public object<witness_object_type, witness_object> {
        witness_object() = delete;

    public:
        enum witness_schedule_type {
            top19,
            timeshare,
            miner,
            none
        };

        template<typename Constructor, typename Allocator>
        witness_object(Constructor &&c, allocator <Allocator> a)
                :url(a) {
            c(*this);
        }

        id_type id;

        /** the account that has authority over this witness */
        account_name_type owner;
        time_point_sec created;
        shared_string url;
        uint32_t total_missed = 0;
        uint64_t last_aslot = 0;
        uint64_t last_confirmed_block_num = 0;

        /**
         * Some witnesses have the job because they did a proof of work,
         * this field indicates where they were in the POW order. After
         * each round, the witness with the lowest pow_worker value greater
         * than 0 is removed.
         */
        uint64_t pow_worker = 0;

        /**
         *  This is the key used to sign blocks on behalf of this witness
         */
        public_key_type signing_key;

        chain_properties props;
        price sbd_exchange_rate;
        time_point_sec last_sbd_exchange_update;


        /**
         *  The total votes for this witness. This determines how the witness is ranked for
         *  scheduling.  The top N witnesses by votes are scheduled every round, every one
         *  else takes turns being scheduled proportional to their votes.
         */
        share_type votes;
        witness_schedule_type schedule = none; /// How the witness was scheduled the last time it was scheduled

        /**
         * These fields are used for the witness scheduling algorithm which uses
         * virtual time to ensure that all witnesses are given proportional time
         * for producing blocks.
         *
         * @ref votes is used to determine speed. The @ref virtual_scheduled_time is
         * the expected time at which this witness should complete a virtual lap which
         * is defined as the position equal to 1000 times MAXVOTES.
         *
         * virtual_scheduled_time = virtual_last_update + (1000*MAXVOTES - virtual_position) / votes
         *
         * Every time the number of votes changes the virtual_position and virtual_scheduled_time must
         * update.  There is a global current virtual_scheduled_time which gets updated every time
         * a witness is scheduled.  To update the virtual_position the following math is performed.
         *
         * virtual_position       = virtual_position + votes * (virtual_current_time - virtual_last_update)
         * virtual_last_update    = virtual_current_time
         * votes                  += delta_vote
         * virtual_scheduled_time = virtual_last_update + (1000*MAXVOTES - virtual_position) / votes
         *
         * @defgroup virtual_time Virtual Time Scheduling
         */
        ///@{
        fc::uint128_t virtual_last_update;
        fc::uint128_t virtual_position;
        fc::uint128_t virtual_scheduled_time = fc::uint128_t::max_value();
        ///@}

        digest_type last_work;

        /**
         * This field represents the Golos blockchain version the witness is running.
         */
        version running_version;

        hardfork_version hardfork_version_vote;
        time_point_sec hardfork_time_vote = STEEMIT_GENESIS_TIME;

        time_point_sec transit_to_cyberway_vote = STEEMIT_GENESIS_TIME;
    };


    class witness_vote_object
            : public object<witness_vote_object_type, witness_vote_object> {
    public:
        template<typename Constructor, typename Allocator>
        witness_vote_object(Constructor &&c, allocator <Allocator> a) {
            c(*this);
        }

        witness_vote_object() {
        }

        id_type id;

        witness_id_type witness;
        account_id_type account;
        share_type rshares;
    };

    class witness_schedule_object
            : public object<witness_schedule_object_type, witness_schedule_object> {
    public:
        template<typename Constructor, typename Allocator>
        witness_schedule_object(Constructor &&c, allocator <Allocator> a) {
            c(*this);
        }

        witness_schedule_object() {
        }

        id_type id;

        fc::uint128_t current_virtual_time;
        uint32_t next_shuffle_block_num = 1;
        fc::array<account_name_type, STEEMIT_MAX_WITNESSES> current_shuffled_witnesses;
        uint8_t num_scheduled_witnesses = 1;
        uint8_t top19_weight = 1;
        uint8_t timeshare_weight = 5;
        uint8_t miner_weight = 1;
        uint32_t witness_pay_normalization_factor = 25;
        chain_properties median_props;
        version majority_version;
    };


    struct by_vote_name;
    struct by_name;
    struct by_pow;
    struct by_work;
    struct by_schedule_time;

        /**
         * @ingroup object_index
         */
    using witness_index = multi_index_container <
        witness_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<witness_object, witness_id_type, &witness_object::id>>,
            ordered_non_unique<
                tag<by_work>,
                member<witness_object, digest_type, &witness_object::last_work>>,
            ordered_unique<
                tag<by_name>,
                member<witness_object, account_name_type, &witness_object::owner>>,
            ordered_non_unique<
                tag<by_pow>,
                member<witness_object, uint64_t, &witness_object::pow_worker>>,
            ordered_unique<
                tag<by_vote_name>,
                composite_key<
                    witness_object,
                    member<witness_object, share_type, &witness_object::votes>,
                    member<witness_object, account_name_type, &witness_object::owner>>,
                composite_key_compare<
                    std::greater<share_type>,
                    golos::protocol::string_less>>,
            ordered_unique<
                tag<by_schedule_time>,
                composite_key<
                    witness_object,
                    member<witness_object, fc::uint128_t, &witness_object::virtual_scheduled_time>,
                    member<witness_object, witness_id_type, &witness_object::id>>>>,
        allocator<witness_object>>;

    struct by_account_witness;
    struct by_witness_account;
    struct by_witness_votes;

    using witness_vote_index = multi_index_container <
        witness_vote_object,
        indexed_by<
            ordered_unique<tag<by_id>,
                member<witness_vote_object, witness_vote_id_type, &witness_vote_object::id>
            >,
            ordered_unique<tag<by_account_witness>,
                composite_key<
                    witness_vote_object,
                    member<witness_vote_object, account_id_type, &witness_vote_object::account>,
                    member<witness_vote_object, witness_id_type, &witness_vote_object::witness>
                >,
                composite_key_compare<
                    std::less<account_id_type>,
                    std::less<witness_id_type>
                >
            >,
            ordered_unique<tag<by_witness_account>,
                composite_key<
                    witness_vote_object,
                    member<witness_vote_object, witness_id_type, &witness_vote_object::witness>,
                    member<witness_vote_object, account_id_type, &witness_vote_object::account>
                >,
                composite_key_compare<
                    std::less<witness_id_type>,
                    std::less<account_id_type>
                >
            >,
            ordered_unique<tag<by_witness_votes>,
                composite_key<
                    witness_vote_object,
                    member<witness_vote_object, witness_id_type, &witness_vote_object::witness>,
                    member<witness_vote_object, share_type, &witness_vote_object::rshares>,
                    member<witness_vote_object, account_id_type, &witness_vote_object::account>
                >,
                composite_key_compare<
                    std::less<witness_id_type>,
                    std::greater<share_type>,
                    std::less<account_id_type>
                >
            >
        >, allocator<witness_vote_object>>;

    using witness_schedule_index = multi_index_container <
        witness_schedule_object,
        indexed_by<
            ordered_unique<
                tag<by_id>,
                member<witness_schedule_object, witness_schedule_id_type, &witness_schedule_object::id>>>,
        allocator <witness_schedule_object>>;

} }

FC_REFLECT_ENUM(golos::chain::witness_object::witness_schedule_type, (top19)(timeshare)(miner)(none))

FC_REFLECT(
    (golos::chain::witness_object),
    (id)(owner)(created)(url)(votes)(schedule)(virtual_last_update)(virtual_position)(virtual_scheduled_time)(total_missed)
    (last_aslot)(last_confirmed_block_num)(pow_worker)(signing_key)(props)(sbd_exchange_rate)(last_sbd_exchange_update)
    (last_work)(running_version)(hardfork_version_vote)(hardfork_time_vote)(transit_to_cyberway_vote))

CHAINBASE_SET_INDEX_TYPE(golos::chain::witness_object, golos::chain::witness_index)

FC_REFLECT(
    (golos::chain::witness_schedule_object),
    (id)(current_virtual_time)(next_shuffle_block_num)(current_shuffled_witnesses)(num_scheduled_witnesses)
    (top19_weight)(timeshare_weight)(miner_weight)(witness_pay_normalization_factor)
    (median_props)(majority_version))

CHAINBASE_SET_INDEX_TYPE(golos::chain::witness_vote_object, golos::chain::witness_vote_index)

CHAINBASE_SET_INDEX_TYPE(golos::chain::witness_schedule_object, golos::chain::witness_schedule_index)
