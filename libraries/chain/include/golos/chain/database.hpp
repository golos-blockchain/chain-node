#pragma once

#include <golos/chain/global_property_object.hpp>
#include <golos/chain/node_property_object.hpp>
#include <golos/chain/worker_objects.hpp>
#include <golos/chain/event_objects.hpp>
#include <golos/chain/paid_subscription_objects.hpp>
#include <golos/chain/nft_objects.hpp>
#include <golos/chain/comment_bill.hpp>
#include <golos/chain/fork_database.hpp>
#include <golos/chain/block_log.hpp>
#include <golos/chain/hardfork.hpp>
#include <golos/protocol/protocol.hpp>

#include <fc/signals.hpp>

#include <fc/log/logger.hpp>

#include <map>

namespace golos { namespace chain {

        using golos::protocol::signed_transaction;
        using golos::protocol::operation;
        using golos::protocol::authority;
        using golos::protocol::asset;
        using golos::protocol::asset_symbol_type;
        using golos::protocol::price;

        class database_impl;

        class custom_operation_interpreter;

        struct operation_notification;

        struct comment_curation_info;

        /**
         *   @class database
         *   @brief tracks the blockchain state in an extensible manner
         */
        class database : public chainbase::database {
        public:
            database();

            ~database();

#ifdef STEEMIT_BUILD_TESTNET
            bool _test_freezing = false;
#endif

            using chainbase::database::remove;

            bool is_producing() const {
                return _is_producing;
            }

            void set_producing(bool p) {
                _is_producing = p;
            }

            bool is_generating() const {
                return _is_generating;
            }

            void set_generating(bool p) {
                _is_generating = p;
            }

            bool is_reindexing() const {
                return _is_reindexing;
            }

            void set_reindexing(bool p) {
                _is_reindexing = p;
            }

            bool is_transit_enabled() const;

            bool _is_producing = false;
            bool _is_generating = false;
            bool _is_testing = false;           ///< set for tests to avoid low free memory spam
            bool _is_reindexing = false;
            bool _log_hardforks = true;
            uint32_t _fixed_irreversible_block_num = UINT32_MAX;

            enum validation_steps {
                skip_nothing = 0,
                skip_witness_signature = 1 << 0,  ///< used while reindexing
                skip_transaction_signatures = 1 << 1,  ///< used by non-witness nodes
                skip_transaction_dupe_check = 1 << 2,  ///< used while reindexing
                skip_fork_db = 1 << 3,  ///< used while reindexing
                skip_block_size_check = 1 << 4,  ///< used when applying locally generated transactions
                skip_tapos_check = 1 << 5,  ///< used while reindexing -- note this skips expiration check as well
                skip_authority_check = 1 << 6,  ///< used while reindexing -- disables any checking of authority on transactions
                skip_merkle_check = 1 << 7,  ///< used while reindexing
                skip_undo_history_check = 1 << 8,  ///< used while reindexing
                skip_witness_schedule_check = 1 << 9,  ///< used while reindexing
                skip_validate_operations = 1 << 10, ///< used prior to checkpoint, skips validate() call on transaction
                skip_validate_invariants = 1 << 11, ///< used to skip database invariant check on block application
                skip_undo_block = 1 << 12, ///< used to skip undo db on reindex
                skip_block_log = 1 << 13,  ///< used to skip block logging on reindex
                skip_apply_transaction = 1 << 14, ///< used to skip apply transaction
                skip_database_locking = 1 << 15 ///< used to skip locking of database
            };

            enum store_metadata_modes {
                store_metadata_for_all,
                store_metadata_for_listed,
                store_metadata_for_nobody
            };

            /**
             * @brief Open a database, creating a new one if necessary
             *
             * Opens a database in the specified directory. If no initialized database is found the database
             * will be initialized with the default state.
             *
             * @param data_dir Path to open or create database in
             */
            void open(const fc::path &data_dir, const fc::path &shared_mem_dir, uint64_t initial_supply = STEEMIT_INIT_SUPPLY, uint64_t shared_file_size = 0, uint32_t chainbase_flags = 0);

            /**
             * @brief Rebuild object graph from block history and open detabase
             *
             * This method may be called after or instead of @ref database::open, and will rebuild the object graph by
             * replaying blockchain history. When this method exits successfully, the database will be open.
             */
            void reindex(const fc::path &data_dir, const fc::path &shared_mem_dir, uint32_t from_block_num, uint64_t shared_file_size = (
                    1024l * 1024l * 1024l * 8l), bool validate_during_replay = false);

            void set_min_free_shared_memory_size(size_t);
            void set_inc_shared_memory_size(size_t);
            void set_block_num_check_free_size(uint32_t);
            void check_free_memory(bool skip_print, uint32_t current_block_num);

            void set_skip_virtual_ops();

            void set_init_block_log(bool init_block_log);

            void set_store_account_metadata(store_metadata_modes store_account_metadata);
            void set_accounts_to_store_metadata(const std::vector<std::string>& accounts_to_store_metadata);
            bool store_metadata_for_account(const std::string& name) const;

            void set_store_asset_metadata(bool store_asset_metadata);
            bool store_asset_metadata() const;

            void set_store_memo_in_savings_withdraws(bool store_memo_in_savings_withdraws);
            bool store_memo_in_savings_withdraws() const;

            void set_store_evaluator_events(bool store_evaluator_events);
            bool store_evaluator_events() const;

            void set_store_comment_extras(bool store_comment_extras);
            bool store_comment_extras() const;

            void set_clear_old_worker_votes(bool clear_old_worker_votes);

            void set_clear_comment_bills(bool clear_comment_bills);

            /**
             * @brief wipe Delete database from disk, and potentially the raw chain as well.
             * @param include_blocks If true, delete the raw chain as well as the database.
             *
             * Will close the database before wiping. Database will be closed when this function returns.
             */
            void wipe(const fc::path &data_dir, const fc::path &shared_mem_dir, bool include_blocks);

            void close(bool rewind = true);

            //////////////////// db_block.cpp ////////////////////

            /**
             *  @return true if the block is in our fork DB or saved to disk as
             *  part of the official chain, otherwise return false
             */
            bool is_known_block(const block_id_type &id) const;

            bool is_known_transaction(const transaction_id_type &id) const;

            fc::sha256 get_pow_target() const;

            uint32_t get_pow_summary_target() const;

            block_id_type get_block_id_for_num( uint32_t block_num )const;

            block_id_type find_block_id_for_num(uint32_t block_num) const;

            optional<signed_block> fetch_block_by_id(const block_id_type &id) const;

            optional<signed_block> fetch_block_by_number(uint32_t num) const;

            const signed_transaction get_recent_transaction(const transaction_id_type &trx_id) const;

            std::vector<block_id_type> get_block_ids_on_fork(block_id_type head_of_fork) const;

            chain_id_type get_chain_id() const;

            void throw_if_exists_limit_order(const account_name_type& account, uint32_t id) const;

            void throw_if_exists_account(const account_name_type& account) const;

            const witness_object &get_witness(const account_name_type& name) const;

            const witness_object *find_witness(const account_name_type& name) const;

            const account_object &get_account(const account_name_type& name) const;

            const account_object *find_account(const account_name_type& name) const;

            share_type get_account_reputation(const account_name_type& name) const;

            // 0 = not blocked, 1 = blocked, 2 = do not bother
            int is_blocking(const account_name_type& account, const account_name_type& blocking) const;
            void check_no_blocking(const account_name_type& account, const account_name_type& blocking, bool can_bypass = true);

            const proposal_object& get_proposal(const account_name_type&, const std::string&) const;
            const proposal_object* find_proposal(const account_name_type&, const std::string&) const;
            void        throw_if_exists_proposal(const account_name_type&, const std::string&) const;

            const worker_request_object& get_worker_request(const comment_id_type& post) const;
            const worker_request_object* find_worker_request(const comment_id_type& post) const;

            const comment_bill_object* find_comment_bill(const comment_id_type& id) const;
            comment_bill get_comment_bill(const comment_id_type& id) const;
            const comment_bill_object* upsert_comment_bill(const comment_id_type& id);

            const hashlink_type make_hashlink_shstr(const shared_string& permlink) const;

            const hashlink_type make_hashlink(const std::string& permlink) const;

            const comment_object &get_comment(const account_name_type& author, hashlink_type hashlink) const;

            const comment_object *find_comment(const account_name_type& author, hashlink_type hashlink) const;

            const comment_object &get_comment_by_perm(const account_name_type &author, const shared_string &permlink) const;

            const comment_object *find_comment_by_perm(const account_name_type &author, const shared_string &permlink) const;

            const comment_object &get_comment_by_perm(const account_name_type &author, const string &permlink) const;

            const comment_object *find_comment_by_perm(const account_name_type &author, const string &permlink) const;

            const comment_object &get_comment(const comment_id_type &comment) const;

            const comment_extras_object* find_extras(const account_name_type& author, hashlink_type hashlink) const;

            const comment_extras_object& get_extras(const account_name_type& author, hashlink_type hashlink) const;

            const escrow_object &get_escrow(const account_name_type &name, uint32_t escrow_id) const;

            const escrow_object *find_escrow(const account_name_type &name, uint32_t escrow_id) const;

            const limit_order_object &get_limit_order(const account_name_type &owner, uint32_t id) const;

            const limit_order_object *find_limit_order(const account_name_type &owner, uint32_t id) const;

            const convert_request_object &get_convert_request(const account_name_type &owner, uint32_t id) const;

            const convert_request_object *find_convert_request(const account_name_type &owner, uint32_t id) const;

            void throw_if_exists_convert_request(const account_name_type &owner, uint32_t id) const;

            const savings_withdraw_object& get_savings_withdraw(const account_name_type& owner, uint32_t request_id) const;
            const savings_withdraw_object* find_savings_withdraw(const account_name_type& owner, uint32_t request_id) const;
            void                throw_if_exists_savings_withdraw(const account_name_type& owner, uint32_t request_id) const;

            const donate_object& get_donate(const donate_object_id_type& donate_id) const;

            const invite_object& get_invite(const public_key_type& invite_key) const;
            const invite_object* find_invite(const public_key_type& invite_key) const;
            void                 throw_if_exists_invite(const public_key_type& invite_key) const;

            const asset_object&  get_asset(const asset_symbol_type& symbol) const;
            const asset_object*  find_asset(const asset_symbol_type& symbol) const;
            void                 throw_if_exists_asset(const asset_symbol_type& symbol) const;

            const asset_object&  get_asset(const std::string& symbol_name, account_name_type creator = account_name_type()) const;
            const asset_object*  find_asset(const std::string& symbol_name) const;
            void                 throw_if_exists_asset(const std::string& symbol_name) const;

            asset_symbol_type    symbol_from_str(std::string str, bool allow_empty = false) const;

            account_balance_object get_or_default_account_balance(const account_name_type& account, const asset_symbol_type& symbol) const;
            void                   adjust_account_balance(const account_name_type& account, const asset& delta, const asset& delta_tip, asset delta_market = asset());

            const paid_subscription_object&  get_paid_subscription(const account_name_type& author, const paid_subscription_id& id) const;
            const paid_subscription_object*  find_paid_subscription(const account_name_type& author, const paid_subscription_id& id) const;
            void                             throw_if_exists_paid_subscription(const account_name_type& author, const paid_subscription_id& id) const;

            const paid_subscriber_object&  get_paid_subscriber(const account_name_type& subscriber, const account_name_type& author, const paid_subscription_id& id) const;
            const paid_subscriber_object*  find_paid_subscriber(const account_name_type& subscriber, const account_name_type& author, const paid_subscription_id& id) const;
            void                             throw_if_exists_paid_subscriber(const account_name_type& subscriber, const account_name_type& author, const paid_subscription_id& id) const;

            const nft_collection_object&  get_nft_collection(asset_symbol_type name) const;
            const nft_collection_object*  find_nft_collection(asset_symbol_type name) const;
            void                     throw_if_exists_nft_collection(asset_symbol_type name) const;

            const nft_object&  get_nft(asset_symbol_type name, const account_name_type& owner) const;
            const nft_object*  find_nft(asset_symbol_type name, const account_name_type& owner) const;

            const nft_object&  get_nft(uint32_t token_id) const;
            const nft_object*  find_nft(uint32_t token_id) const;

            const nft_order_object& get_nft_order(account_name_type seller, uint32_t order_id) const;
            const nft_order_object* find_nft_order(account_name_type seller, uint32_t order_id) const;
            void               throw_if_exists_nft_order(const account_name_type& seller, uint32_t order_id) const;

            void clear_nft_orders(uint32_t token_id,
                const nft_order_object* proceed_order, const nft_order_object* clear_order,
                uint32_t& sell_count, uint32_t& buy_count, double& market_depth, double& market_asks,
                bool only_buying = false);

            bool check_nft_buying_price(uint32_t token_id, asset price) const;

            bool check_nft_bets(uint32_t token_id, asset price) const;

            void clear_nft_bets(uint32_t token_id, const nft_bet_object* proceed_bet = nullptr);

            void process_nft_bets();

            void update_pair_depth(asset base, asset quote);
            void update_asset_marketed(asset_symbol_type symbol);

            const account_authority_object &get_authority(const account_name_type &name) const;

            const dynamic_global_property_object &get_dynamic_global_properties() const;

            const feed_history_object &get_feed_history() const;

            const witness_schedule_object &get_witness_schedule_object() const;

            const hardfork_property_object &get_hardfork_property_object() const;

            const block_summary_object &get_block_summary(const block_summary_id_type &ref_block_num) const;


            const time_point_sec calculate_discussion_payout_time(const comment_object &comment) const;

            /**
             *  Deducts fee from the account and the share supply
             */
            void pay_fee(const account_object &a, asset fee);

            /**
             * Update an account's bandwidth and returns if the account had the requisite bandwidth for the trx
             */
            bool update_account_bandwidth(const dynamic_global_property_object& props, const account_object &a, uint32_t trx_size, const bandwidth_type type);

            void check_negrep_posting_bandwidth(const account_object& acc,
                const std::string& target_type,
                const std::string& id1, const std::string& id2,
                const std::string& id3, const std::string& id4);

            /**
             *  Calculate the percent of block production slots that were missed in the
             *  past 128 blocks, not including the current block.
             */
            uint32_t witness_participation_rate() const;

            void add_checkpoints(const flat_map<uint32_t, block_id_type> &checkpts);

            const flat_map<uint32_t, block_id_type> get_checkpoints() const {
                return _checkpoints;
            }

            bool before_last_checkpoint() const;

            std::pair<bool, int64_t> chain_status() const;

            uint32_t validate_block(const signed_block &b, uint32_t skip = skip_nothing);

            bool push_block(const signed_block &b, uint32_t skip = skip_nothing);

            void enable_plugins_on_push_transaction(bool);

            void push_transaction(const signed_transaction &trx, uint32_t skip = skip_nothing);

            void _maybe_warn_multiple_production(uint32_t height) const;

            bool _push_block(const signed_block &b, uint32_t skip);

            void _push_transaction(const signed_transaction &trx, uint32_t skip);

            void push_proposal(const proposal_object&);

            void remove(const proposal_object&);

            void clear_expired_proposals();

            signed_block generate_block(
                    const fc::time_point_sec when,
                    const account_name_type &witness_owner,
                    const fc::ecc::private_key &block_signing_private_key,
                    uint32_t skip
            );

            signed_block _generate_block(
                    const fc::time_point_sec when,
                    const account_name_type &witness_owner,
                    const fc::ecc::private_key &block_signing_private_key,
                    uint32_t skip
            );

            void pop_block();

            void clear_pending();

            /**
             *  This method is used to track applied operations during the evaluation of a block, these
             *  operations should include any operation actually included in a transaction as well
             *  as any implied/virtual operations that resulted, such as filling an order.
             *  The applied operations are cleared after post_apply_operation.
             */
            void notify_pre_apply_operation(operation_notification &note);

            void notify_post_apply_operation(const operation_notification &note);

            const void push_virtual_operation(const operation &op, bool force = false); // vops are not needed for low mem. Force will push them on low mem.
            void notify_applied_block(const signed_block &block);

            void notify_on_pending_transaction(const signed_transaction &tx);

            void notify_on_applied_transaction(const signed_transaction &tx);

            bool can_push_events();

            void push_event(const operation &op);

            void process_events();

            void push_order_create_event(const limit_order_object& order);

            void push_order_delete_event(const limit_order_object& order);

            bool claim_for_subscription(const account_object& subscriber, const asset& amount, bool from_tip);

            void pay_for_subscription(const account_object& author, const asset& amount, bool to_tip);

            void push_payment_event(const paid_subscriber_object& psro, const sponsor_payment& payment_type, asset prepaid, asset amount, asset rest, asset to_prepaid);

            void process_paid_subscribers();

            bool is_account_vote(const vote_operation& op) const {
                // Check if it is account vote, not comment vote
                return !op.permlink.size() && has_hardfork(STEEMIT_HARDFORK_0_26__164);
            }

            /**
             *  This signal is emitted for plugins to process every operation after it has been fully applied.
             */
            fc::signal<void(operation_notification &)> pre_apply_operation;
            fc::signal<void(const operation_notification &)> post_apply_operation;

            /**
             *  This signal is emitted after all operations and virtual operation for a
             *  block have been applied but before the get_applied_operations() are cleared.
             *
             *  You may not yield from this callback because the blockchain is holding
             *  the write lock and may be in an "inconstant state" until after it is
             *  released.
             */
            fc::signal<void(const signed_block &)> applied_block;

            /**
             * This signal is emitted any time a new transaction is added to the pending
             * block state.
             */
            fc::signal<void(const signed_transaction &)> on_pending_transaction;

            /**
             * This signal is emitted any time a new transaction has been applied to the
             * chain state.
             */
            fc::signal<void(const signed_transaction &)> on_applied_transaction;

            /**
             * This signal is emitted when required number of votes is reached to transit to CyberWay
             */
            fc::signal<void(const uint32_t, const uint32_t)> transit_to_cyberway;

            /**
             *  Emitted After a block has been applied and committed.  The callback
             *  should not yield and should execute quickly.
             */
            //fc::signal<void(const vector< golos::db2::generic_id >&)> changed_objects;

            /** this signal is emitted any time an object is removed and contains a
             * pointer to the last value of every object that was removed.
             */
            //fc::signal<void(const vector<const object*>&)>  removed_objects;

            //////////////////// db_witness_schedule.cpp ////////////////////

            /**
             * @brief Get the witness scheduled for block production in a slot.
             *
             * slot_num always corresponds to a time in the future.
             *
             * If slot_num == 1, returns the next scheduled witness.
             * If slot_num == 2, returns the next scheduled witness after
             * 1 block gap.
             *
             * Use the get_slot_time() and get_slot_at_time() functions
             * to convert between slot_num and timestamp.
             *
             * Passing slot_num == 0 returns STEEMIT_NULL_WITNESS
             */
            account_name_type get_scheduled_witness(uint32_t slot_num) const;

            /**
             * Get the time at which the given slot occurs.
             *
             * If slot_num == 0, return time_point_sec().
             *
             * If slot_num == N for N > 0, return the Nth next
             * block-interval-aligned time greater than head_block_time().
             */
            fc::time_point_sec get_slot_time(uint32_t slot_num) const;

            /**
             * Get the last slot which occurs AT or BEFORE the given time.
             *
             * The return value is the greatest value N such that
             * get_slot_time( N ) <= when.
             *
             * If no such N exists, return 0.
             */
            uint32_t get_slot_at_time(fc::time_point_sec when) const;

            /** @return the sbd created and deposited to_account, may return STEEM if there is no median feed */
            std::pair<asset, asset> create_sbd(const account_object &to_account, asset steem);

            asset create_vesting(const account_object &to_account, asset steem);

            void check_witness_idleness(bool periodically = true);

            void check_account_idleness();

            void check_claim_idleness();

            void update_witness_windows_sec_to_min();

            void update_witness_schedule();

            void adjust_liquidity_reward(const account_object &owner, const asset &volume, bool is_bid);

            void adjust_balance(const account_object &a, const asset &delta);

            void pay_savings_interest(account_object& acnt);

            void adjust_savings_balance(const account_object &a, const asset &delta);

            void adjust_market_balance(const account_object& a, const asset& delta);

            void adjust_supply(const asset &delta, bool adjust_vesting = false);

            void adjust_rshares2(const comment_object &comment, fc::uint128_t old_rshares2, fc::uint128_t new_rshares2);

            authority update_owner_authority(const account_object &account, const authority &owner_authority);

            asset get_balance(const account_object &a, asset_symbol_type symbol) const;

            asset get_savings_balance(const account_object &a, asset_symbol_type symbol) const;

            asset get_balance(const string &aname, asset_symbol_type symbol) const {
                return get_balance(get_account(aname), symbol);
            }

            /** this updates the votes for witnesses as a result of account voting proxy changing */
            void adjust_proxied_witness_votes(const account_object &a,
                    const std::array<share_type,
                            STEEMIT_MAX_PROXY_RECURSION_DEPTH + 1> &delta,
                    int depth = 0);

            /** this updates the votes for all witnesses as a result of account VESTS changing */
            void adjust_proxied_witness_votes(const account_object &a, share_type delta, int depth = 0);

            /** this is called by `adjust_proxied_witness_votes` when account proxy to self */
            void adjust_witness_votes(const account_object &a, share_type delta);

            /** this updates the vote of a single witness as a result of a vote being added or removed*/
            void adjust_witness_vote(const account_object& a, const witness_object& obj, share_type delta, bool overwrite = false);

            /** clears all vote records for a particular account but does not update the
             * witness vote totals.  Vote totals should be updated first via a call to
             * adjust_proxied_witness_votes( a, -a.witness_vote_weight() )
             */
            void clear_witness_votes(const account_object &a);

            void freeze_account(const account_object &account, const account_object &receiver);
            void liberate_golos_classic();
            void set_gc_authority(const account_object &account);
            void clear_authority(const account_object &account);
            void terminate_vesting_activities(const account_object &account);
            void transfer_vestings(const account_object &account, const account_object &receiver);
            void transfer_golos(const account_object &account, const account_object &receiver);
            void transfer_gbg(const account_object &account, const account_object &receiver);
            void replace_recovery(const account_object &old_recovery, const account_object &new_recovery);

            void process_vesting_withdrawals();

            uint64_t pay_delegators(const account_object& delegatee, const comment_vote_object& cvo, uint64_t claim);

            share_type pay_curators(const comment_curation_info& c, share_type max_rewards, share_type& actual_rewards);

            void cashout_comment_helper(const comment_object &comment);

            void process_comment_cashout();

            void clear_worker_request_votes(const worker_request_object& wro);

            void close_worker_request(const worker_request_object& wro, worker_request_state closed_state);

            void send_worker_state(const comment_object& post, worker_request_state closed_state);

            void process_worker_votes();

            void process_worker_cashout();

            void process_funds();

            std::pair<asset, asset> get_min_gp_to_emission() const;

            std::pair<asset, asset> get_min_gp_for_groups() const;

            void process_accumulative_distributions();

            void auto_claim_accumulatives();

            void process_conversions();

            void process_sbd_debt_conversions();

            void process_savings_withdraws();

            void account_recovery_processing();

            void expire_escrow_ratification();

            void process_decline_voting_rights();

            void fix_recovery_accounts();

            void process_account_freezing();

            void process_gbg_payments();

            void update_median_feed();

            share_type claim_rshare_reward(share_type rshares, uint16_t reward_weight, asset max_steem);

            asset get_liquidity_reward() const;
            asset get_content_reward() const;
            asset get_producer_reward() const;
            asset get_curation_reward() const;
            asset get_pow_reward() const;

            uint128_t get_content_constant_s() const;

            uint128_t calculate_vshares(uint128_t rshares) const;

            void pay_liquidity_reward();

            /**
             * Helper method to return the current sbd value of a given amount of
             * STEEM.  Return 0 SBD if there isn't a current_median_history
             */
            asset to_sbd(const asset &steem) const;

            asset to_steem(const asset &sbd) const;

            time_point_sec head_block_time() const;

            uint32_t head_block_num() const;

            block_id_type head_block_id() const;

            uint32_t last_non_undoable_block_num() const;
            //////////////////// db_init.cpp ////////////////////

            void initialize_evaluators();

            void set_custom_operation_interpreter(const std::string &id, std::shared_ptr<custom_operation_interpreter> registry);

            std::shared_ptr<custom_operation_interpreter> get_custom_json_evaluator(const std::string &id);

            /// Reset the object graph in-memory
            void initialize_indexes();

            void init_schema();

            void init_genesis(uint64_t initial_supply = STEEMIT_INIT_SUPPLY);

            /**
             *  This method validates transactions without adding it to the pending state.
             *  @throw if an error occurs
             *  @return modified skip flags
             */
            uint32_t validate_transaction(const signed_transaction &trx, uint32_t skip = skip_nothing);

            /** when popping a block, the transactions that were removed get cached here so they
             * can be reapplied at the proper time */
            std::deque<signed_transaction> _popped_tx;


            bool apply_order(const limit_order_object &new_order_object);

            bool fill_order(const limit_order_object& order, const asset& pays, asset receives);

            void cancel_order(const limit_order_object &obj);

            int match(const limit_order_object &bid, const limit_order_object &ask, const price &trade_price);

            void perform_vesting_share_split(uint32_t magnitude);

            void retally_comment_children();

            void retally_witness_votes();

            void retally_witness_votes_hf22();

            void retally_witness_vote_counts(bool force = false);

            void retally_liquidity_weight();

            void update_virtual_supply();

            bool has_hardfork(uint32_t hardfork) const;

            /* For testing and debugging only. Given a hardfork
               with id N, applies all hardforks with id <= N */
            void set_hardfork(uint32_t hardfork, bool process_now = true);

            void validate_invariants() const;

            /**
             * @}
             */

            const std::string &get_json_schema() const;

            void set_flush_interval(uint32_t flush_blocks);

#ifdef STEEMIT_BUILD_TESTNET
            bool liquidity_rewards_enabled = true;
            bool skip_price_feed_limit_check = true;
            bool skip_transaction_delta_check = true;
#endif

            const block_log &get_block_log() const;
        protected:
            //Mark pop_undo() as protected -- we do not want outside calling pop_undo(); it should call pop_block() instead
            //void pop_undo() { object_database::pop_undo(); }
            void notify_changed_objects();

        private:
            optional<chainbase::database::session> _pending_tx_session;

            void apply_block(const signed_block &next_block, uint32_t skip = skip_nothing);

            void apply_transaction(const signed_transaction &trx, uint32_t skip = skip_nothing);

            void _validate_block(const signed_block& next_block, uint32_t skip);

            void _apply_block(const signed_block &next_block, uint32_t skip);

            void _apply_transaction(const signed_transaction &trx, uint32_t skip);

            void _validate_transaction(const signed_transaction& trx, uint32_t skip);

            void apply_operation(const operation &op, bool is_virtual = false);


            ///Steps involved in applying a new block
            ///@{

            const witness_object &validate_block_header(uint32_t skip, const signed_block &next_block) const;

            void create_block_summary(const signed_block &next_block);

            void update_witness_schedule4();

            void process_transit_to_cyberway(const signed_block& b, uint32_t skip);

            void update_median_witness_props();

            void clear_null_account_balance();

            void update_global_dynamic_data(const signed_block &b, uint32_t skip);

            void update_signing_witness(const witness_object &signing_witness, const signed_block &new_block);

            void update_last_irreversible_block(uint32_t skip);

            void clear_expired_transactions();
            void clear_expired_orders();
            void clear_expired_delegations();

            void process_header_extensions(const signed_block &next_block);

            void reset_virtual_schedule_time();

            void init_hardforks();

            void process_hardforks();

            void apply_hardfork(uint32_t hardfork);

            bool _resize(uint32_t block_num);

            uint64_t pay_curator(const comment_vote_object& cvo, const uint64_t& claim, const comment_curation_info& c, share_type& back_to_fund);

            void adjust_sbd_balance(const account_object &a, const asset &delta);

            ///@}

            std::unique_ptr<database_impl> _my;

            vector<signed_transaction> _pending_tx;
            fork_database _fork_db;
            fc::time_point_sec _hardfork_times[STEEMIT_NUM_HARDFORKS + 1];
            protocol::hardfork_version _hardfork_versions[STEEMIT_NUM_HARDFORKS + 1];

            block_log _block_log;

            // this function needs access to _plugin_index_signal
            template<typename MultiIndexType>
            friend void add_plugin_index(database &db);

            friend struct database_fixture;

            fc::signal<void()> _plugin_index_signal;

            transaction_id_type _current_trx_id;
            uint32_t _current_block_num = 0;
            uint16_t _current_trx_in_block = 0;
            uint16_t _current_op_in_trx = 0;
            uint32_t _current_virtual_op = 0;

            flat_map<uint32_t, block_id_type> _checkpoints;

            uint32_t _flush_blocks = 0;
            uint32_t _next_flush_block = 0;

            uint32_t _last_free_gb_printed = 0;

            size_t _inc_shared_memory_size = 0;
            size_t _min_free_shared_memory_size = 0;

            uint32_t _block_num_check_free_memory = 1000;

            uint32_t _clear_votes_block = 0;
            bool _skip_virtual_ops = false;
            bool _enable_plugins_on_push_transaction = true;

            bool _init_block_log = true;

            store_metadata_modes _store_account_metadata = store_metadata_for_all;
            std::vector<std::string> _accounts_to_store_metadata;

            bool _store_asset_metadata = true;

            bool _store_memo_in_savings_withdraws = true;

            bool _store_evaluator_events = true;

            bool _store_comment_extras = true;

            bool _clear_old_worker_votes = false;

            bool _clear_comment_bills = false;

            asset _accumulative_remainder = asset(0, STEEM_SYMBOL);

            flat_map<std::string, std::shared_ptr<custom_operation_interpreter>> _custom_operation_interpreters;
            std::string _json_schema;
        };

} } // golos::chain
