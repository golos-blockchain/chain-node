#pragma once

#include <golos/protocol/base.hpp>
#include <golos/protocol/block_header.hpp>
#include <golos/protocol/asset.hpp>
#include <golos/protocol/reward_curve.hpp>

#include <fc/utf8.hpp>
#include <fc/crypto/equihash.hpp>

namespace golos { namespace protocol {

        void validate_account_name(const std::string &name);

        void validate_permlink(const std::string &permlink);

        struct account_create_operation : public base_operation {
            asset fee;
            account_name_type creator;
            account_name_type new_account_name;
            authority owner;
            authority active;
            authority posting;
            public_key_type memo_key;
            string json_metadata;

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(creator);
            }
        };

        struct account_referral_options {
            account_name_type referrer;
            uint16_t interest_rate;
            time_point_sec end_date;
            asset break_fee;

            void validate() const;
        };

        using account_create_with_delegation_extension = static_variant<
            account_referral_options
        >;

        using account_create_with_delegation_extensions_type = flat_set<account_create_with_delegation_extension>;

        struct account_create_with_delegation_operation: public base_operation {
            asset fee;
            asset delegation;
            account_name_type creator;
            account_name_type new_account_name;
            authority owner;
            authority active;
            authority posting;
            public_key_type memo_key;
            string json_metadata;

            account_create_with_delegation_extensions_type extensions;

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(creator);
            }
        };

        struct account_create_with_invite_operation: public base_operation {
            string invite_secret;
            account_name_type creator;
            account_name_type new_account_name;
            authority owner;
            authority active;
            authority posting;
            public_key_type memo_key;
            string json_metadata;

            extensions_type extensions;

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(creator);
            }
        };


        struct account_update_operation : public base_operation {
            account_name_type account;
            optional<authority> owner;
            optional<authority> active;
            optional<authority> posting;
            public_key_type memo_key;
            string json_metadata;

            void validate() const;

            void get_required_owner_authorities(flat_set<account_name_type> &a) const {
                if (owner) {
                    a.insert(account);
                }
            }

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                if (!owner) {
                    a.insert(account);
                }
            }
        };

        struct account_metadata_operation : public base_operation {
            account_name_type account;
            string json_metadata;

            void validate() const;
            void get_required_posting_authorities(flat_set<account_name_type>& a) const {
                a.insert(account);
            }
        };


        struct comment_operation : public base_operation {
            account_name_type parent_author;
            string parent_permlink;

            account_name_type author;
            string permlink;

            string title;
            string body;
            string json_metadata;

            void validate() const;

            void get_required_posting_authorities(flat_set<account_name_type> &a) const {
                a.insert(author);
            }
        };

        struct beneficiary_route_type {
            beneficiary_route_type() {
            }

            beneficiary_route_type(const account_name_type &a, const uint16_t &w)
                    : account(a), weight(w) {
            }

            account_name_type account;
            uint16_t weight;

            // For use by std::sort such that the route is sorted first by name (ascending)
            bool operator<(const beneficiary_route_type &o) const {
                return string_less()(account, o.account);
            }
        };

        struct comment_payout_beneficiaries {
            vector<beneficiary_route_type> beneficiaries;

            void validate() const;
        };

        // destination of returning tokens from auction window
        enum auction_window_reward_destination_type {
            to_reward_fund,
            to_curators,
            to_author
        };

        struct comment_auction_window_reward_destination {
            comment_auction_window_reward_destination() {
            }

            comment_auction_window_reward_destination(auction_window_reward_destination_type dest) 
                : destination(dest) {
            }

            auction_window_reward_destination_type destination;

            void validate() const;
        };

        struct comment_curation_rewards_percent {
            comment_curation_rewards_percent() {
            }

            comment_curation_rewards_percent(uint16_t perc)
                : percent(perc) {
            }

            uint16_t percent = STEEMIT_DEF_CURATION_PERCENT;

            void validate() const;
        };

        struct comment_decrypt_fee {
            comment_decrypt_fee() {
            }

            comment_decrypt_fee(asset f)
                : fee(f) {
            }

            asset fee{0, STEEM_SYMBOL};

            void validate() const;
        };

        typedef static_variant <
            comment_payout_beneficiaries,
            comment_auction_window_reward_destination,
            comment_curation_rewards_percent,
            comment_decrypt_fee
        > comment_options_extension;

        typedef flat_set <comment_options_extension> comment_options_extensions_type;


        /**
         *  Authors of posts may not want all of the benefits that come from creating a post. This
         *  operation allows authors to update properties associated with their post.
         *
         *  The max_accepted_payout may be decreased, but never increased.
         *  The percent_steem_dollars may be decreased, but never increased
         *
         */
        struct comment_options_operation : public base_operation {
            account_name_type author;
            string permlink;

            asset max_accepted_payout = asset(1000000000, SBD_SYMBOL);       /// SBD value of the maximum payout this post will receive
            uint16_t percent_steem_dollars = STEEMIT_100_PERCENT; /// the percent of Golos Dollars to key, unkept amounts will be received as Golos Power
            bool allow_votes = true;      /// allows a post to receive votes;
            bool allow_curation_rewards = true; /// allows voters to recieve curation rewards. Rewards return to reward fund.
            comment_options_extensions_type extensions;

            void validate() const;

            void get_required_posting_authorities(flat_set<account_name_type> &a) const {
                a.insert(author);
            }
        };


        struct challenge_authority_operation : public base_operation {
            account_name_type challenger;
            account_name_type challenged;
            bool require_owner = false;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(challenger);
            }
        };

        struct prove_authority_operation : public base_operation {
            account_name_type challenged;
            bool require_owner = false;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                if (!require_owner) {
                    a.insert(challenged);
                }
            }

            void get_required_owner_authorities(flat_set<account_name_type>& a) const {
                if (require_owner) {
                    a.insert(challenged);
                }
            }
        };


        struct delete_comment_operation : public base_operation {
            account_name_type author;
            string permlink;

            void validate() const;

            void get_required_posting_authorities(flat_set<account_name_type> &a) const {
                a.insert(author);
            }
        };


        struct vote_operation : public base_operation {
            account_name_type voter;
            account_name_type author;
            string permlink;
            int16_t weight = 0;

            void validate() const;

            void get_required_posting_authorities(flat_set<account_name_type> &a) const {
                a.insert(voter);
            }
        };


        /**
         * @ingroup operations
         *
         * @brief Transfers STEEM from one account to another.
         */
        struct transfer_operation : public base_operation {
            account_name_type from;
            /// Account to transfer asset to
            account_name_type to;
            /// The amount of asset to transfer from @ref from to @ref to
            asset amount;

            /// The memo is plain-text, any encryption on the memo is up to
            /// a higher level protocol.
            string memo;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                if (amount.symbol != VESTS_SYMBOL) {
                    a.insert(from);
                }
            }

            void get_required_owner_authorities(flat_set<account_name_type> &a) const {
                if (amount.symbol == VESTS_SYMBOL) {
                    a.insert(from);
                }
            }
        };


        /**
         *  The purpose of this operation is to enable someone to send money contingently to
         *  another individual. The funds leave the *from* account and go into a temporary balance
         *  where they are held until *from* releases it to *to* or *to* refunds it to *from*.
         *
         *  In the event of a dispute the *agent* can divide the funds between the to/from account.
         *  Disputes can be raised any time before or on the dispute deadline time, after the escrow
         *  has been approved by all parties.
         *
         *  This operation only creates a proposed escrow transfer. Both the *agent* and *to* must
         *  agree to the terms of the arrangement by approving the escrow.
         *
         *  The escrow agent is paid the fee on approval of all parties. It is up to the escrow agent
         *  to determine the fee.
         *
         *  Escrow transactions are uniquely identified by 'from' and 'escrow_id', the 'escrow_id' is defined
         *  by the sender.
         */
        struct escrow_transfer_operation : public base_operation {
            account_name_type from;
            account_name_type to;
            account_name_type agent;
            uint32_t escrow_id = 30;

            asset sbd_amount = asset(0, SBD_SYMBOL);
            asset steem_amount = asset(0, STEEM_SYMBOL);
            asset fee;

            time_point_sec ratification_deadline;
            time_point_sec escrow_expiration;

            string json_meta;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(from);
            }
        };


        /**
         *  The agent and to accounts must approve an escrow transaction for it to be valid on
         *  the blockchain. Once a part approves the escrow, the cannot revoke their approval.
         *  Subsequent escrow approve operations, regardless of the approval, will be rejected.
         */
        struct escrow_approve_operation : public base_operation {
            account_name_type from;
            account_name_type to;
            account_name_type agent;
            account_name_type who; // Either to or agent

            uint32_t escrow_id = 30;
            bool approve = true;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(who);
            }
        };


        /**
         *  If either the sender or receiver of an escrow payment has an issue, they can
         *  raise it for dispute. Once a payment is in dispute, the agent has authority over
         *  who gets what.
         */
        struct escrow_dispute_operation : public base_operation {
            account_name_type from;
            account_name_type to;
            account_name_type agent;
            account_name_type who;

            uint32_t escrow_id = 30;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(who);
            }
        };


        /**
         *  This operation can be used by anyone associated with the escrow transfer to
         *  release funds if they have permission.
         *
         *  The permission scheme is as follows:
         *  If there is no dispute and escrow has not expired, either party can release funds to the other.
         *  If escrow expires and there is no dispute, either party can release funds to either party.
         *  If there is a dispute regardless of expiration, the agent can release funds to either party
         *     following whichever agreement was in place between the parties.
         */
        struct escrow_release_operation : public base_operation {
            account_name_type from;
            account_name_type to; ///< the original 'to'
            account_name_type agent;
            account_name_type who; ///< the account that is attempting to release the funds, determines valid 'receiver'
            account_name_type receiver; ///< the account that should receive funds (might be from, might be to)

            uint32_t escrow_id = 30;
            asset sbd_amount = asset(0, SBD_SYMBOL); ///< the amount of sbd to release
            asset steem_amount = asset(0, STEEM_SYMBOL); ///< the amount of steem to release

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(who);
            }
        };


        /**
         *  This operation converts STEEM into VFS (Vesting Fund Shares) at
         *  the current exchange rate. With this operation it is possible to
         *  give another account vesting shares so that faucets can
         *  pre-fund new accounts with vesting shares.
         */
        struct transfer_to_vesting_operation : public base_operation {
            account_name_type from;
            account_name_type to; ///< if null, then same as from
            asset amount; ///< must be STEEM

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(from);
            }
        };


        /**
         * At any given point in time an account can be withdrawing from their
         * vesting shares. A user may change the number of shares they wish to
         * cash out at any time between 0 and their total vesting stake.
         *
         * After applying this operation, vesting_shares will be withdrawn
         * at a rate of vesting_shares/104 per week for two years starting
         * one week after this operation is included in the blockchain.
         *
         * This operation is not valid if the user has no vesting shares.
         */
        struct withdraw_vesting_operation : public base_operation {
            account_name_type account;
            asset vesting_shares;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(account);
            }
        };


        /**
         * Allows an account to setup a vesting withdraw but with the additional
         * request for the funds to be transferred directly to another account's
         * balance rather than the withdrawing account. In addition, those funds
         * can be immediately vested again, circumventing the conversion from
         * vests to steem and back, guaranteeing they maintain their value.
         */
        struct set_withdraw_vesting_route_operation : public base_operation {
            account_name_type from_account;
            account_name_type to_account;
            uint16_t percent = 0;
            bool auto_vest = false;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(from_account);
            }
        };

        struct chain_properties_18;
        struct chain_properties_19;

        /**
         * Witnesses must vote on how to set certain chain properties to ensure a smooth
         * and well functioning network. Any time @owner is in the active set of witnesses these
         * properties will be used to control the blockchain configuration.
         */
        struct chain_properties_17 {
            /**
             *  This fee, paid in STEEM, is converted into VESTING SHARES for the new account. Accounts
             *  without vesting shares cannot earn usage rations and therefore are powerless. This minimum
             *  fee requires all accounts to have some kind of commitment to the network that includes the
             *  ability to vote and make transactions.
             */
            asset account_creation_fee =
                    asset(STEEMIT_MIN_ACCOUNT_CREATION_FEE, STEEM_SYMBOL);

            /**
             *  This witnesses vote for the maximum_block_size which is used by the network
             *  to tune rate limiting and capacity
             */
            uint32_t maximum_block_size = STEEMIT_MIN_BLOCK_SIZE_LIMIT * 2;
            uint16_t sbd_interest_rate = STEEMIT_DEFAULT_SBD_INTEREST_RATE;

            void validate() const;

            chain_properties_17& operator=(const chain_properties_17&) = default;

            chain_properties_17& operator=(const chain_properties_18& src);
        };

        /**
         * Witnesses must vote on how to set certain chain properties to ensure a smooth
         * and well functioning network. Any time @owner is in the active set of witnesses these
         * properties will be used to control the blockchain configuration.
         */
        struct chain_properties_18: public chain_properties_17 {

            /**
             *  Minimum fee (in GOLOS) payed when create account with delegation
             */
            asset create_account_min_golos_fee =
                asset(STEEMIT_MIN_ACCOUNT_CREATION_FEE * GOLOS_CREATE_ACCOUNT_WITH_GOLOS_MODIFIER, STEEM_SYMBOL);

            /**
             *  Minimum GP delegation amount when create account with delegation
             *
             *  Note: this minimum is applied only when fee is minimal. If fee is greater,
             *  then actual delegation can be less (up to 0 if fee part is greater or equal than account_creation_fee)
             */
            asset create_account_min_delegation =
                asset(STEEMIT_MIN_ACCOUNT_CREATION_FEE *
                    GOLOS_CREATE_ACCOUNT_WITH_GOLOS_MODIFIER * GOLOS_CREATE_ACCOUNT_DELEGATION_RATIO, STEEM_SYMBOL);

            /**
             * Minimum time of delegated GP on create account (in seconds)
             */
            uint32_t create_account_delegation_time = (GOLOS_CREATE_ACCOUNT_DELEGATION_TIME).to_seconds();

            /**
             * Minimum delegated GP
             */
            asset min_delegation =
                asset(STEEMIT_MIN_ACCOUNT_CREATION_FEE * GOLOS_MIN_DELEGATION_MULTIPLIER, STEEM_SYMBOL);


            void validate() const;

            chain_properties_18& operator=(const chain_properties_17& src) {
                account_creation_fee = src.account_creation_fee;
                maximum_block_size = src.maximum_block_size;
                sbd_interest_rate = src.sbd_interest_rate;
                return *this;
            }

            chain_properties_18& operator=(const chain_properties_18&) = default;
        };

        /**
         * Users can invite referrals, and they will pay some percent of rewards to their referrers.
         * Referral can break paying for some fee.
         */
        struct chain_properties_19: public chain_properties_18 {

            /**
             * Maximum percent of referral deductions
             */
            uint16_t max_referral_interest_rate = GOLOS_DEFAULT_REFERRAL_INTEREST_RATE;

            /**
             * Maximum term of referral deductions
             */
            uint32_t max_referral_term_sec = GOLOS_DEFAULT_REFERRAL_TERM_SEC;

            /**
             * Min fee for breaking referral deductions by referral
             */
            asset min_referral_break_fee = GOLOS_MIN_REFERRAL_BREAK_FEE;

            /**
             * Max fee for breaking referral deductions by referral
             */
            asset max_referral_break_fee = GOLOS_MAX_REFERRAL_BREAK_FEE_PRE_HF22;

            /**
             * Time window for commenting by account
             */
            uint16_t posts_window = STEEMIT_POSTS_WINDOW;

            /**
             * Maximum count of comments per one window by account
             */
            uint16_t posts_per_window = STEEMIT_POSTS_PER_WINDOW;

            /**
             * Time window for commenting by account
             */
            uint16_t comments_window = STEEMIT_COMMENTS_WINDOW;

            /**
             * Maximum count of comments per one window by account
             */
            uint16_t comments_per_window = STEEMIT_COMMENTS_PER_WINDOW;

            /**
             * Time window for voting by account
             */
            uint16_t votes_window = STEEMIT_VOTES_WINDOW;

            /**
             * Maximum count of votes per one window by account
             */
            uint16_t votes_per_window = STEEMIT_VOTES_PER_WINDOW;

            void hf26_windows_sec_to_min() {
                posts_window = std::max(posts_window / 60, 1);
                comments_window = std::max(comments_window / 60, 1);
                votes_window = std::max(votes_window / 60, 1);
            }

            /**
             * Auction window size
             */
            uint16_t auction_window_size = STEEMIT_REVERSE_AUCTION_WINDOW_SECONDS;

            /**
             * Maximum interest rate for delegated vesting shares
             */
            uint16_t max_delegated_vesting_interest_rate = STEEMIT_DEFAULT_DELEGATED_VESTING_INTEREST_RATE;

            /**
             * Custom operations bandwidth multiplier
             */
            uint16_t custom_ops_bandwidth_multiplier = STEEMIT_CUSTOM_OPS_BANDWIDTH_MULTIPLIER;

            /**
             * Minimum rate of all curation rewards in total payment
             */
            uint16_t min_curation_percent = STEEMIT_DEF_CURATION_PERCENT;

            /**
             * Maximum rate of all curation rewards in total payment
             */
            uint16_t max_curation_percent = STEEMIT_DEF_CURATION_PERCENT;

            /**
             * Curation curve
             */
            curation_curve curation_reward_curve = curation_curve::linear;

            /**
             * Allow to return auction window reward to reward fund
             */
            bool allow_return_auction_reward_to_fund = true;

             /**
              * Allow to distribute auction window reward between curators
              */
            bool allow_distribute_auction_reward = true;


            void validate() const;

            chain_properties_19& operator=(const chain_properties_17& src) {
                chain_properties_18::operator=(src);
                return *this;
            }

            chain_properties_19& operator=(const chain_properties_18& src) {
                chain_properties_18::operator=(src);
                return *this;
            }

            chain_properties_19& operator=(const chain_properties_19&) = default;
        };

        struct chain_properties_22 : public chain_properties_19 {

            /**
             * Percent of emission on each block being redirected to worker fund
             */
            uint16_t worker_reward_percent = GOLOS_WORKER_REWARD_PERCENT_PRE_HF26;

            /**
             * Percent of emission on each block being redirected to witness fund
             */
            uint16_t witness_reward_percent = GOLOS_WITNESS_REWARD_PERCENT_PRE_HF26;

            /**
             * Percent of emission on each block being redirected to vesting fund
             */
            uint16_t vesting_reward_percent = GOLOS_VESTING_REWARD_PERCENT_PRE_HF26;

            /**
             * Amount of fee in GBG have to be claimed from worker request author on creating request
             */
            asset worker_request_creation_fee = GOLOS_WORKER_REQUEST_CREATION_FEE;

            /**
             * Minimum percent of total vesting shares have to be voted for request in period to approve payments
             */
            uint16_t worker_request_approve_min_percent = GOLOS_WORKER_REQUEST_APPROVE_MIN_PERCENT;

            /**
             * Percent of each account balance (incl. saving) to be converted on each SBD debt conversion
             */
            uint16_t sbd_debt_convert_rate = STEEMIT_SBD_DEBT_CONVERT_RATE;

            /**
             * The number of votes regenerated per day.  Any user voting slower than this rate will be
             * "wasting" voting power through spillover; any user voting faster than this rate will have
             * their votes reduced.
             */
            uint32_t vote_regeneration_per_day = STEEMIT_VOTE_REGENERATION_PER_DAY;

            /**
             * The minimum time of witness skipping blocks to erase its signing key.
             */
            uint32_t witness_skipping_reset_time = GOLOS_DEF_WITNESS_SKIPPING_RESET_TIME;

            /**
             * The minimum time of witness idleness to periodically clear all its votes.
             */
            uint32_t witness_idleness_time = GOLOS_DEF_WITNESS_IDLENESS_TIME;

            /**
             * The minimum time of account idleness to lower its vesting shares.
             */
            uint32_t account_idleness_time = GOLOS_DEF_ACCOUNT_IDLENESS_TIME;

            void validate() const;

            chain_properties_22& operator=(const chain_properties_17& src) {
                chain_properties_19::operator=(src);
                return *this;
            }

            chain_properties_22& operator=(const chain_properties_18& src) {
                chain_properties_19::operator=(src);
                return *this;
            }

            chain_properties_22& operator=(const chain_properties_19& src) {
                chain_properties_19::operator=(src);
                return *this;
            }

            chain_properties_22& operator=(const chain_properties_22&) = default;
        };

        struct chain_properties_23 : public chain_properties_22 {

            /**
             * The minimum time of claim idleness for accumulative_balance claiming.
             */
            uint32_t claim_idleness_time = GOLOS_DEF_CLAIM_IDLENESS_TIME;

            /**
             * Minimum amount to create invite
             */
            asset min_invite_balance = GOLOS_DEF_MIN_INVITE_BALANCE;

            void validate() const;

            chain_properties_23& operator=(const chain_properties_17& src) {
                chain_properties_22::operator=(src);
                return *this;
            }

            chain_properties_23& operator=(const chain_properties_18& src) {
                chain_properties_22::operator=(src);
                return *this;
            }

            chain_properties_23& operator=(const chain_properties_19& src) {
                chain_properties_22::operator=(src);
                return *this;
            }

            chain_properties_23& operator=(const chain_properties_22& src) {
                chain_properties_22::operator=(src);
                return *this;
            }

            chain_properties_23& operator=(const chain_properties_23&) = default;
        };

        struct chain_properties_24 : public chain_properties_23 {

            /**
             * Amount of fee in GBG have to be claimed on creating asset
             */
            asset asset_creation_fee = GOLOS_DEF_ASSET_CREATION_FEE;

            /**
             * Minimum interval between fund transfers from same invite
             */
            uint32_t invite_transfer_interval_sec = GOLOS_DEF_INVITE_TRANSFER_INTERVAL_SEC;

            void validate() const;

            chain_properties_24& operator=(const chain_properties_17& src) {
                chain_properties_23::operator=(src);
                return *this;
            }

            chain_properties_24& operator=(const chain_properties_18& src) {
                chain_properties_23::operator=(src);
                return *this;
            }

            chain_properties_24& operator=(const chain_properties_19& src) {
                chain_properties_23::operator=(src);
                return *this;
            }

            chain_properties_24& operator=(const chain_properties_22& src) {
                chain_properties_23::operator=(src);
                return *this;
            }

            chain_properties_24& operator=(const chain_properties_23& src) {
                chain_properties_23::operator=(src);
                return *this;
            }

            chain_properties_24& operator=(const chain_properties_24&) = default;
        };

        struct chain_properties_26 : public chain_properties_24 {

            /**
             * Percent of fee on GOLOS-GBG conversions.
             */
            uint16_t convert_fee_percent = GOLOS_DEF_CONVERT_FEE_PERCENT;

            /**
             * Minimum vesting shares amount (in GOLOS) to receive curation rewards.
             */
            asset min_golos_power_to_curate = GOLOS_DEF_GOLOS_POWER_TO_CURATE;

            /**
             * Percent of emission on each block being redirected to worker fund.
             */
            uint16_t worker_emission_percent = GOLOS_WORKER_EMISSION_PERCENT;

            /**
             * Percent of remain (emission - witness - workers), being redirected to vesting.
             * And another part of remain being redirected to author fund.
             */
            uint16_t vesting_of_remain_percent = GOLOS_VESTING_OF_REMAIN_PERCENT;

            /**
             * Posting window (in minutes) for accounts with negative reputation
             */
            uint16_t negrep_posting_window = GOLOS_NEGREP_POSTING_WINDOW;

            /**
             * Posting operations total (comment, vote) available per posting window, per 1 account
             */
            uint16_t negrep_posting_per_window = GOLOS_NEGREP_POSTING_PER_WINDOW;

            void validate() const;

            chain_properties_26& operator=(const chain_properties_17& src) {
                chain_properties_24::operator=(src);
                return *this;
            }

            chain_properties_26& operator=(const chain_properties_18& src) {
                chain_properties_24::operator=(src);
                return *this;
            }

            chain_properties_26& operator=(const chain_properties_19& src) {
                chain_properties_24::operator=(src);
                return *this;
            }

            chain_properties_26& operator=(const chain_properties_22& src) {
                chain_properties_24::operator=(src);
                return *this;
            }

            chain_properties_26& operator=(const chain_properties_23& src) {
                chain_properties_24::operator=(src);
                return *this;
            }

            chain_properties_26& operator=(const chain_properties_24& src) {
                chain_properties_24::operator=(src);
                return *this;
            }

            chain_properties_26& operator=(const chain_properties_26&) = default;
        };

        struct chain_properties_27 : public chain_properties_26 {

            /**
             * Fee on operations by blocked users.
             */
            asset unwanted_operation_cost = GOLOS_DEF_UNWANTED_OPERATION_COST;

            /**
             * Fee on operations by negative-reputation users, and operations outside window limits (post, comment, vote).
             */
            asset unlimit_operation_cost = GOLOS_DEF_UNLIMIT_OPERATION_COST;

            void validate() const;

            chain_properties_27& operator=(const chain_properties_17& src) {
                chain_properties_26::operator=(src);
                return *this;
            }

            chain_properties_27& operator=(const chain_properties_18& src) {
                chain_properties_26::operator=(src);
                return *this;
            }

            chain_properties_27& operator=(const chain_properties_19& src) {
                chain_properties_26::operator=(src);
                return *this;
            }

            chain_properties_27& operator=(const chain_properties_22& src) {
                chain_properties_26::operator=(src);
                return *this;
            }

            chain_properties_27& operator=(const chain_properties_23& src) {
                chain_properties_26::operator=(src);
                return *this;
            }

            chain_properties_27& operator=(const chain_properties_24& src) {
                chain_properties_26::operator=(src);
                return *this;
            }

            chain_properties_27& operator=(const chain_properties_26& src) {
                chain_properties_26::operator=(src);
                return *this;
            }

            chain_properties_27& operator=(const chain_properties_27&) = default;
        };

        struct chain_properties_28 : public chain_properties_27 {

            /**
             * Minimum vesting shares amount (in GBG) to receive emission.
             */
            asset min_golos_power_to_emission = GOLOS_DEF_GOLOS_POWER_TO_EMISSION;

            void validate() const;

            chain_properties_28& operator=(const chain_properties_17& src) {
                chain_properties_27::operator=(src);
                return *this;
            }

            chain_properties_28& operator=(const chain_properties_18& src) {
                chain_properties_27::operator=(src);
                return *this;
            }

            chain_properties_28& operator=(const chain_properties_19& src) {
                chain_properties_27::operator=(src);
                return *this;
            }

            chain_properties_28& operator=(const chain_properties_22& src) {
                chain_properties_27::operator=(src);
                return *this;
            }

            chain_properties_28& operator=(const chain_properties_23& src) {
                chain_properties_27::operator=(src);
                return *this;
            }

            chain_properties_28& operator=(const chain_properties_24& src) {
                chain_properties_27::operator=(src);
                return *this;
            }

            chain_properties_28& operator=(const chain_properties_26& src) {
                chain_properties_27::operator=(src);
                return *this;
            }

            chain_properties_28& operator=(const chain_properties_27& src) {
                chain_properties_27::operator=(src);
                return *this;
            }

            chain_properties_28& operator=(const chain_properties_28&) = default;
        };

        struct chain_properties_29 : public chain_properties_28 {

            /**
             * NFT issue cost (in GBG).
             */
            asset nft_issue_cost = GOLOS_DEF_NFT_ISSUE_COST;

            void validate() const;

            chain_properties_29& operator=(const chain_properties_17& src) {
                chain_properties_28::operator=(src);
                return *this;
            }

            chain_properties_29& operator=(const chain_properties_18& src) {
                chain_properties_28::operator=(src);
                return *this;
            }

            chain_properties_29& operator=(const chain_properties_19& src) {
                chain_properties_28::operator=(src);
                return *this;
            }

            chain_properties_29& operator=(const chain_properties_22& src) {
                chain_properties_28::operator=(src);
                return *this;
            }

            chain_properties_29& operator=(const chain_properties_23& src) {
                chain_properties_28::operator=(src);
                return *this;
            }

            chain_properties_29& operator=(const chain_properties_24& src) {
                chain_properties_28::operator=(src);
                return *this;
            }

            chain_properties_29& operator=(const chain_properties_26& src) {
                chain_properties_28::operator=(src);
                return *this;
            }

            chain_properties_29& operator=(const chain_properties_27& src) {
                chain_properties_28::operator=(src);
                return *this;
            }

            chain_properties_29& operator=(const chain_properties_28& src) {
                chain_properties_28::operator=(src);
                return *this;
            }

            chain_properties_29& operator=(const chain_properties_29&) = default;
        };

        struct chain_properties_30 : public chain_properties_29 {
            /**
             * Minimal golos power for messaging in private groups.
             */
            asset private_group_golos_power = GOLOS_DEF_PRIVATE_GROUP_GOLOS_POWER;

            /**
             * Private group creation - cost (in GBG).
             */
            asset private_group_cost = GOLOS_DEF_PRIVATE_GROUP_COST;

            void validate() const;

            chain_properties_30& operator=(const chain_properties_17& src) {
                chain_properties_29::operator=(src);
                return *this;
            }

            chain_properties_30& operator=(const chain_properties_18& src) {
                chain_properties_29::operator=(src);
                return *this;
            }

            chain_properties_30& operator=(const chain_properties_19& src) {
                chain_properties_29::operator=(src);
                return *this;
            }

            chain_properties_30& operator=(const chain_properties_22& src) {
                chain_properties_29::operator=(src);
                return *this;
            }

            chain_properties_30& operator=(const chain_properties_23& src) {
                chain_properties_29::operator=(src);
                return *this;
            }

            chain_properties_30& operator=(const chain_properties_24& src) {
                chain_properties_29::operator=(src);
                return *this;
            }

            chain_properties_30& operator=(const chain_properties_26& src) {
                chain_properties_29::operator=(src);
                return *this;
            }

            chain_properties_30& operator=(const chain_properties_27& src) {
                chain_properties_29::operator=(src);
                return *this;
            }

            chain_properties_30& operator=(const chain_properties_28& src) {
                chain_properties_29::operator=(src);
                return *this;
            }

            chain_properties_30& operator=(const chain_properties_29& src) {
                chain_properties_29::operator=(src);
                return *this;
            }

            chain_properties_30& operator=(const chain_properties_30&) = default;
        };

        inline chain_properties_17& chain_properties_17::operator=(const chain_properties_18& src) {
            account_creation_fee = src.account_creation_fee;
            maximum_block_size = src.maximum_block_size;
            sbd_interest_rate = src.sbd_interest_rate;
            return *this;
        }

        using versioned_chain_properties = fc::static_variant<
            chain_properties_17,
            chain_properties_18,
            chain_properties_19,
            chain_properties_22,
            chain_properties_23,
            chain_properties_24,
            chain_properties_26,
            chain_properties_27,
            chain_properties_28,
            chain_properties_29,
            chain_properties_30
        >;

        /**
         *  Users who wish to become a witness must pay a fee acceptable to
         *  the current witnesses to apply for the position and allow voting
         *  to begin.
         *
         *  If the owner isn't a witness they will become a witness. Witnesses
         *  are charged a fee equal to 1 weeks worth of witness pay which in
         *  turn is derived from the current share supply. The fee is
         *  only applied if the owner is not already a witness.
         *
         *  If the block_signing_key is null then the witness is removed from
         *  contention. The network will pick the top 21 witnesses for
         *  producing blocks.
         */
        struct witness_update_operation : public base_operation {
            account_name_type owner;
            string url;
            public_key_type block_signing_key;
            chain_properties_17 props;
            asset fee; ///< the fee paid to register a new witness, should be 10x current block production pay

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(owner);
            }
        };

        /**
         *  Wintesses can change some dynamic votable params to control the blockchain configuration
         */
        struct chain_properties_update_operation : public base_operation {
            account_name_type owner;
            versioned_chain_properties props;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(owner);
            }
        };


        /**
         * All accounts with a VFS can vote for or against any witness.
         *
         * If a proxy is specified then all existing votes are removed.
         */
        struct account_witness_vote_operation : public base_operation {
            account_name_type account;
            account_name_type witness;
            bool approve = true;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(account);
            }
        };


        struct account_witness_proxy_operation : public base_operation {
            account_name_type account;
            account_name_type proxy;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(account);
            }
        };


        /**
         * @brief provides a generic way to add higher level protocols on top of witness consensus
         * @ingroup operations
         *
         * There is no validation for this operation other than that required auths are valid
         */
        struct custom_operation : public base_operation {
            flat_set<account_name_type> required_auths;
            uint16_t id = 0;
            vector<char> data;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                for (const auto &i : required_auths) {
                    a.insert(i);
                }
            }
        };


        /** serves the same purpose as custom_operation but also supports required posting authorities. Unlike custom_operation,
         * this operation is designed to be human readable/developer friendly.
         **/
        struct custom_json_operation : public base_operation {
            flat_set<account_name_type> required_auths;
            flat_set<account_name_type> required_posting_auths;
            string id; ///< must be less than 32 characters long
            string json; ///< must be proper utf8 / JSON string.

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                for (const auto &i : required_auths) {
                    a.insert(i);
                }
            }

            void get_required_posting_authorities(flat_set<account_name_type> &a) const {
                for (const auto &i : required_posting_auths) {
                    a.insert(i);
                }
            }
        };


        struct custom_binary_operation : public base_operation {
            flat_set<account_name_type> required_owner_auths;
            flat_set<account_name_type> required_active_auths;
            flat_set<account_name_type> required_posting_auths;
            vector<authority> required_auths;

            string id; ///< must be less than 32 characters long
            vector<char> data;

            void validate() const;

            void get_required_owner_authorities(flat_set<account_name_type> &a) const {
                for (const auto &i : required_owner_auths) {
                    a.insert(i);
                }
            }

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                for (const auto &i : required_active_auths) {
                    a.insert(i);
                }
            }

            void get_required_posting_authorities(flat_set<account_name_type> &a) const {
                for (const auto &i : required_posting_auths) {
                    a.insert(i);
                }
            }

            void get_required_authorities(vector<authority> &a) const {
                for (const auto &i : required_auths) {
                    a.push_back(i);
                }
            }
        };


        /**
         *  Feeds can only be published by the top N witnesses which are included in every round and are
         *  used to define the exchange rate between steem and the dollar.
         */
        struct feed_publish_operation : public base_operation {
            account_name_type publisher;
            price exchange_rate;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(publisher);
            }
        };


        /**
         *  This operation instructs the blockchain to start a conversion between STEEM and SBD,
         *  The funds are deposited after STEEMIT_CONVERSION_DELAY
         */
        struct convert_operation : public base_operation {
            account_name_type owner;
            uint32_t requestid = 0;
            asset amount;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(owner);
            }
        };


        /**
         * This operation creates a limit order and matches it against existing open orders.
         */
        struct limit_order_create_operation : public base_operation {
            account_name_type owner;
            uint32_t orderid = 0; /// an ID assigned by owner, must be unique
            asset amount_to_sell;
            asset min_to_receive;
            bool fill_or_kill = false;
            time_point_sec expiration = time_point_sec::maximum();

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(owner);
            }

            price get_price() const {
                return amount_to_sell / min_to_receive;
            }

            pair<asset_symbol_type, asset_symbol_type> get_market() const {
                return amount_to_sell.symbol < min_to_receive.symbol ?
                       std::make_pair(amount_to_sell.symbol, min_to_receive.symbol)
                                                                     :
                       std::make_pair(min_to_receive.symbol, amount_to_sell.symbol);
            }
        };


        /**
         *  This operation is identical to limit_order_create except it serializes the price rather
         *  than calculating it from other fields.
         */
        struct limit_order_create2_operation : public base_operation {
            account_name_type owner;
            uint32_t orderid = 0; /// an ID assigned by owner, must be unique
            asset amount_to_sell;
            bool fill_or_kill = false;
            price exchange_rate;
            time_point_sec expiration = time_point_sec::maximum();

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(owner);
            }

            price get_price() const {
                return exchange_rate;
            }

            pair<asset_symbol_type, asset_symbol_type> get_market() const {
                return exchange_rate.base.symbol < exchange_rate.quote.symbol ?
                       std::make_pair(exchange_rate.base.symbol, exchange_rate.quote.symbol)
                                                                              :
                       std::make_pair(exchange_rate.quote.symbol, exchange_rate.base.symbol);
            }
        };


        /**
         *  Cancels an order and returns the balance to owner.
         */
        struct limit_order_cancel_operation : public base_operation {
            account_name_type owner;
            uint32_t orderid = 0;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(owner);
            }
        };


        struct pair_to_cancel {
            std::string base;
            std::string quote;
            bool reverse;
        };

        using limit_order_cancel_extension = static_variant<
            pair_to_cancel
        >;

        using limit_order_cancel_extensions_type = flat_set<limit_order_cancel_extension>;

        /**
         *  limit_order_cancel with extensions
         */
        struct limit_order_cancel_ex_operation : public base_operation {
            account_name_type owner;
            uint32_t orderid = 0;

            limit_order_cancel_extensions_type extensions;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(owner);
            }
        };


        struct pow {
            public_key_type worker;
            digest_type input;
            signature_type signature;
            digest_type work;

            void create(const fc::ecc::private_key &w, const digest_type &i);

            void validate() const;
        };


        struct pow_operation : public base_operation {
            account_name_type worker_account;
            block_id_type block_id;
            uint64_t nonce = 0;
            pow work;
            chain_properties_17 props;

            void validate() const;

            fc::sha256 work_input() const;

            const account_name_type &get_worker_account() const {
                return worker_account;
            }

            /** there is no need to verify authority, the proof of work is sufficient */
            void get_required_active_authorities(flat_set<account_name_type> &a) const {
            }
        };


        struct pow2_input {
            account_name_type worker_account;
            block_id_type prev_block;
            uint64_t nonce = 0;
        };


        struct pow2 {
            pow2_input input;
            uint32_t pow_summary = 0;

            void create(const block_id_type &prev_block, const account_name_type &account_name, uint64_t nonce);

            void validate() const;
        };

        struct equihash_pow {
            pow2_input input;
            fc::equihash::proof proof;
            block_id_type prev_block;
            uint32_t pow_summary = 0;

            void create(const block_id_type &recent_block, const account_name_type &account_name, uint32_t nonce);

            void validate() const;
        };

        typedef fc::static_variant<pow2, equihash_pow> pow2_work;

        struct pow2_operation : public base_operation {
            pow2_work work;
            optional<public_key_type> new_owner_key;
            chain_properties_17 props;

            void validate() const;

            void get_required_active_authorities(flat_set<account_name_type> &a) const;

            void get_required_authorities(vector<authority> &a) const {
                if (new_owner_key) {
                    a.push_back(authority(1, *new_owner_key, 1));
                }
            }
        };


        /**
         * This operation is used to report a miner who signs two blocks
         * at the same time. To be valid, the violation must be reported within
         * STEEMIT_MAX_WITNESSES blocks of the head block (1 round) and the
         * producer must be in the ACTIVE witness set.
         *
         * Users not in the ACTIVE witness set should not have to worry about their
         * key getting compromised and being used to produced multiple blocks so
         * the attacker can report it and steel their vesting steem.
         *
         * The result of the operation is to transfer the full VESTING STEEM balance
         * of the block producer to the reporter.
         */
        struct report_over_production_operation : public base_operation {
            account_name_type reporter;
            signed_block_header first_block;
            signed_block_header second_block;

            void validate() const;
        };


        /**
         * All account recovery requests come from a listed recovery account. This
         * is secure based on the assumption that only a trusted account should be
         * a recovery account. It is the responsibility of the recovery account to
         * verify the identity of the account holder of the account to recover by
         * whichever means they have agreed upon. The blockchain assumes identity
         * has been verified when this operation is broadcast.
         *
         * This operation creates an account recovery request which the account to
         * recover has 24 hours to respond to before the request expires and is
         * invalidated.
         *
         * There can only be one active recovery request per account at any one time.
         * Pushing this operation for an account to recover when it already has
         * an active request will either update the request to a new new owner authority
         * and extend the request expiration to 24 hours from the current head block
         * time or it will delete the request. To cancel a request, simply set the
         * weight threshold of the new owner authority to 0, making it an open authority.
         *
         * Additionally, the new owner authority must be satisfiable. In other words,
         * the sum of the key weights must be greater than or equal to the weight
         * threshold.
         *
         * This operation only needs to be signed by the the recovery account.
         * The account to recover confirms its identity to the blockchain in
         * the recover account operation.
         */
        struct request_account_recovery_operation : public base_operation {
            account_name_type recovery_account;       ///< The recovery account is listed as the recovery account on the account to recover.

            account_name_type account_to_recover;     ///< The account to recover. This is likely due to a compromised owner authority.

            authority new_owner_authority;    ///< The new owner authority the account to recover wishes to have. This is secret
            ///< known by the account to recover and will be confirmed in a recover_account_operation

            extensions_type extensions;             ///< Extensions. Not currently used.

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(recovery_account);
            }

            void validate() const;
        };


        /**
         * Recover an account to a new authority using a previous authority and verification
         * of the recovery account as proof of identity. This operation can only succeed
         * if there was a recovery request sent by the account's recover account.
         *
         * In order to recover the account, the account holder must provide proof
         * of past ownership and proof of identity to the recovery account. Being able
         * to satisfy an owner authority that was used in the past 30 days is sufficient
         * to prove past ownership. The get_owner_history function in the database API
         * returns past owner authorities that are valid for account recovery.
         *
         * Proving identity is an off chain contract between the account holder and
         * the recovery account. The recovery request contains a new authority which
         * must be satisfied by the account holder to regain control. The actual process
         * of verifying authority may become complicated, but that is an application
         * level concern, not a blockchain concern.
         *
         * This operation requires both the past and future owner authorities in the
         * operation because neither of them can be derived from the current chain state.
         * The operation must be signed by keys that satisfy both the new owner authority
         * and the recent owner authority. Failing either fails the operation entirely.
         *
         * If a recovery request was made inadvertantly, the account holder should
         * contact the recovery account to have the request deleted.
         *
         * The two setp combination of the account recovery request and recover is
         * safe because the recovery account never has access to secrets of the account
         * to recover. They simply act as an on chain endorsement of off chain identity.
         * In other systems, a fork would be required to enforce such off chain state.
         * Additionally, an account cannot be permanently recovered to the wrong account.
         * While any owner authority from the past 30 days can be used, including a compromised
         * authority, the account can be continually recovered until the recovery account
         * is confident a combination of uncompromised authorities were used to
         * recover the account. The actual process of verifying authority may become
         * complicated, but that is an application level concern, not the blockchain's
         * concern.
         */
        struct recover_account_operation : public base_operation {
            account_name_type account_to_recover;        ///< The account to be recovered

            authority new_owner_authority;       ///< The new owner authority as specified in the request account recovery operation.

            authority recent_owner_authority;    ///< A previous owner authority that the account holder will use to prove past ownership of the account to be recovered.

            extensions_type extensions;                ///< Extensions. Not currently used.

            void get_required_authorities(vector<authority> &a) const {
                a.push_back(new_owner_authority);
                a.push_back(recent_owner_authority);
            }

            void validate() const;
        };


        /**
         *  This operation allows recovery_accoutn to change account_to_reset's owner authority to
         *  new_owner_authority after 60 days of inactivity.
         */
        struct reset_account_operation : public base_operation {
            account_name_type reset_account;
            account_name_type account_to_reset;
            authority new_owner_authority;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(reset_account);
            }

            void validate() const;
        };

        /**
         * This operation allows 'account' owner to control which account has the power
         * to execute the 'reset_account_operation' after 60 days.
         */
        struct set_reset_account_operation : public base_operation {
            account_name_type account;
            account_name_type current_reset_account;
            account_name_type reset_account;

            void validate() const;

            void get_required_owner_authorities(flat_set<account_name_type> &a) const {
                if (current_reset_account.size()) {
                    a.insert(account);
                }
            }

            void get_required_posting_authorities(flat_set<account_name_type> &a) const {
                if (!current_reset_account.size()) {
                    a.insert(account);
                }
            }
        };


        /**
         * Each account lists another account as their recovery account.
         * The recovery account has the ability to create account_recovery_requests
         * for the account to recover. An account can change their recovery account
         * at any time with a 30 day delay. This delay is to prevent
         * an attacker from changing the recovery account to a malicious account
         * during an attack. These 30 days match the 30 days that an
         * owner authority is valid for recovery purposes.
         *
         * On account creation the recovery account is set either to the creator of
         * the account (The account that pays the creation fee and is a signer on the transaction)
         * or to the empty string if the account was mined. An account with no recovery
         * has the top voted witness as a recovery account, at the time the recover
         * request is created. Note: This does mean the effective recovery account
         * of an account with no listed recovery account can change at any time as
         * witness vote weights. The top voted witness is explicitly the most trusted
         * witness according to stake.
         */
        struct change_recovery_account_operation : public base_operation {
            account_name_type account_to_recover;     ///< The account that would be recovered in case of compromise
            account_name_type new_recovery_account;   ///< The account that creates the recover request
            extensions_type extensions;             ///< Extensions. Not currently used.

            void get_required_owner_authorities(flat_set<account_name_type> &a) const {
                a.insert(account_to_recover);
            }

            void validate() const;
        };


        struct transfer_to_savings_operation : public base_operation {
            account_name_type from;
            account_name_type to;
            asset amount;
            string memo;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(from);
            }

            void validate() const;
        };


        struct transfer_from_savings_operation : public base_operation {
            account_name_type from;
            uint32_t request_id = 0;
            account_name_type to;
            asset amount;
            string memo;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(from);
            }

            void validate() const;
        };


        struct cancel_transfer_from_savings_operation : public base_operation {
            account_name_type from;
            uint32_t request_id = 0;

            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(from);
            }

            void validate() const;
        };


        struct decline_voting_rights_operation : public base_operation {
            account_name_type account;
            bool decline = true;

            void get_required_owner_authorities(flat_set<account_name_type> &a) const {
                a.insert(account);
            }

            void validate() const;
        };

/**
 * Delegate vesting shares from one account to the other. The vesting shares are still owned
 * by the original account, but content voting rights and bandwidth allocation are transferred
 * to the receiving account. This sets the delegation to `vesting_shares`, increasing it or
 * decreasing it as needed. (i.e. a delegation of 0 removes the delegation)
 *
 * When a delegation is removed the shares are placed in limbo for a week to prevent a satoshi
 * of GESTS from voting on the same content twice.
 */
        class delegate_vesting_shares_operation: public base_operation {
        public:
            account_name_type delegator;    ///< The account delegating vesting shares
            account_name_type delegatee;    ///< The account receiving vesting shares
            asset vesting_shares;           ///< The amount of vesting shares delegated

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(delegator);
            }
        };

        class break_free_referral_operation : public base_operation {
        public:
            account_name_type referral;

            extensions_type extensions;             ///< Extensions. Not currently used.

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(referral);
            }
        };

        enum delegator_payout_strategy {
            to_delegator,
            to_delegated_vesting,
            _size
        };

        struct interest_direction {
            bool is_emission;
        };

        using delegate_vesting_shares_extension = static_variant<
            interest_direction
        >;

        using delegate_vesting_shares_extensions_type = flat_set<delegate_vesting_shares_extension>;

        class delegate_vesting_shares_with_interest_operation : public base_operation {
        public:
            account_name_type delegator;    ///< The account delegating vesting shares
            account_name_type delegatee;    ///< The account receiving vesting shares
            asset vesting_shares;           ///< The amount of vesting shares delegated
            uint16_t interest_rate = STEEMIT_DEFAULT_DELEGATED_VESTING_INTEREST_RATE; ///< The interest rate wanted by delegator
            // NOT REFLECTED DUE AN OLD BUG:
            delegator_payout_strategy payout_strategy = to_delegator; ///< The strategy of delegator vesting payouts
            delegate_vesting_shares_extensions_type extensions;     ///< Extensions.

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(delegator);
            }
        };

        class reject_vesting_shares_delegation_operation : public base_operation {
        public:
            account_name_type delegator;    ///< The account delegating vesting shares
            account_name_type delegatee;    ///< The account receiving vesting shares
            extensions_type extensions;     ///< Extensions. Not currently used.

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(delegatee);
            }
        };

        class transit_to_cyberway_operation : public base_operation {
        public:
            account_name_type owner;
            bool vote_to_transit = true;

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type> &a) const {
                a.insert(owner);
            }
        };

        class claim_operation : public base_operation {
        public:
            account_name_type from;
            account_name_type to;
            asset amount;
            bool to_vesting;

            extensions_type extensions;

            void validate() const;
            void get_required_posting_authorities(flat_set<account_name_type>& a) const {
                a.insert(from);
            }
        };

        class donate_memo {
        public:
            account_name_type app;
            uint16_t version;
            fc::variant_object target;
            fc::optional<string> comment;
        };

        class donate_operation : public base_operation {
        public:
            account_name_type from;
            account_name_type to;
            asset amount;
            donate_memo memo;

            extensions_type extensions;

            void validate() const;
            void get_required_posting_authorities(flat_set<account_name_type>& a) const {
                a.insert(from);
            }
        };

        class transfer_to_tip_operation : public base_operation {
        public:
            account_name_type from;
            account_name_type to;
            asset amount;
            string memo;

            extensions_type extensions;

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(from);
            }
        };

        class transfer_from_tip_operation : public base_operation {
        public:
            account_name_type from;
            account_name_type to;
            asset amount;
            string memo;

            extensions_type extensions;

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(from);
            }
        };

        struct is_invite_referral {
            bool is_referral;
        };

        using invite_extension = static_variant<
            is_invite_referral
        >;

        using invite_extensions_type = flat_set<invite_extension>;

        class invite_operation : public base_operation {
        public:
            account_name_type creator;
            asset balance;
            public_key_type invite_key;

            invite_extensions_type extensions;

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(creator);
            }
        };

        class invite_claim_operation : public base_operation {
        public:
            account_name_type initiator;
            account_name_type receiver;
            string invite_secret;

            extensions_type extensions;

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(initiator);
            }
        };

        struct asset_create_operation : public base_operation {
            account_name_type creator;
            asset max_supply; // also stores asset symbol
            bool allow_fee = false;
            bool allow_override_transfer = false;
            string json_metadata;

            extensions_type extensions;

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(creator);
            }
        };

        struct asset_update_operation : public base_operation {
            account_name_type creator;
            string symbol;
            flat_set<string> symbols_whitelist;
            uint16_t fee_percent = 0;
            string json_metadata;

            extensions_type extensions;

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(creator);
            }
        };

        struct asset_issue_operation : public base_operation {
            account_name_type creator;
            asset amount;
            account_name_type to;

            extensions_type extensions;

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(creator);
            }
        };

        struct asset_transfer_operation : public base_operation {
            account_name_type creator;
            string symbol;
            account_name_type new_owner;

            extensions_type extensions;

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(creator);
            }
        };

        struct override_transfer_operation : public base_operation {
            account_name_type creator;
            account_name_type from;
            account_name_type to;
            asset amount;

            /// The memo is plain-text, any encryption on the memo is up to
            /// a higher level protocol.
            string memo;

            extensions_type extensions;

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(creator);
            }
        };

        class invite_donate_operation : public base_operation {
        public:
            account_name_type from;
            public_key_type invite_key;
            asset amount;

            /// The memo is plain-text, any encryption on the memo is up to
            /// a higher level protocol.
            string memo;

            extensions_type extensions;

            void validate() const;
            void get_required_active_authorities(flat_set<account_name_type>& a) const {
                a.insert(from);
            }
        };

        class invite_transfer_operation : public base_operation {
        public:
            public_key_type from;
            public_key_type to;
            asset amount;

            /// The memo is plain-text, any encryption on the memo is up to
            /// a higher level protocol.
            string memo;

            extensions_type extensions;

            void validate() const;
            void get_required_authorities(vector<authority> &a) const {
                a.push_back(authority(1, from, 1));
            }
        };

        struct account_block_setting {
            account_name_type account;
            bool block;

            void validate() const;
        };

        struct do_not_bother_setting {
            bool do_not_bother;

            void validate() const;
        };

        using account_setting = static_variant<
            account_block_setting,
            do_not_bother_setting
        >;

        using account_settings_type = flat_set<account_setting>;

        struct account_setup_operation: public base_operation {
            account_name_type account;
            account_settings_type settings;

            extensions_type extensions;

            void validate() const;
            void get_required_posting_authorities(flat_set<account_name_type>& a) const {
                a.insert(account);
            }
        };

} } // golos::protocol


FC_REFLECT((golos::protocol::transfer_to_savings_operation), (from)(to)(amount)(memo))
FC_REFLECT((golos::protocol::transfer_from_savings_operation), (from)(request_id)(to)(amount)(memo))
FC_REFLECT((golos::protocol::cancel_transfer_from_savings_operation), (from)(request_id))

FC_REFLECT((golos::protocol::reset_account_operation), (reset_account)(account_to_reset)(new_owner_authority))
FC_REFLECT((golos::protocol::set_reset_account_operation), (account)(current_reset_account)(reset_account))


FC_REFLECT((golos::protocol::report_over_production_operation), (reporter)(first_block)(second_block))
FC_REFLECT((golos::protocol::convert_operation), (owner)(requestid)(amount))
FC_REFLECT((golos::protocol::feed_publish_operation), (publisher)(exchange_rate))
FC_REFLECT((golos::protocol::pow), (worker)(input)(signature)(work))
FC_REFLECT((golos::protocol::pow2), (input)(pow_summary))
FC_REFLECT((golos::protocol::pow2_input), (worker_account)(prev_block)(nonce))
FC_REFLECT((golos::protocol::equihash_pow), (input)(proof)(prev_block)(pow_summary))

FC_REFLECT(
    (golos::protocol::chain_properties_17),
    (account_creation_fee)(maximum_block_size)(sbd_interest_rate))
FC_REFLECT_DERIVED(
    (golos::protocol::chain_properties_18), ((golos::protocol::chain_properties_17)),
    (create_account_min_golos_fee)(create_account_min_delegation)
    (create_account_delegation_time)(min_delegation))
FC_REFLECT_DERIVED(
    (golos::protocol::chain_properties_19), ((golos::protocol::chain_properties_18)),
    (max_referral_interest_rate)(max_referral_term_sec)(min_referral_break_fee)(max_referral_break_fee)
    (posts_window)(posts_per_window)(comments_window)(comments_per_window)(votes_window)(votes_per_window)(auction_window_size)
    (max_delegated_vesting_interest_rate)(custom_ops_bandwidth_multiplier)(min_curation_percent)(max_curation_percent)
    (curation_reward_curve)(allow_distribute_auction_reward)(allow_return_auction_reward_to_fund))
FC_REFLECT_DERIVED(
    (golos::protocol::chain_properties_22), ((golos::protocol::chain_properties_19)),
    (worker_reward_percent)(witness_reward_percent)(vesting_reward_percent)
    (worker_request_creation_fee)(worker_request_approve_min_percent)
    (sbd_debt_convert_rate)(vote_regeneration_per_day)
    (witness_skipping_reset_time)(witness_idleness_time)(account_idleness_time))
FC_REFLECT_DERIVED(
    (golos::protocol::chain_properties_23), ((golos::protocol::chain_properties_22)),
    (claim_idleness_time)(min_invite_balance))
FC_REFLECT_DERIVED(
    (golos::protocol::chain_properties_24), ((golos::protocol::chain_properties_23)),
    (asset_creation_fee)(invite_transfer_interval_sec))
FC_REFLECT_DERIVED(
    (golos::protocol::chain_properties_26), ((golos::protocol::chain_properties_24)),
    (convert_fee_percent)(min_golos_power_to_curate)
    (worker_emission_percent)(vesting_of_remain_percent)
    (negrep_posting_window)(negrep_posting_per_window)
)
FC_REFLECT_DERIVED(
    (golos::protocol::chain_properties_27), ((golos::protocol::chain_properties_26)),
    (unwanted_operation_cost)(unlimit_operation_cost)
)
FC_REFLECT_DERIVED(
    (golos::protocol::chain_properties_28), ((golos::protocol::chain_properties_27)),
    (min_golos_power_to_emission)
)
FC_REFLECT_DERIVED(
    (golos::protocol::chain_properties_29), ((golos::protocol::chain_properties_28)),
    (nft_issue_cost)
)
FC_REFLECT_DERIVED(
    (golos::protocol::chain_properties_30), ((golos::protocol::chain_properties_29)),
    (private_group_golos_power)(private_group_cost)
)

FC_REFLECT_TYPENAME((golos::protocol::versioned_chain_properties))

FC_REFLECT_TYPENAME((golos::protocol::pow2_work))
FC_REFLECT((golos::protocol::pow_operation), (worker_account)(block_id)(nonce)(work)(props))
FC_REFLECT((golos::protocol::pow2_operation), (work)(new_owner_key)(props))

FC_REFLECT((golos::protocol::account_create_operation),
        (fee)
                (creator)
                (new_account_name)
                (owner)
                (active)
                (posting)
                (memo_key)
                (json_metadata))

FC_REFLECT((golos::protocol::account_referral_options), (referrer)(interest_rate)(end_date)(break_fee))
FC_REFLECT_TYPENAME((golos::protocol::account_create_with_delegation_extension));
FC_REFLECT((golos::protocol::account_create_with_delegation_operation),
    (fee)(delegation)(creator)(new_account_name)(owner)(active)(posting)(memo_key)(json_metadata)(extensions));

FC_REFLECT((golos::protocol::account_create_with_invite_operation),
    (invite_secret)(creator)(new_account_name)(owner)(active)(posting)(memo_key)(json_metadata)(extensions));

FC_REFLECT((golos::protocol::account_update_operation),
        (account)
                (owner)
                (active)
                (posting)
                (memo_key)
                (json_metadata))

FC_REFLECT((golos::protocol::account_metadata_operation), (account)(json_metadata))

FC_REFLECT((golos::protocol::transfer_operation), (from)(to)(amount)(memo))
FC_REFLECT((golos::protocol::transfer_to_vesting_operation), (from)(to)(amount))
FC_REFLECT((golos::protocol::withdraw_vesting_operation), (account)(vesting_shares))
FC_REFLECT((golos::protocol::set_withdraw_vesting_route_operation), (from_account)(to_account)(percent)(auto_vest))
FC_REFLECT((golos::protocol::witness_update_operation), (owner)(url)(block_signing_key)(props)(fee))
FC_REFLECT((golos::protocol::account_witness_vote_operation), (account)(witness)(approve))
FC_REFLECT((golos::protocol::account_witness_proxy_operation), (account)(proxy))
FC_REFLECT((golos::protocol::comment_operation), (parent_author)(parent_permlink)(author)(permlink)(title)(body)(json_metadata))
FC_REFLECT((golos::protocol::vote_operation), (voter)(author)(permlink)(weight))
FC_REFLECT((golos::protocol::custom_operation), (required_auths)(id)(data))
FC_REFLECT((golos::protocol::custom_json_operation), (required_auths)(required_posting_auths)(id)(json))
FC_REFLECT((golos::protocol::custom_binary_operation), (required_owner_auths)(required_active_auths)(required_posting_auths)(required_auths)(id)(data))
FC_REFLECT((golos::protocol::limit_order_create_operation), (owner)(orderid)(amount_to_sell)(min_to_receive)(fill_or_kill)(expiration))
FC_REFLECT((golos::protocol::limit_order_create2_operation), (owner)(orderid)(amount_to_sell)(exchange_rate)(fill_or_kill)(expiration))
FC_REFLECT((golos::protocol::limit_order_cancel_operation), (owner)(orderid))
FC_REFLECT((golos::protocol::pair_to_cancel), (base)(quote)(reverse));
FC_REFLECT_TYPENAME((golos::protocol::limit_order_cancel_extension));
FC_REFLECT((golos::protocol::limit_order_cancel_ex_operation), (owner)(orderid)(extensions))

FC_REFLECT((golos::protocol::delete_comment_operation), (author)(permlink));

FC_REFLECT((golos::protocol::beneficiary_route_type), (account)(weight))
FC_REFLECT_ENUM(golos::protocol::auction_window_reward_destination_type, (to_reward_fund)(to_curators)(to_author))
FC_REFLECT((golos::protocol::comment_payout_beneficiaries), (beneficiaries));
FC_REFLECT((golos::protocol::comment_auction_window_reward_destination), (destination));
FC_REFLECT((golos::protocol::comment_curation_rewards_percent), (percent));
FC_REFLECT((golos::protocol::comment_decrypt_fee), (fee));
FC_REFLECT_TYPENAME((golos::protocol::comment_options_extension));
FC_REFLECT((golos::protocol::comment_options_operation), (author)(permlink)(max_accepted_payout)(percent_steem_dollars)(allow_votes)(allow_curation_rewards)(extensions))

FC_REFLECT((golos::protocol::escrow_transfer_operation), (from)(to)(sbd_amount)(steem_amount)(escrow_id)(agent)(fee)(json_meta)(ratification_deadline)(escrow_expiration));
FC_REFLECT((golos::protocol::escrow_approve_operation), (from)(to)(agent)(who)(escrow_id)(approve));
FC_REFLECT((golos::protocol::escrow_dispute_operation), (from)(to)(agent)(who)(escrow_id));
FC_REFLECT((golos::protocol::escrow_release_operation), (from)(to)(agent)(who)(receiver)(escrow_id)(sbd_amount)(steem_amount));
FC_REFLECT((golos::protocol::challenge_authority_operation), (challenger)(challenged)(require_owner));
FC_REFLECT((golos::protocol::prove_authority_operation), (challenged)(require_owner));
FC_REFLECT((golos::protocol::request_account_recovery_operation), (recovery_account)(account_to_recover)(new_owner_authority)(extensions));
FC_REFLECT((golos::protocol::recover_account_operation), (account_to_recover)(new_owner_authority)(recent_owner_authority)(extensions));
FC_REFLECT((golos::protocol::change_recovery_account_operation), (account_to_recover)(new_recovery_account)(extensions));
FC_REFLECT((golos::protocol::decline_voting_rights_operation), (account)(decline));
FC_REFLECT((golos::protocol::delegate_vesting_shares_operation), (delegator)(delegatee)(vesting_shares));
FC_REFLECT((golos::protocol::chain_properties_update_operation), (owner)(props));
FC_REFLECT((golos::protocol::break_free_referral_operation), (referral)(extensions));

FC_REFLECT_ENUM(golos::protocol::delegator_payout_strategy, (to_delegator)(to_delegated_vesting)(_size))
FC_REFLECT((golos::protocol::interest_direction),
    (is_emission)
)
FC_REFLECT_TYPENAME((golos::protocol::delegate_vesting_shares_extension));
FC_REFLECT((golos::protocol::delegate_vesting_shares_with_interest_operation), (delegator)(delegatee)(vesting_shares)(interest_rate)(extensions));
FC_REFLECT((golos::protocol::reject_vesting_shares_delegation_operation), (delegator)(delegatee)(extensions));
FC_REFLECT((golos::protocol::transit_to_cyberway_operation), (owner)(vote_to_transit));

FC_REFLECT((golos::protocol::claim_operation), (from)(to)(amount)(to_vesting)(extensions))
FC_REFLECT((golos::protocol::donate_memo), (app)(version)(target)(comment))
FC_REFLECT((golos::protocol::donate_operation), (from)(to)(amount)(memo)(extensions))
FC_REFLECT((golos::protocol::transfer_to_tip_operation), (from)(to)(amount)(memo)(extensions))
FC_REFLECT((golos::protocol::transfer_from_tip_operation), (from)(to)(amount)(memo)(extensions))

FC_REFLECT((golos::protocol::is_invite_referral), (is_referral))
FC_REFLECT_TYPENAME((golos::protocol::invite_extension));
FC_REFLECT((golos::protocol::invite_operation), (creator)(balance)(invite_key)(extensions))
FC_REFLECT((golos::protocol::invite_claim_operation), (initiator)(receiver)(invite_secret)(extensions))

FC_REFLECT((golos::protocol::asset_create_operation), (creator)(max_supply)(allow_fee)(allow_override_transfer)(json_metadata)(extensions))
FC_REFLECT((golos::protocol::asset_issue_operation), (creator)(amount)(to)(extensions))
FC_REFLECT((golos::protocol::asset_update_operation), (creator)(symbol)(symbols_whitelist)(fee_percent)(json_metadata)(extensions))
FC_REFLECT((golos::protocol::asset_transfer_operation), (creator)(symbol)(new_owner)(extensions))
FC_REFLECT((golos::protocol::override_transfer_operation), (creator)(from)(to)(amount)(memo)(extensions))

FC_REFLECT((golos::protocol::invite_donate_operation), (from)(invite_key)(amount)(memo)(extensions))
FC_REFLECT((golos::protocol::invite_transfer_operation), (from)(to)(amount)(memo)(extensions))

FC_REFLECT((golos::protocol::account_block_setting),
    (account)(block)
)
FC_REFLECT((golos::protocol::do_not_bother_setting),
    (do_not_bother)
)
FC_REFLECT_TYPENAME((golos::protocol::account_setting));
FC_REFLECT((golos::protocol::account_setup_operation),
    (account)(settings)(extensions)
);
