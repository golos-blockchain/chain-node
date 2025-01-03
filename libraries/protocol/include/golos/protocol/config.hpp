/*
 * Copyright (c) 2016 Golos, Inc., and contributors.
 */
#pragma once


#define STEEMIT_BLOCKCHAIN_VERSION              (version(0, 30, 0))
#define STEEMIT_BLOCKCHAIN_HARDFORK_VERSION     (hardfork_version(STEEMIT_BLOCKCHAIN_VERSION))


#define GOLOS_BUG1074_BLOCK 23569125

#ifdef STEEMIT_BUILD_TESTNET
#define BLOCKCHAIN_NAME "GOLOSTEST"

#define STEEMIT_SYMBOL                          "GOLOS"
#define STEEMIT_ADDRESS_PREFIX                  "GLS"

#define STEEMIT_INIT_PRIVATE_KEY                (fc::ecc::private_key::regenerate(fc::sha256::hash(BLOCKCHAIN_NAME)))
#define STEEMIT_INIT_PUBLIC_KEY_STR             (std::string(golos::protocol::public_key_type(STEEMIT_INIT_PRIVATE_KEY.get_public_key())))
#define STEEMIT_CHAIN_ID                        (fc::sha256::hash(BLOCKCHAIN_NAME))

#define VESTS_SYMBOL  (uint64_t(6) | (uint64_t('G') << 8) | (uint64_t('E') << 16) | (uint64_t('S') << 24) | (uint64_t('T') << 32) | (uint64_t('S') << 40)) ///< GESTS with 6 digits of precision
#define STEEM_SYMBOL  (uint64_t(3) | (uint64_t('G') << 8) | (uint64_t('O') << 16) | (uint64_t('L') << 24) | (uint64_t('O') << 32) | (uint64_t('S') << 40)) ///< GOLOS with 3 digits of precision
#define SBD_SYMBOL    (uint64_t(3) | (uint64_t('G') << 8) | (uint64_t('B') << 16) | (uint64_t('G') << 24) ) ///< Test Backed Dollars with 3 digits of precision
#define STMD_SYMBOL   (uint64_t(3) | (uint64_t('S') << 8) | (uint64_t('T') << 16) | (uint64_t('M') << 24) | (uint64_t('D') << 32) ) ///< Test Dollars with 3 digits of precision

#define STEEMIT_GENESIS_TIME                    (fc::time_point_sec(1476788400))
#define STEEMIT_MINING_TIME                     (fc::time_point_sec(1451606400))
#define STEEMIT_CASHOUT_WINDOW_SECONDS          (60*60) /// 1 hour
#define STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF12 (STEEMIT_CASHOUT_WINDOW_SECONDS * 2)
#define STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF17 (STEEMIT_CASHOUT_WINDOW_SECONDS * 2)
#define STEEMIT_SECOND_CASHOUT_WINDOW           (60*60*5) /// 5 hours
#define STEEMIT_MAX_CASHOUT_WINDOW_SECONDS      (60*60*2) /// 2 hours
#define STEEMIT_VOTE_CHANGE_LOCKOUT_PERIOD      (60*10) /// 10 minutes

#define STEEMIT_MAX_PROPOSAL_LIFETIME_SEC       (60*60*12) /// 12 hours
#define STEEMIT_MAX_PROPOSAL_DEPTH              (2)

#define STEEMIT_ORIGINAL_MIN_ACCOUNT_CREATION_FEE 0
#define STEEMIT_MIN_ACCOUNT_CREATION_FEE          0

#define STEEMIT_OWNER_AUTH_RECOVERY_PERIOD                  fc::hours(2)
#define STEEMIT_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD  fc::minutes(3)
#define STEEMIT_OWNER_UPDATE_LIMIT                          fc::minutes(2)
#define STEEMIT_OWNER_AUTH_HISTORY_TRACKING_START_BLOCK_NUM 1

#define STEEMIT_BLOCK_INTERVAL                  3
#define STEEMIT_BLOCKS_PER_YEAR                 (365*24*60*60/STEEMIT_BLOCK_INTERVAL)
#define STEEMIT_BLOCKS_PER_DAY                  (24*60*60/STEEMIT_BLOCK_INTERVAL)
#define STEEMIT_START_VESTING_BLOCK             (STEEMIT_BLOCKS_PER_DAY / 512)
#define STEEMIT_START_MINER_VOTING_BLOCK        (60*10/STEEMIT_BLOCK_INTERVAL)
#define STEEMIT_FIRST_CASHOUT_TIME              (fc::time_point_sec(1476788400 + STEEMIT_BLOCK_INTERVAL))

#define STEEMIT_INIT_MINER_NAME                 "cyberfounder"
#define STEEMIT_NUM_INIT_MINERS                 1
#define STEEMIT_INIT_TIME                       (fc::time_point_sec());
#ifndef STEEMIT_MAX_VOTED_WITNESSES
#  define STEEMIT_MAX_VOTED_WITNESSES           1
#endif
#define STEEMIT_MAX_MINER_WITNESSES             1
#define STEEMIT_MAX_RUNNER_WITNESSES            1
#define STEEMIT_MAX_WITNESSES                   (STEEMIT_MAX_VOTED_WITNESSES+STEEMIT_MAX_MINER_WITNESSES+STEEMIT_MAX_RUNNER_WITNESSES) /// 21 is more than enough
#define STEEMIT_TRANSIT_REQUIRED_WITNESSES      1 // 16 from top19 -> This guarantees 75% participation on all subsequent rounds.
#define STEEMIT_HARDFORK_REQUIRED_WITNESSES     1 // 17 of the 20 dpos witnesses (19 elected and 1 virtual time) required for hardfork. This guarantees 75% participation on all subsequent rounds.
#define STEEMIT_MAJOR_VOTED_WITNESSES           (STEEMIT_MAX_VOTED_WITNESSES / 2 + 1)
#define STEEMIT_SUPER_MAJOR_VOTED_WITNESSES     (STEEMIT_MAX_VOTED_WITNESSES * 3 / 4 + 1)
#define STEEMIT_MAX_TIME_UNTIL_EXPIRATION       (60*60) // seconds,  aka: 1 hour
#define STEEMIT_MAX_MEMO_SIZE                   2048
#define STEEMIT_MAX_PROXY_RECURSION_DEPTH       4
#define STEEMIT_VESTING_WITHDRAW_INTERVALS_PRE_HF_16 104
#define STEEMIT_VESTING_WITHDRAW_INTERVALS_PRE_HF_23 13
#define STEEMIT_VESTING_WITHDRAW_INTERVALS_PRE_HF_26 8
#define STEEMIT_VESTING_WITHDRAW_INTERVALS      4
#define STEEMIT_VESTING_WITHDRAW_INTERVAL_SECONDS (60*1) // 1 hour per interval
#define STEEMIT_MAX_WITHDRAW_ROUTES             10
#define STEEMIT_SAVINGS_WITHDRAW_TIME           (fc::days(3))
#define STEEMIT_SAVINGS_WITHDRAW_REQUEST_LIMIT  100
#define STEEMIT_VOTE_REGENERATION_PER_DAY_PRE_HF_22 40
#define STEEMIT_VOTE_REGENERATION_PER_DAY       10
#define STEEMIT_VOTE_REGENERATION_SECONDS       (5*60*60*24) // 5 days
#define STEEMIT_MAX_VOTE_CHANGES                5
#define STEEMIT_UPVOTE_LOCKOUT                  (fc::minutes(1))
#define STEEMIT_REVERSE_AUCTION_WINDOW_SECONDS  (10*60) /// 10 minutes
#define STEEMIT_MAX_AUCTION_WINDOW_SIZE_SECONDS (24*60*60) /// 24 hours
#define STEEMIT_MIN_VOTE_INTERVAL_SEC           3
#define STEEMIT_MAX_COMMENT_BENEFICIARIES       8

#define STEEMIT_MIN_ROOT_COMMENT_INTERVAL       (fc::seconds(60*5)) // 5 minutes
#define STEEMIT_MIN_REPLY_INTERVAL              (fc::seconds(20)) // 20 seconds
#define STEEMIT_POST_AVERAGE_WINDOW             (60*60*24u) // 1 day
#define STEEMIT_POST_MAX_BANDWIDTH              (4*STEEMIT_100_PERCENT) // 2 posts per 1 days, average 1 every 12 hours
#define STEEMIT_POST_WEIGHT_CONSTANT            (uint64_t(STEEMIT_POST_MAX_BANDWIDTH) * STEEMIT_POST_MAX_BANDWIDTH)

#define STEEMIT_COMMENTS_WINDOW                 200    // 200 seconds
#define STEEMIT_COMMENTS_PER_WINDOW             10
#define STEEMIT_VOTES_WINDOW                    200
#define STEEMIT_VOTES_PER_WINDOW                10
#define STEEMIT_POSTS_WINDOW                    60
#define STEEMIT_POSTS_PER_WINDOW                1
#define GOLOS_NEGREP_POSTING_WINDOW             24*60 // 1440 minutes
#define GOLOS_NEGREP_POSTING_PER_WINDOW         3

#define STEEMIT_CUSTOM_OPS_BANDWIDTH_MULTIPLIER 100

#define STEEMIT_MAX_ACCOUNT_WITNESS_VOTES       30

#define STEEMIT_100_PERCENT                     10000
#define STEEMIT_1_PERCENT                       (STEEMIT_100_PERCENT/100)
#define STEEMIT_1_TENTH_PERCENT                 (STEEMIT_100_PERCENT/1000)
#define STEEMIT_DEFAULT_SBD_INTEREST_RATE       (10*STEEMIT_1_PERCENT) ///< 10% APR

#define STEEMIT_INFLATION_RATE_START_PERCENT    (1500) // Fixes block 0 to 15%
#define STEEMIT_INFLATION_RATE_STOP_PERCENT     (95) // 0.95%
#define STEEMIT_INFLATION_NARROWING_PERIOD      (250000) // Narrow 0.01% every 250k blocks

#define GOLOS_WITNESS_EMISSION_PERCENT          (15.00*STEEMIT_1_PERCENT)
#define GOLOS_WORKER_EMISSION_PERCENT           (5.00*STEEMIT_1_PERCENT)
#define GOLOS_MAX_WORKER_EMISSION_PERCENT       (50.00*STEEMIT_1_PERCENT)
#define GOLOS_VESTING_OF_REMAIN_PERCENT         (80.00*STEEMIT_1_PERCENT)

#define GOLOS_WORKER_REQUEST_MIN_DURATION        (0)
#define GOLOS_WORKER_REQUEST_MAX_DURATION        (60*60*24*30) ///< 30 days
#define GOLOS_WORKER_REQUEST_APPROVE_MIN_PERCENT (10.00*STEEMIT_1_PERCENT) ///< 10%
#define GOLOS_WORKER_REQUEST_CREATION_FEE        asset(100000, SBD_SYMBOL) ///< 100.000 GBG
#define GOLOS_WORKER_CASHOUT_INTERVAL        	 (STEEMIT_BLOCKS_PER_HOUR/6) ///< every 10 minutes

#define STEEMIT_MINER_PAY_PERCENT               (STEEMIT_1_PERCENT) // 1%
#define STEEMIT_MIN_RATION                      100000
#define STEEMIT_MAX_RATION_DECAY_RATE           (1000000)
#define STEEMIT_FREE_TRANSACTIONS_WITH_NEW_ACCOUNT 100

#define STEEMIT_BANDWIDTH_AVERAGE_WINDOW_SECONDS (60*60*24*7) ///< 1 week
#define STEEMIT_BANDWIDTH_PRECISION             1000000ll ///< 1 million
#define STEEMIT_MAX_COMMENT_DEPTH_PRE_HF17      5
#define STEEMIT_MAX_COMMENT_DEPTH               0xfff0 // 64k - 16
#define STEEMIT_SOFT_MAX_COMMENT_DEPTH          0xff // 255
#define STEEMIT_MAX_RESERVE_RATIO   (20000)

#define GOLOS_CREATE_ACCOUNT_WITH_GOLOS_MODIFIER    1
#define GOLOS_CREATE_ACCOUNT_DELEGATION_RATIO       5
#define GOLOS_CREATE_ACCOUNT_DELEGATION_TIME        (fc::days(1))
#define GOLOS_MIN_DELEGATION_MULTIPLIER             10

#define STEEMIT_DEFAULT_DELEGATED_VESTING_INTEREST_RATE      (25*STEEMIT_1_PERCENT) // 25%
#define STEEMIT_MAX_DELEGATED_VESTING_INTEREST_RATE          STEEMIT_100_PERCENT
#define STEEMIT_MAX_DELEGATED_VESTING_INTEREST_RATE_PRE_HF22 (80*STEEMIT_1_PERCENT) // 80%

#define GOLOS_DEFAULT_REFERRAL_INTEREST_RATE    (10*STEEMIT_1_PERCENT) // 10%
#define GOLOS_MAX_REFERRAL_INTEREST_RATE        STEEMIT_100_PERCENT
#define GOLOS_DEFAULT_REFERRAL_TERM_SEC         (60*60*24*30*6)
#define GOLOS_MAX_REFERRAL_TERM_SEC             (60*60*24*30*12)
#define GOLOS_MIN_REFERRAL_BREAK_FEE            asset(1, STEEM_SYMBOL)
#define GOLOS_MAX_REFERRAL_BREAK_FEE_PRE_HF22   asset(100000, STEEM_SYMBOL)

#define STEEMIT_MINING_REWARD                   asset(666, STEEM_SYMBOL)
#define STEEMIT_MINING_REWARD_PRE_HF_16         asset(1000, STEEM_SYMBOL)
#define STEEMIT_EQUIHASH_N                      140
#define STEEMIT_EQUIHASH_K                      6

#define STEEMIT_LIQUIDITY_TIMEOUT_SEC           (fc::seconds(60*60*24*7)) // After one week volume is set to 0
#define STEEMIT_MIN_LIQUIDITY_REWARD_PERIOD_SEC (fc::seconds(60)) // 1 minute required on books to receive volume
#define STEEMIT_LIQUIDITY_REWARD_PERIOD_SEC     (60*60)
#define STEEMIT_LIQUIDITY_REWARD_BLOCKS         (STEEMIT_LIQUIDITY_REWARD_PERIOD_SEC/STEEMIT_BLOCK_INTERVAL)
#define STEEMIT_MIN_LIQUIDITY_REWARD            (asset(1000*STEEMIT_LIQUIDITY_REWARD_BLOCKS, STEEM_SYMBOL)) // Minumum reward to be paid out to liquidity providers
#define STEEMIT_MIN_CONTENT_REWARD              asset(1500, STEEM_SYMBOL)
#define STEEMIT_MIN_CURATE_REWARD               asset(500, STEEM_SYMBOL)
#define STEEMIT_MIN_PRODUCER_REWARD             STEEMIT_MINING_REWARD
#define STEEMIT_MIN_PRODUCER_REWARD_PRE_HF_16   STEEMIT_MINING_REWARD_PRE_HF_16
#define STEEMIT_MIN_POW_REWARD                  STEEMIT_MINING_REWARD
#define STEEMIT_MIN_POW_REWARD_PRE_HF_16        STEEMIT_MINING_REWARD_PRE_HF_16

#define STEEMIT_MIN_CURATION_PERCENT            0 // 0%
#define STEEMIT_MIN_CURATION_PERCENT_PRE_HF22   (25*STEEMIT_1_PERCENT) // 25%
#define STEEMIT_DEF_CURATION_PERCENT            (25*STEEMIT_1_PERCENT) // 25%
#define STEEMIT_MAX_CURATION_PERCENT            STEEMIT_100_PERCENT

#define STEEMIT_ACTIVE_CHALLENGE_FEE            asset(2000, STEEM_SYMBOL)
#define STEEMIT_OWNER_CHALLENGE_FEE             asset(30000, STEEM_SYMBOL)
#define STEEMIT_ACTIVE_CHALLENGE_COOLDOWN       fc::days(1)
#define STEEMIT_OWNER_CHALLENGE_COOLDOWN        fc::days(1)

// 5ccc e802 de5f
// int(expm1( log1p( 1 ) / BLOCKS_PER_YEAR ) * 2**STEEMIT_APR_PERCENT_SHIFT_PER_BLOCK / 100000 + 0.5)
// we use 100000 here instead of 10000 because we end up creating an additional 9x for vesting
#define STEEMIT_APR_PERCENT_MULTIPLY_PER_BLOCK          ( (uint64_t( 0x5ccc ) << 0x20) \
                                                        | (uint64_t( 0xe802 ) << 0x10) \
                                                        | (uint64_t( 0xde5f )        ) \
                                                        )
// chosen to be the maximal value such that STEEMIT_APR_PERCENT_MULTIPLY_PER_BLOCK * 2**64 * 100000 < 2**128
#define STEEMIT_APR_PERCENT_SHIFT_PER_BLOCK             87

#define STEEMIT_APR_PERCENT_MULTIPLY_PER_ROUND          ( (uint64_t( 0x79cc ) << 0x20 ) \
                                                        | (uint64_t( 0xf5c7 ) << 0x10 ) \
                                                        | (uint64_t( 0x3480 )         ) \
                                                        )

#define STEEMIT_APR_PERCENT_SHIFT_PER_ROUND             83

// We have different constants for hourly rewards
// i.e. hex(int(math.expm1( math.log1p( 1 ) / HOURS_PER_YEAR ) * 2**STEEMIT_APR_PERCENT_SHIFT_PER_HOUR / 100000 + 0.5))
#define STEEMIT_APR_PERCENT_MULTIPLY_PER_HOUR           ( (uint64_t( 0x6cc1 ) << 0x20) \
                                                        | (uint64_t( 0x39a1 ) << 0x10) \
                                                        | (uint64_t( 0x5cbd )        ) \
                                                        )

// chosen to be the maximal value such that STEEMIT_APR_PERCENT_MULTIPLY_PER_HOUR * 2**64 * 100000 < 2**128
#define STEEMIT_APR_PERCENT_SHIFT_PER_HOUR              77

// These constants add up to GRAPHENE_100_PERCENT.  Each GRAPHENE_1_PERCENT is equivalent to 1% per year APY
// *including the corresponding 9x vesting rewards*
#define STEEMIT_CURATE_APR_PERCENT              1937
#define STEEMIT_CONTENT_APR_PERCENT             5813
#define STEEMIT_LIQUIDITY_APR_PERCENT            750
#define STEEMIT_PRODUCER_APR_PERCENT             750
#define STEEMIT_POW_APR_PERCENT                  750

#define STEEMIT_MIN_PAYOUT_SBD                  (asset(20, SBD_SYMBOL))

#define STEEMIT_SBD_STOP_PERCENT                (5*STEEMIT_1_PERCENT ) // Stop printing SBD at 5% Market Cap
#define STEEMIT_SBD_START_PERCENT               (2*STEEMIT_1_PERCENT) // Start reducing printing of SBD at 2% Market Cap

#define STEEMIT_MIN_ACCOUNT_NAME_LENGTH          3
#define STEEMIT_MAX_ACCOUNT_NAME_LENGTH         16

#define STEEMIT_MIN_PERMLINK_LENGTH             0
#define STEEMIT_MAX_PERMLINK_LENGTH             256
#define STEEMIT_MAX_WITNESS_URL_LENGTH          2048

#define STEEMIT_INIT_SUPPLY                     int64_t(1000000000000)
#define STEEMIT_MAX_SHARE_SUPPLY                int64_t(1000000000000000ll)
#define STEEMIT_MAX_SIG_CHECK_DEPTH             2

#define STEEMIT_MIN_TRANSACTION_SIZE_LIMIT      1024
#define STEEMIT_SECONDS_PER_YEAR                (uint64_t(60*60*24*365ll))

#define STEEMIT_SBD_INTEREST_COMPOUND_INTERVAL_SEC  (60*60)
#define STEEMIT_MAX_TRANSACTION_SIZE            (1024*64)
#define STEEMIT_MIN_BLOCK_SIZE_LIMIT            (STEEMIT_MAX_TRANSACTION_SIZE)
#define STEEMIT_MAX_BLOCK_SIZE                  (STEEMIT_MAX_TRANSACTION_SIZE*STEEMIT_BLOCK_INTERVAL*2000)
#define STEEMIT_BLOCKS_PER_HOUR                 (60*60/STEEMIT_BLOCK_INTERVAL)
#define STEEMIT_FEED_INTERVAL_BLOCKS            (STEEMIT_BLOCKS_PER_HOUR / 60)
#define STEEMIT_FEED_HISTORY_WINDOW_PRE_HF_16   (24*7) /// 7 days * 24 hours per day
#define STEEMIT_FEED_HISTORY_WINDOW             (12*7) // 3.5 days
#define STEEMIT_MAX_FEED_AGE                    (fc::days(1))
#define STEEMIT_MIN_FEEDS                       1 /// protects the network from conversions before price has been established
#define STEEMIT_CONVERSION_DELAY_PRE_HF_16      (fc::days(7))
#define STEEMIT_CONVERSION_DELAY                (fc::seconds(30)) //30 second conversion

#define STEEMIT_SBD_DEBT_CONVERT_THRESHOLD      (20*STEEMIT_1_PERCENT) ///< Start force conversion SBD debt to GOLOS on account balances at 20% Market Cap
#define STEEMIT_SBD_DEBT_CONVERT_RATE           (STEEMIT_1_PERCENT) ///< Convert 1% of account balance (incl. savings) on each SBD debt conversion
#define STEEMIT_SBD_DEBT_CONVERT_INTERVAL       (5) ///< 1 day

#define GOLOS_MIN_WITNESS_SKIPPING_RESET_TIME   (60*60*1) ///< 1 hour
#define GOLOS_DEF_WITNESS_SKIPPING_RESET_TIME   (60*60*6) ///< 6 hours
#define GOLOS_MIN_WITNESS_IDLENESS_TIME         (60*60*24*30*1) ///< 1 month
#define GOLOS_DEF_WITNESS_IDLENESS_TIME         (60*60*24*30*12) ///< 12 month
#define GOLOS_WITNESS_IDLENESS_CHECK_INTERVAL   (STEEMIT_BLOCKS_PER_HOUR*24) ///< 1 day
#define GOLOS_WITNESS_IDLENESS_TOP_COUNT        100 ///< Count of TOP voted witnesses to periodically clear votes if idling too long

#define GOLOS_MIN_ACCOUNT_IDLENESS_TIME         (60*60*24*30*1) ///< 1 month
#define GOLOS_DEF_ACCOUNT_IDLENESS_TIME         (60*60*24*30*12) ///< 12 month
#define GOLOS_ACCOUNT_IDLENESS_CHECK_INTERVAL   (STEEMIT_BLOCKS_PER_HOUR*24) ///< 1 day

#define GOLOS_MIN_CLAIM_IDLENESS_TIME           (60) ///< 60 seconds
#define GOLOS_DEF_CLAIM_IDLENESS_TIME           (60) ///< 60 seconds
#define GOLOS_CLAIM_IDLENESS_CHECK_INTERVAL     (STEEMIT_BLOCK_INTERVAL*(70)) ///< 70 seconds
#define GOLOS_ACCUM_DISTRIBUTION_INTERVAL_HF26  (120) ///< 360 seconds
#define GOLOS_ACCUM_DISTRIBUTION_INTERVAL       (120) ///< 360 seconds
#define GOLOS_ACCUM_DISTRIBUTION_STEP           2 ///< accounts receiving their VS share per block

#define GOLOS_MIN_INVITE_BALANCE                1000
#define GOLOS_DEF_MIN_INVITE_BALANCE            asset(10000, STEEM_SYMBOL)

#define GOLOS_MIN_ASSET_CREATION_FEE            0
#define GOLOS_DEF_ASSET_CREATION_FEE            asset(0, SBD_SYMBOL)
#define GOLOS_MIN_INVITE_TRANSFER_INTERVAL_SEC  0
#define GOLOS_DEF_INVITE_TRANSFER_INTERVAL_SEC  60

#define GOLOS_MIN_CONVERT_FEE_PERCENT           0
#define GOLOS_MAX_CONVERT_FEE_PERCENT           STEEMIT_100_PERCENT
#define GOLOS_DEF_CONVERT_FEE_PERCENT           (5.00*STEEMIT_1_PERCENT)

#define GOLOS_MIN_GOLOS_POWER_TO_CURATE         0
#define GOLOS_DEF_GOLOS_POWER_TO_CURATE         asset(1000000, STEEM_SYMBOL)

#define GOLOS_MIN_GOLOS_POWER_TO_EMISSION       0
#define GOLOS_DEF_GOLOS_POWER_TO_EMISSION       asset(1000000, SBD_SYMBOL)

#define GOLOS_DEF_UNWANTED_OPERATION_COST       asset(100000, STEEM_SYMBOL)
#define GOLOS_DEF_UNLIMIT_OPERATION_COST        asset(10000, STEEM_SYMBOL)

#define GOLOS_MIN_NFT_ISSUE_COST                0
#define GOLOS_DEF_NFT_ISSUE_COST                asset(5000, SBD_SYMBOL)

#define GOLOS_MIN_PRIVATE_GROUP_GOLOS_POWER     0
#define GOLOS_DEF_PRIVATE_GROUP_GOLOS_POWER     asset(1000000, SBD_SYMBOL)
#define GOLOS_MIN_PRIVATE_GROUP_COST            0
#define GOLOS_DEF_PRIVATE_GROUP_COST            asset(0, SBD_SYMBOL)

#define STEEMIT_MIN_UNDO_HISTORY                10
#define STEEMIT_MAX_UNDO_HISTORY                10000

#define STEEMIT_MIN_TRANSACTION_EXPIRATION_LIMIT (STEEMIT_BLOCK_INTERVAL * 5) // 5 transactions per block
#define STEEMIT_BLOCKCHAIN_PRECISION            uint64_t(1000)

#define STEEMIT_BLOCKCHAIN_PRECISION_DIGITS     3
#define STEEMIT_MAX_INSTANCE_ID                 (uint64_t(-1)>>16)
/** NOTE: making this a power of 2 (say 2^15) would greatly accelerate fee calcs */
#define STEEMIT_MAX_AUTHORITY_MEMBERSHIP        10
#define STEEMIT_MAX_ASSET_WHITELIST_AUTHORITIES 10
#define STEEMIT_MAX_URL_LENGTH                  127

#define GRAPHENE_CURRENT_DB_VERSION             "GPH2.4"

#define STEEMIT_IRREVERSIBLE_THRESHOLD          (75 * STEEMIT_1_PERCENT)

#define STEEMIT_OAUTH_ACCOUNT                   "oauth"
#define STEEMIT_REGISTRATOR_ACCOUNT             "newacc"
#define STEEMIT_NOTIFY_ACCOUNT                  "notify"
#define STEEMIT_RECOVERY_ACCOUNT                "recovery"

#else // IS LIVE STEEM NETWORK

#define STEEMIT_INIT_PUBLIC_KEY_STR             "GLS7KVuKX87DK44xmhAD92hqJeR8Acd1TBKCtVnGLC5VDpER5CtWE"
#define BLOCKCHAIN_NAME                         "GOLOS"
#define STEEMIT_CHAIN_ID                        (fc::sha256::hash(BLOCKCHAIN_NAME))

#define VESTS_SYMBOL  (uint64_t(6) | (uint64_t('G') << 8) | (uint64_t('E') << 16) | (uint64_t('S') << 24) | (uint64_t('T') << 32) | (uint64_t('S') << 40)) ///< GESTS with 6 digits of precision
#define STEEM_SYMBOL  (uint64_t(3) | (uint64_t('G') << 8) | (uint64_t('O') << 16) | (uint64_t('L') << 24) | (uint64_t('O') << 32) | (uint64_t('S') << 40)) ///< GOLOS with 3 digits of precision
#define SBD_SYMBOL    (uint64_t(3) | (uint64_t('G') << 8) | (uint64_t('B') << 16) | (uint64_t('G') << 24) ) ///< STEEM Backed Dollars with 3 digits of precision
#define STMD_SYMBOL   (uint64_t(3) | (uint64_t('S') << 8) | (uint64_t('T') << 16) | (uint64_t('M') << 24) | (uint64_t('D') << 32) ) ///< STEEM Dollars with 3 digits of precision
#define STEEMIT_SYMBOL                          "GOLOS"
#define STEEMIT_ADDRESS_PREFIX                  "GLS"

#define STEEMIT_GENESIS_TIME                    (fc::time_point_sec(1476788400))
#define STEEMIT_MINING_TIME                     (fc::time_point_sec(1458838800))
//#ifdef STEEMIT_BUILD_LIVETEST
//#define STEEMIT_CASHOUT_WINDOW_SECONDS          (60*2)  // 2 minutes
//#else
#define STEEMIT_CASHOUT_WINDOW_SECONDS          (60*60*24*7)  // 1 week
//#endif
#define STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF12 (60*60*24)    // 1 day
#define STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF17 STEEMIT_CASHOUT_WINDOW_SECONDS_PRE_HF12
#define STEEMIT_SECOND_CASHOUT_WINDOW           (60*60*24*30) /// 30 days
#define STEEMIT_MAX_CASHOUT_WINDOW_SECONDS      (60*60*24*14) /// 2 weeks
#define STEEMIT_VOTE_CHANGE_LOCKOUT_PERIOD      (60*60*2) /// 2 hours

#define STEEMIT_MAX_PROPOSAL_LIFETIME_SEC       (60*60*24*7*4) /// 4 weeks
#define STEEMIT_MAX_PROPOSAL_DEPTH              (2)

#define STEEMIT_ORIGINAL_MIN_ACCOUNT_CREATION_FEE  100000
#define STEEMIT_MIN_ACCOUNT_CREATION_FEE           1

#ifdef STEEMIT_BUILD_LIVETEST
#define STEEMIT_OWNER_AUTH_RECOVERY_PERIOD                  fc::minutes(10)
#else
#define STEEMIT_OWNER_AUTH_RECOVERY_PERIOD                  fc::days(30)
#endif
#define STEEMIT_ACCOUNT_RECOVERY_REQUEST_EXPIRATION_PERIOD  fc::days(1)
#ifdef STEEMIT_BUILD_LIVETEST
#define STEEMIT_OWNER_UPDATE_LIMIT                          fc::minutes(10)
#else
#define STEEMIT_OWNER_UPDATE_LIMIT                          fc::minutes(60)
#endif
#define STEEMIT_OWNER_AUTH_HISTORY_TRACKING_START_BLOCK_NUM 1

#define STEEMIT_BLOCK_INTERVAL                  3
#define STEEMIT_BLOCKS_PER_YEAR                 (365*24*60*60/STEEMIT_BLOCK_INTERVAL)
#define STEEMIT_BLOCKS_PER_DAY                  (24*60*60/STEEMIT_BLOCK_INTERVAL)
#define STEEMIT_START_VESTING_BLOCK             (STEEMIT_BLOCKS_PER_DAY * 49)
#define STEEMIT_START_MINER_VOTING_BLOCK        (60*10/STEEMIT_BLOCK_INTERVAL)
#define STEEMIT_FIRST_CASHOUT_TIME              (fc::time_point_sec(1484478000))

#define STEEMIT_INIT_MINER_NAME                 "cyberfounder"
#define STEEMIT_NUM_INIT_MINERS                 1
#define STEEMIT_INIT_TIME                       (fc::time_point_sec());
#define STEEMIT_MAX_VOTED_WITNESSES             19
#define STEEMIT_MAX_MINER_WITNESSES             1
#define STEEMIT_MAX_RUNNER_WITNESSES            1
#define STEEMIT_MAX_WITNESSES                   (STEEMIT_MAX_VOTED_WITNESSES+STEEMIT_MAX_MINER_WITNESSES+STEEMIT_MAX_RUNNER_WITNESSES) /// 21 is more than enough
#define STEEMIT_TRANSIT_REQUIRED_WITNESSES      16 // 16 from top19 -> This guarantees 75% participation on all subsequent rounds.
#define STEEMIT_HARDFORK_REQUIRED_WITNESSES     17 // 17 of the 20 dpos witnesses (19 elected and 1 virtual time) required for hardfork. This guarantees 75% participation on all subsequent rounds.
#define STEEMIT_MAJOR_VOTED_WITNESSES           (STEEMIT_MAX_VOTED_WITNESSES / 2 + 1)
#define STEEMIT_SUPER_MAJOR_VOTED_WITNESSES      (STEEMIT_MAX_VOTED_WITNESSES * 3 / 4 + 1)
#define STEEMIT_MAX_TIME_UNTIL_EXPIRATION       (60*60) // seconds,  aka: 1 hour
#define STEEMIT_MAX_MEMO_SIZE                   2048
#define STEEMIT_MAX_PROXY_RECURSION_DEPTH       4
#define STEEMIT_VESTING_WITHDRAW_INTERVALS_PRE_HF_16 104
#define STEEMIT_VESTING_WITHDRAW_INTERVALS_PRE_HF_23 13
#define STEEMIT_VESTING_WITHDRAW_INTERVALS_PRE_HF_26 8
#define STEEMIT_VESTING_WITHDRAW_INTERVALS      4
#define STEEMIT_VESTING_WITHDRAW_INTERVAL_SECONDS (60*60*24*7) // 1 week per interval
#define STEEMIT_MAX_WITHDRAW_ROUTES             10
#define STEEMIT_SAVINGS_WITHDRAW_TIME           (fc::days(3))
#define STEEMIT_SAVINGS_WITHDRAW_REQUEST_LIMIT  100
#define STEEMIT_VOTE_REGENERATION_PER_DAY_PRE_HF_22 40
#define STEEMIT_VOTE_REGENERATION_PER_DAY       10
#define STEEMIT_VOTE_REGENERATION_SECONDS       (5*60*60*24) // 5 days
#define STEEMIT_MAX_VOTE_CHANGES                5
#define STEEMIT_UPVOTE_LOCKOUT                  (fc::minutes(1))
#define STEEMIT_REVERSE_AUCTION_WINDOW_SECONDS  (60*30) /// 30 minutes
#define STEEMIT_MAX_AUCTION_WINDOW_SIZE_SECONDS (3*60*60) /// 3 hours
#define STEEMIT_MIN_VOTE_INTERVAL_SEC           3
#define STEEMIT_MAX_COMMENT_BENEFICIARIES       64

#define STEEMIT_MIN_ROOT_COMMENT_INTERVAL       (fc::seconds(60*5)) // 5 minutes
#define STEEMIT_MIN_REPLY_INTERVAL              (fc::seconds(20)) // 20 seconds
#define STEEMIT_POST_AVERAGE_WINDOW             (60*60*24u) // 1 day
#define STEEMIT_POST_MAX_BANDWIDTH              (4*STEEMIT_100_PERCENT) // 2 posts per 1 days, average 1 every 12 hours
#define STEEMIT_POST_WEIGHT_CONSTANT            (uint64_t(STEEMIT_POST_MAX_BANDWIDTH) * STEEMIT_POST_MAX_BANDWIDTH)

#define STEEMIT_COMMENTS_WINDOW                 200
#define STEEMIT_COMMENTS_PER_WINDOW             10
#define STEEMIT_VOTES_WINDOW                    15
#define STEEMIT_VOTES_PER_WINDOW                5
#define STEEMIT_POSTS_WINDOW                    3
#define STEEMIT_POSTS_PER_WINDOW                1
#define GOLOS_NEGREP_POSTING_WINDOW             24*60 // 1440 minutes
#define GOLOS_NEGREP_POSTING_PER_WINDOW         3

#define STEEMIT_CUSTOM_OPS_BANDWIDTH_MULTIPLIER 100

#define STEEMIT_MAX_ACCOUNT_WITNESS_VOTES       30

#define STEEMIT_100_PERCENT                     10000
#define STEEMIT_1_PERCENT                       (STEEMIT_100_PERCENT/100)
#define STEEMIT_1_TENTH_PERCENT                 (STEEMIT_100_PERCENT/1000)
#define STEEMIT_DEFAULT_SBD_INTEREST_RATE       (10*STEEMIT_1_PERCENT) ///< 10% APR

#define STEEMIT_INFLATION_RATE_START_PERCENT    (1515) // Fixes block 3860400 to 15%
#define STEEMIT_INFLATION_RATE_STOP_PERCENT     (95) // 0.95%
#define STEEMIT_INFLATION_NARROWING_PERIOD      (250000) // Narrow 0.01% every 250k blocks

#define GOLOS_WITNESS_EMISSION_PERCENT          (15.00*STEEMIT_1_PERCENT)
#define GOLOS_WORKER_EMISSION_PERCENT           (1.00*STEEMIT_1_PERCENT)
#define GOLOS_MAX_WORKER_EMISSION_PERCENT       (50.00*STEEMIT_1_PERCENT)
#define GOLOS_VESTING_OF_REMAIN_PERCENT         (80.00*STEEMIT_1_PERCENT)

#define GOLOS_WORKER_REQUEST_MIN_DURATION        (60*60*24*5) ///< 5 days
#define GOLOS_WORKER_REQUEST_MAX_DURATION        (60*60*24*30) ///< 30 days
#define GOLOS_WORKER_REQUEST_APPROVE_MIN_PERCENT (10.00*STEEMIT_1_PERCENT) ///< 10%
#define GOLOS_WORKER_REQUEST_CREATION_FEE        asset(100000, SBD_SYMBOL) ///< 100.000 GBG
#define GOLOS_WORKER_CASHOUT_INTERVAL        	 (STEEMIT_BLOCKS_PER_HOUR/6) ///< every 10 minutes

#define STEEMIT_MINER_PAY_PERCENT               (STEEMIT_1_PERCENT) // 1%
#define STEEMIT_MIN_RATION                      100000
#define STEEMIT_MAX_RATION_DECAY_RATE           (1000000)
#define STEEMIT_FREE_TRANSACTIONS_WITH_NEW_ACCOUNT 100

#define STEEMIT_BANDWIDTH_AVERAGE_WINDOW_SECONDS (60*60*24*7) ///< 1 week
#define STEEMIT_BANDWIDTH_PRECISION             1000000ll ///< 1 million
#define STEEMIT_MAX_COMMENT_DEPTH_PRE_HF17      5
#define STEEMIT_MAX_COMMENT_DEPTH               0xfff0 // 64k - 16
#define STEEMIT_SOFT_MAX_COMMENT_DEPTH          0xff // 255

#define STEEMIT_MAX_RESERVE_RATIO   (20000)

#define GOLOS_CREATE_ACCOUNT_WITH_GOLOS_MODIFIER    30
#define GOLOS_CREATE_ACCOUNT_DELEGATION_RATIO       5
#define GOLOS_CREATE_ACCOUNT_DELEGATION_TIME        (fc::days(30))
#define GOLOS_MIN_DELEGATION_MULTIPLIER             10

#define STEEMIT_DEFAULT_DELEGATED_VESTING_INTEREST_RATE      (25*STEEMIT_1_PERCENT) // 25%
#define STEEMIT_MAX_DELEGATED_VESTING_INTEREST_RATE          STEEMIT_100_PERCENT
#define STEEMIT_MAX_DELEGATED_VESTING_INTEREST_RATE_PRE_HF22 (80*STEEMIT_1_PERCENT) // 80%

#define GOLOS_DEFAULT_REFERRAL_INTEREST_RATE    (10*STEEMIT_1_PERCENT) // 10%
#define GOLOS_MAX_REFERRAL_INTEREST_RATE        STEEMIT_100_PERCENT
#define GOLOS_DEFAULT_REFERRAL_TERM_SEC         (60*60*24*30*6)
#define GOLOS_MAX_REFERRAL_TERM_SEC             (60*60*24*30*12)
#define GOLOS_MIN_REFERRAL_BREAK_FEE            asset(1, STEEM_SYMBOL)
#define GOLOS_MAX_REFERRAL_BREAK_FEE_PRE_HF22   asset(100000, STEEM_SYMBOL)

#define STEEMIT_MINING_REWARD                   asset(666, STEEM_SYMBOL)
#define STEEMIT_MINING_REWARD_PRE_HF_16         asset(1000, STEEM_SYMBOL)
#define STEEMIT_EQUIHASH_N                      140
#define STEEMIT_EQUIHASH_K                      6

#define STEEMIT_LIQUIDITY_TIMEOUT_SEC           (fc::seconds(60*60*24*7)) // After one week volume is set to 0
#define STEEMIT_MIN_LIQUIDITY_REWARD_PERIOD_SEC (fc::seconds(60)) // 1 minute required on books to receive volume
#define STEEMIT_LIQUIDITY_REWARD_PERIOD_SEC     (60*60)
#define STEEMIT_LIQUIDITY_REWARD_BLOCKS         (STEEMIT_LIQUIDITY_REWARD_PERIOD_SEC/STEEMIT_BLOCK_INTERVAL)
#define STEEMIT_MIN_LIQUIDITY_REWARD            (asset(1000*STEEMIT_LIQUIDITY_REWARD_BLOCKS, STEEM_SYMBOL)) // Minumum reward to be paid out to liquidity providers
#define STEEMIT_MIN_CONTENT_REWARD              asset(1500, STEEM_SYMBOL)
#define STEEMIT_MIN_CURATE_REWARD               asset(500, STEEM_SYMBOL)
#define STEEMIT_MIN_PRODUCER_REWARD             STEEMIT_MINING_REWARD
#define STEEMIT_MIN_PRODUCER_REWARD_PRE_HF_16   STEEMIT_MINING_REWARD_PRE_HF_16
#define STEEMIT_MIN_POW_REWARD                  STEEMIT_MINING_REWARD
#define STEEMIT_MIN_POW_REWARD_PRE_HF_16        STEEMIT_MINING_REWARD_PRE_HF_16

#define STEEMIT_MIN_CURATION_PERCENT            0 // 0%
#define STEEMIT_MIN_CURATION_PERCENT_PRE_HF22   (25*STEEMIT_1_PERCENT) // 25%
#define STEEMIT_DEF_CURATION_PERCENT            (25*STEEMIT_1_PERCENT) // 25%
#define STEEMIT_MAX_CURATION_PERCENT            STEEMIT_100_PERCENT

#define STEEMIT_ACTIVE_CHALLENGE_FEE            asset(2000, STEEM_SYMBOL)
#define STEEMIT_OWNER_CHALLENGE_FEE             asset(30000, STEEM_SYMBOL)
#define STEEMIT_ACTIVE_CHALLENGE_COOLDOWN       fc::days(1)
#define STEEMIT_OWNER_CHALLENGE_COOLDOWN        fc::days(1)

// 5ccc e802 de5f
// int(expm1( log1p( 1 ) / BLOCKS_PER_YEAR ) * 2**STEEMIT_APR_PERCENT_SHIFT_PER_BLOCK / 100000 + 0.5)
// we use 100000 here instead of 10000 because we end up creating an additional 9x for vesting
#define STEEMIT_APR_PERCENT_MULTIPLY_PER_BLOCK          ( (uint64_t( 0x5ccc ) << 0x20) \
                                                        | (uint64_t( 0xe802 ) << 0x10) \
                                                        | (uint64_t( 0xde5f )        ) \
                                                        )
// chosen to be the maximal value such that STEEMIT_APR_PERCENT_MULTIPLY_PER_BLOCK * 2**64 * 100000 < 2**128
#define STEEMIT_APR_PERCENT_SHIFT_PER_BLOCK             87

#define STEEMIT_APR_PERCENT_MULTIPLY_PER_ROUND          ( (uint64_t( 0x79cc ) << 0x20 ) \
                                                        | (uint64_t( 0xf5c7 ) << 0x10 ) \
                                                        | (uint64_t( 0x3480 )         ) \
                                                        )

#define STEEMIT_APR_PERCENT_SHIFT_PER_ROUND             83

// We have different constants for hourly rewards
// i.e. hex(int(math.expm1( math.log1p( 1 ) / HOURS_PER_YEAR ) * 2**STEEMIT_APR_PERCENT_SHIFT_PER_HOUR / 100000 + 0.5))
#define STEEMIT_APR_PERCENT_MULTIPLY_PER_HOUR           ( (uint64_t( 0x6cc1 ) << 0x20) \
                                                        | (uint64_t( 0x39a1 ) << 0x10) \
                                                        | (uint64_t( 0x5cbd )        ) \
                                                        )

// chosen to be the maximal value such that STEEMIT_APR_PERCENT_MULTIPLY_PER_HOUR * 2**64 * 100000 < 2**128
#define STEEMIT_APR_PERCENT_SHIFT_PER_HOUR              77

// These constants add up to GRAPHENE_100_PERCENT.  Each GRAPHENE_1_PERCENT is equivalent to 1% per year APY
// *including the corresponding 9x vesting rewards*
#define STEEMIT_CURATE_APR_PERCENT              1937
#define STEEMIT_CONTENT_APR_PERCENT             5813
#define STEEMIT_LIQUIDITY_APR_PERCENT            750
#define STEEMIT_PRODUCER_APR_PERCENT             750
#define STEEMIT_POW_APR_PERCENT                  750

#define STEEMIT_MIN_PAYOUT_SBD                  (asset(20,SBD_SYMBOL))

#define STEEMIT_SBD_STOP_PERCENT                (5*STEEMIT_1_PERCENT ) // Stop printing SBD at 5% Market Cap
#define STEEMIT_SBD_START_PERCENT               (2*STEEMIT_1_PERCENT) // Start reducing printing of SBD at 2% Market Cap

#define STEEMIT_MIN_ACCOUNT_NAME_LENGTH          3
#define STEEMIT_MAX_ACCOUNT_NAME_LENGTH         16

#define STEEMIT_MIN_PERMLINK_LENGTH             0
#define STEEMIT_MAX_PERMLINK_LENGTH             256
#define STEEMIT_MAX_WITNESS_URL_LENGTH          2048

#define STEEMIT_INIT_SUPPLY                     int64_t(43306176000)
#define STEEMIT_MAX_SHARE_SUPPLY                int64_t(1000000000000000ll)
#define STEEMIT_MAX_SIG_CHECK_DEPTH             2

#define STEEMIT_MIN_TRANSACTION_SIZE_LIMIT      1024
#define STEEMIT_SECONDS_PER_YEAR                (uint64_t(60*60*24*365ll))

#define STEEMIT_SBD_INTEREST_COMPOUND_INTERVAL_SEC  (60*60*24*30)
#define STEEMIT_MAX_TRANSACTION_SIZE            (1024*64)
#define STEEMIT_MIN_BLOCK_SIZE_LIMIT            (STEEMIT_MAX_TRANSACTION_SIZE)
#define STEEMIT_MAX_BLOCK_SIZE                  (STEEMIT_MAX_TRANSACTION_SIZE*STEEMIT_BLOCK_INTERVAL*2000)
#define STEEMIT_BLOCKS_PER_HOUR                 (60*60/STEEMIT_BLOCK_INTERVAL)
#define STEEMIT_FEED_INTERVAL_BLOCKS            (STEEMIT_BLOCKS_PER_HOUR)
#define STEEMIT_FEED_HISTORY_WINDOW_PRE_HF_16   (24*7) /// 7 days * 24 hours per day
#define STEEMIT_FEED_HISTORY_WINDOW             (12*7) // 3.5 days
#define STEEMIT_MAX_FEED_AGE                    (fc::days(7))
#define STEEMIT_MIN_FEEDS                       (STEEMIT_MAX_WITNESSES/3) /// protects the network from conversions before price has been established
#define STEEMIT_CONVERSION_DELAY_PRE_HF_16      (fc::days(7))
#ifdef STEEMIT_BUILD_LIVETEST
#define STEEMIT_CONVERSION_DELAY                (fc::seconds(30)) //30 second conversion
#else
#define STEEMIT_CONVERSION_DELAY                (fc::hours(STEEMIT_FEED_HISTORY_WINDOW)) //3.5 day conversion
#endif

#define STEEMIT_SBD_DEBT_CONVERT_THRESHOLD      (20*STEEMIT_1_PERCENT) ///< Start force conversion SBD debt to GOLOS on account balances at 20% Market Cap
#define STEEMIT_SBD_DEBT_CONVERT_RATE           (STEEMIT_1_PERCENT) ///< Convert 1% of account balance (incl. savings) on each SBD debt conversion
#define STEEMIT_SBD_DEBT_CONVERT_INTERVAL       (STEEMIT_BLOCKS_PER_HOUR*24) ///< 1 day

#define GOLOS_MIN_WITNESS_SKIPPING_RESET_TIME   (60*60*1) ///< 1 hour
#define GOLOS_DEF_WITNESS_SKIPPING_RESET_TIME   (60*60*6) ///< 6 hours
#define GOLOS_MIN_WITNESS_IDLENESS_TIME         (60*60*24*30*1) ///< 1 month
#define GOLOS_DEF_WITNESS_IDLENESS_TIME         (60*60*24*30*12) ///< 12 month
#define GOLOS_WITNESS_IDLENESS_CHECK_INTERVAL   (STEEMIT_BLOCKS_PER_HOUR*24) ///< 1 day
#define GOLOS_WITNESS_IDLENESS_TOP_COUNT        100 ///< Count of TOP voted witnesses to clear votes if idling too long

#define GOLOS_MIN_ACCOUNT_IDLENESS_TIME         (60*60*24*30*1) ///< 1 month
#define GOLOS_DEF_ACCOUNT_IDLENESS_TIME         (60*60*24*30*12) ///< 12 month
#define GOLOS_ACCOUNT_IDLENESS_CHECK_INTERVAL   (STEEMIT_BLOCKS_PER_HOUR*24) ///< 1 day

#define GOLOS_MIN_CLAIM_IDLENESS_TIME           (60*60*24*1) ///< 1 day
#define GOLOS_DEF_CLAIM_IDLENESS_TIME           (60*60*24*1) ///< 1 day
#define GOLOS_CLAIM_IDLENESS_CHECK_INTERVAL     (STEEMIT_BLOCKS_PER_HOUR*(24+1)) ///< 1 day + 1 hour
#define GOLOS_ACCUM_DISTRIBUTION_INTERVAL_HF26  (60*60*1) ///< 3 hours
#define GOLOS_ACCUM_DISTRIBUTION_INTERVAL       (STEEMIT_BLOCKS_PER_HOUR) ///< 1 hour
#define GOLOS_ACCUM_DISTRIBUTION_STEP           100 ///< accounts receiving their VS share per block

#define GOLOS_MIN_INVITE_BALANCE                1000
#define GOLOS_DEF_MIN_INVITE_BALANCE            asset(10000, STEEM_SYMBOL)

#define GOLOS_MIN_ASSET_CREATION_FEE            100000
#define GOLOS_DEF_ASSET_CREATION_FEE            asset(2000000, SBD_SYMBOL)
#define GOLOS_MIN_INVITE_TRANSFER_INTERVAL_SEC  1
#define GOLOS_DEF_INVITE_TRANSFER_INTERVAL_SEC  60

#define GOLOS_MIN_CONVERT_FEE_PERCENT           0
#define GOLOS_MAX_CONVERT_FEE_PERCENT           STEEMIT_100_PERCENT
#define GOLOS_DEF_CONVERT_FEE_PERCENT           (5.00*STEEMIT_1_PERCENT)

#define GOLOS_MIN_GOLOS_POWER_TO_CURATE         0
#define GOLOS_DEF_GOLOS_POWER_TO_CURATE         asset(1000000, STEEM_SYMBOL)

#define GOLOS_MIN_GOLOS_POWER_TO_EMISSION       0
#define GOLOS_DEF_GOLOS_POWER_TO_EMISSION       asset(1000000, SBD_SYMBOL)

#define GOLOS_DEF_UNWANTED_OPERATION_COST       asset(100000, STEEM_SYMBOL)
#define GOLOS_DEF_UNLIMIT_OPERATION_COST        asset(10000, STEEM_SYMBOL)

#define GOLOS_MIN_NFT_ISSUE_COST                0
#define GOLOS_DEF_NFT_ISSUE_COST                asset(5000, SBD_SYMBOL)

#define GOLOS_MIN_PRIVATE_GROUP_GOLOS_POWER     0
#define GOLOS_DEF_PRIVATE_GROUP_GOLOS_POWER     asset(1000000, SBD_SYMBOL)
#define GOLOS_MIN_PRIVATE_GROUP_COST            0
#define GOLOS_DEF_PRIVATE_GROUP_COST            asset(0, SBD_SYMBOL)

#define STEEMIT_MIN_UNDO_HISTORY                10
#define STEEMIT_MAX_UNDO_HISTORY                10000

#define STEEMIT_MIN_TRANSACTION_EXPIRATION_LIMIT (STEEMIT_BLOCK_INTERVAL * 5) // 5 transactions per block
#define STEEMIT_BLOCKCHAIN_PRECISION            uint64_t(1000)

#define STEEMIT_BLOCKCHAIN_PRECISION_DIGITS     3
#define STEEMIT_MAX_INSTANCE_ID                 (uint64_t(-1)>>16)
/** NOTE: making this a power of 2 (say 2^15) would greatly accelerate fee calcs */
#define STEEMIT_MAX_AUTHORITY_MEMBERSHIP        10
#define STEEMIT_MAX_ASSET_WHITELIST_AUTHORITIES 10
#define STEEMIT_MAX_URL_LENGTH                  127

#define GRAPHENE_CURRENT_DB_VERSION             "GPH2.4"

#define STEEMIT_IRREVERSIBLE_THRESHOLD          (75 * STEEMIT_1_PERCENT)

#endif

/**
 *  Reserved Account IDs with special meaning
 */
///@{
/// Represents the current witnesses
#define STEEMIT_MINER_ACCOUNT                   "miners"
/// Represents the canonical account with NO authority (nobody can access funds in null account)
#define STEEMIT_NULL_ACCOUNT                    "null"
/// Represents the canonical account with WILDCARD authority (anybody can access funds in temp account)
#define STEEMIT_TEMP_ACCOUNT                    "temp"
/// Represents the canonical account for specifying you will vote for directly (as opposed to a proxy)
#define STEEMIT_PROXY_TO_SELF_ACCOUNT           ""
/// Represents the canonical root post parent account
#define STEEMIT_ROOT_POST_PARENT                (account_name_type())
/// Represents the current worker pool (stored on its balances)
#define STEEMIT_WORKER_POOL_ACCOUNT             "workers"
// Respresents the Golos Signer account which is using in authority to sign operations
#define STEEMIT_OAUTH_ACCOUNT                   "oauth"
// Represents the account which is used to register accounts using transfer
#define STEEMIT_REGISTRATOR_ACCOUNT             "newacc"
// Represents the account which is used to automatically send Golos Messenger technical notifications to users
#define STEEMIT_NOTIFY_ACCOUNT                  "notify"
// Represents the account which is used to recover accounts who haven't correct recovery_account at HF 27 time
#define STEEMIT_RECOVERY_ACCOUNT                "recovery"
///@}

#include <golos/protocol/config_old.hpp>
