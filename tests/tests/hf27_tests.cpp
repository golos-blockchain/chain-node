#include <boost/test/unit_test.hpp>

#include "database_fixture.hpp"
#include "comment_reward.hpp"
#include "helpers.hpp"

namespace golos { namespace chain {

struct hf27_database_fixture : public database_fixture {
    hf27_database_fixture() {
        try {
            initialize();
            open_database();

            generate_block();
            db->set_hardfork(STEEMIT_HARDFORK_0_26);
#ifdef STEEMIT_BUILD_TESTNET
            db->_test_freezing = true;
#endif
            startup(false);
        } catch (const fc::exception& e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    ~hf27_database_fixture() {
    }

    void create200accs() {
        PREP_ACTOR(created);
        for (int i = 1; i <= 200; ++i) {
            auto name = "created" + std::to_string(i);
            GOLOS_CHECK_NO_THROW(account_create(name, created_public_key, created_post_key.get_public_key()));
            fund(name, 1000);
            vest(name, 1000);
        }
    }
};

} } // golos::chain

using namespace golos;
using namespace golos::chain;
using namespace golos::protocol;

// Requires build with MAX_19_VOTED_WITNESSES=TRUE
BOOST_FIXTURE_TEST_SUITE(hf27_tests, hf27_database_fixture)

    BOOST_AUTO_TEST_CASE(emission_distribution_before_hf27) { try {
        BOOST_TEST_MESSAGE("Testing: emission_distribution_before_hf27");

        BOOST_TEST_MESSAGE("-- Create +200 accs");

        create200accs();

        BOOST_TEST_MESSAGE("-- Checking no distribution before interval occurs");

        generate_block();

        auto gb = GOLOS_ACCUM_DISTRIBUTION_INTERVAL - (db->head_block_num() % GOLOS_ACCUM_DISTRIBUTION_INTERVAL);
        generate_blocks(gb - 1);

        for (const auto& acc : db->get_index<account_index>().indices()) {
            BOOST_CHECK_EQUAL(acc.accumulative_balance.amount, 0);
            BOOST_CHECK_EQUAL(acc.tip_balance.amount, 0);
        }

        {
            auto arops = get_last_operations<accumulative_remainder_operation>(1);
            BOOST_CHECK_EQUAL(arops.size(), 0);
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Performing distribution");

        generate_blocks(1);

        BOOST_TEST_MESSAGE("-- Checking correctness of distribution");

        const auto& props = db->get_dynamic_global_properties();

        BOOST_CHECK_GT(db->get_account("cyberfounder").accumulative_balance.amount, 0);
        BOOST_CHECK_EQUAL(db->get_account("cyberfounder").tip_balance.amount, 0);

        asset total_distributed = asset(0, STEEM_SYMBOL);
        for (const auto& acc : db->get_index<account_index>().indices()) {
            total_distributed += acc.accumulative_balance;
        }

        auto arops = get_last_operations<accumulative_remainder_operation>(1);
        if (arops.size()) {
            total_distributed += arops[0].amount;
        }

        for (const auto& acc : db->get_index<account_index>().indices()) {
            auto stake = (uint128_t(total_distributed.amount.value) *
                acc.emission_vesting_shares().amount.value / props.total_vesting_shares.amount.value).to_uint64();
            APPROX_CHECK_EQUAL(acc.accumulative_balance.amount.value, int64_t(stake), 1);
        }

        BOOST_TEST_MESSAGE("-- Waiting for claim idleness");

        auto gb2 = GOLOS_CLAIM_IDLENESS_CHECK_INTERVAL - (db->head_block_num() % GOLOS_CLAIM_IDLENESS_CHECK_INTERVAL);
        generate_blocks(gb2 - 1);

        BOOST_CHECK_GT(db->get_account("cyberfounder").accumulative_balance.amount, 0);
        BOOST_CHECK_EQUAL(db->get_account("cyberfounder").tip_balance.amount, 0);

        BOOST_TEST_MESSAGE("-- Checking for claim idleness");

        comment_fund fund(*db);

        auto workers = fund.worker_fund();

        generate_blocks(1);

        for (const auto& acc : db->get_index<account_index>().indices()) {
            BOOST_CHECK_EQUAL(acc.accumulative_balance.amount, 0);
            BOOST_CHECK_EQUAL(acc.tip_balance.amount, 0);
        }

        workers += total_distributed;
        if (arops.size()) {
            workers -= arops[0].amount;
        }

        BOOST_CHECK_EQUAL(db->get_account("workers").balance, workers);

        validate_database();
    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(emission_distribution_after_hf27) { try {
        BOOST_TEST_MESSAGE("Testing: emission_distribution_after_hf27");

        BOOST_TEST_MESSAGE("-- Create +200 accs");

        create200accs();

        BOOST_TEST_MESSAGE("-- Performing distribution");

        auto gb = GOLOS_ACCUM_DISTRIBUTION_INTERVAL - (db->head_block_num() % GOLOS_ACCUM_DISTRIBUTION_INTERVAL);
        generate_blocks(gb);

        BOOST_CHECK_GT(db->get_account("cyberfounder").accumulative_balance.amount, 0);
        BOOST_CHECK_EQUAL(db->get_account("cyberfounder").tip_balance.amount, 0);

        std::vector<account_name_type> acc_names;
        std::vector<asset> acc_bals;

        {
            const auto& acc_idx = db->get_index<account_index, by_accumulative>();
            auto acc_itr = acc_idx.begin();
            for (; acc_itr != acc_idx.end() && acc_itr->accumulative_balance.amount != 0; ++acc_itr) {
                acc_names.push_back(acc_itr->name);
                acc_bals.push_back(acc_itr->accumulative_balance);
            }
        }

        BOOST_CHECK_GT(acc_names.size(), 100); // ~221

        BOOST_TEST_MESSAGE("-- Apply HF");

        db->set_hardfork(STEEMIT_HARDFORK_0_27);
        validate_database();

        BOOST_TEST_MESSAGE("-- Checking 1st auto claim");

        generate_block();

        for (size_t i = 0; i < 100; ++i) {
            BOOST_CHECK_EQUAL(db->get_account(acc_names[i]).accumulative_balance.amount, 0);
            BOOST_CHECK_EQUAL(db->get_account(acc_names[i]).tip_balance, acc_bals[i]);
        }

        for (size_t i = 100; i < acc_names.size(); ++i) {
            BOOST_CHECK_GT(db->get_account(acc_names[i]).accumulative_balance.amount, 0);
            BOOST_CHECK_EQUAL(db->get_account(acc_names[i]).tip_balance.amount, 0);
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Checking 2rd auto claim");

        generate_block();

        for (size_t i = 0; i < 100*2; ++i) {
            BOOST_CHECK_EQUAL(db->get_account(acc_names[i]).accumulative_balance.amount, 0);
            BOOST_CHECK_EQUAL(db->get_account(acc_names[i]).tip_balance, acc_bals[i]);
        }

        for (size_t i = 100*2; i < acc_names.size(); ++i) {
            BOOST_CHECK_GT(db->get_account(acc_names[i]).accumulative_balance.amount, 0);
            BOOST_CHECK_EQUAL(db->get_account(acc_names[i]).tip_balance.amount, 0);
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Checking last auto claim");

        generate_block();

        for (size_t i = 0; i <  acc_names.size(); ++i) {
            BOOST_CHECK_EQUAL(db->get_account(acc_names[i]).accumulative_balance.amount, 0);
            BOOST_CHECK_EQUAL(db->get_account(acc_names[i]).tip_balance, acc_bals[i]);
        }

        for (const auto& acc : db->get_index<account_index>().indices()) {
            BOOST_CHECK_EQUAL(acc.accumulative_balance.amount, 0);
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Checking no more auto claims");

        generate_block();

        validate_database();

        BOOST_TEST_MESSAGE("-- Waiting for next emission");

        {
            auto gb = GOLOS_ACCUM_DISTRIBUTION_INTERVAL - (db->head_block_num() % GOLOS_ACCUM_DISTRIBUTION_INTERVAL);
            generate_blocks(gb);
        }

        validate_database();

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(account_freezing) { try {
        BOOST_TEST_MESSAGE("Testing: account_freezing");

        BOOST_TEST_MESSAGE("-- Create +200 accs");

        create200accs();

        generate_block();

        BOOST_TEST_MESSAGE("-- Marking them as inactive");

        db->modify(db->get_account("cyberfounder"), [&](auto& acc) {
            acc.recovery_account = "gc-regfund";
        });

        generate_block();

        int accs_now = 0;

        for (const auto& acc : db->get_index<account_index>().indices()) {
            BOOST_CHECK_EQUAL(acc.frozen, false);
            ++accs_now;
        }

        BOOST_TEST_MESSAGE("-- Apply HF, check freezing");

        db->set_hardfork(STEEMIT_HARDFORK_0_27);
        validate_database();

        generate_block();
        validate_database();

        int accs_after = 0;

        const auto& idx = db->get_index<account_index, by_proved>();
        auto itr = idx.begin();
        for (; itr != idx.end() && !itr->frozen; ++itr) {
            ++accs_after;
        }

        // 5 was system ones + 1 system acc (newacc) created when HF
        BOOST_CHECK_EQUAL(accs_now - accs_after, 94);

        generate_blocks(38);

        itr = idx.begin();
        for (; itr != idx.end(); ++itr) {
            BOOST_CHECK_EQUAL(itr->proved_hf, 27);
        }

        BOOST_TEST_MESSAGE("-- Check setting recovery account");

        BOOST_CHECK_EQUAL(db->get_account("cyberfounder").recovery_account, "gc-regfund");

        validate_database();

        generate_block(1);

        BOOST_CHECK_EQUAL(db->get_account("cyberfounder").recovery_account, "recovery");

        validate_database();

        BOOST_TEST_MESSAGE("-- Checking recovery accounts are not fixing after HF27 long-ago interval");

        db->modify(db->get_account("cyberfounder"), [&](auto& acc) {
            acc.recovery_account = "gc-regfund";
        });

        generate_block(1);

        BOOST_CHECK_EQUAL(db->get_account("cyberfounder").recovery_account, "gc-regfund");

        validate_database();
    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(comment_negrep_payout) { try {
        BOOST_TEST_MESSAGE("Testing: comment_negrep_payout");

        signed_transaction tx;

        ACTORS((goodie)(badie)(whale)(curator))
        generate_block();
        validate_database();

        vest("goodie", ASSET_GOLOS(1000));
        vest("badie", ASSET_GOLOS(1000));
        vest("curator", ASSET_GOLOS(1000));
        vest("whale", ASSET_GOLOS(10000000));

        validate_database();

        // Need for curator payouts
        price exchange_rate(ASSET("1.000 GOLOS"), ASSET("1.000 GBG"));
        set_price_feed(exchange_rate);
        generate_block();

        validate_database();

        BOOST_TEST_MESSAGE("-- Creating two posts");

        comment_create("goodie", goodie_private_key, "test", "", "test", "Hello", "World", "");
        comment_create("badie", badie_private_key, "test", "", "test", "Hello", "World", "");

        generate_block(); // prevent auction time

        validate_database();

        BOOST_TEST_MESSAGE("-- Making badie reputation negative");

        {
            vote_operation vote;
            vote.voter = "whale";
            vote.author = "whale";
            vote.weight = 10000;
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, whale_private_key, vote));

            BOOST_CHECK_GT(db->get_account_reputation("whale"), 0);

            vote.author = "badie";
            vote.weight = -10000; 
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, whale_private_key, vote));
        }

        BOOST_CHECK_LT(db->get_account_reputation("badie"), 0);
        BOOST_CHECK_EQUAL(db->get_account_reputation("goodie"), 0);

        validate_database();

        BOOST_TEST_MESSAGE("-- Curating");

        vote_operation vote;
        vote.voter = "curator";
        vote.author = "goodie";
        vote.permlink = "test";
        vote.weight = STEEMIT_100_PERCENT; 
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, curator_private_key, vote));

        vote.author = "badie";
        vote.permlink = "test";
        vote.weight = STEEMIT_100_PERCENT; 
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, curator_private_key, vote));

        validate_database();

        BOOST_TEST_MESSAGE("-- Waiting for payout");

        generate_blocks(db->head_block_time() + STEEMIT_CASHOUT_WINDOW_SECONDS);

        BOOST_CHECK_EQUAL(db->get_comment_by_perm("goodie", string("test")).mode, archived);
        BOOST_CHECK_EQUAL(db->get_comment_by_perm("badie", string("test")).mode, archived);

        {
            auto ops = get_last_operations<curation_reward_operation>(10);
            BOOST_CHECK_EQUAL(ops.size(), 2);
        }

        {
            auto ops = get_last_operations<author_reward_operation>(10);
            BOOST_CHECK_EQUAL(ops.size(), 2);
        }
    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(comment_negrep_payout_after_hf27) { try {
        BOOST_TEST_MESSAGE("Testing: comment_negrep_payout_after_hf27");

        signed_transaction tx;

        ACTORS((goodie)(badie)(whale)(curator))
        generate_block();

        validate_database();

        vest("goodie", ASSET_GOLOS(1000));
        vest("badie", ASSET_GOLOS(1000));
        vest("curator", ASSET_GOLOS(1000));
        vest("whale", ASSET_GOLOS(10000000));

        validate_database();

        // Need for curator payouts
        price exchange_rate(ASSET("1.000 GOLOS"), ASSET("1.000 GBG"));
        set_price_feed(exchange_rate);
        generate_block();

        validate_database();

        BOOST_TEST_MESSAGE("-- Creating two posts");

        comment_create("goodie", goodie_private_key, "test", "", "test", "Hello", "World", "");
        comment_create("badie", badie_private_key, "test", "", "test", "Hello", "World", "");

        generate_block(); // prevent auction time

        validate_database();

        BOOST_TEST_MESSAGE("-- Making badie reputation negative");

        {
            vote_operation vote;
            vote.voter = "whale";
            vote.author = "whale";
            vote.weight = 10000;
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, whale_private_key, vote));

            BOOST_CHECK_GT(db->get_account_reputation("whale"), 0);

            vote.author = "badie";
            vote.weight = -10000; 
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, whale_private_key, vote));
        }

        BOOST_CHECK_LT(db->get_account_reputation("badie"), 0);
        BOOST_CHECK_EQUAL(db->get_account_reputation("goodie"), 0);

        validate_database();

        BOOST_TEST_MESSAGE("-- Curating");

        vote_operation vote;
        vote.voter = "curator";
        vote.author = "goodie";
        vote.permlink = "test";
        vote.weight = STEEMIT_100_PERCENT; 
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, curator_private_key, vote));

        vote.author = "badie";
        vote.permlink = "test";
        vote.weight = STEEMIT_100_PERCENT; 
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, curator_private_key, vote));

        validate_database();

        BOOST_TEST_MESSAGE("-- Waiting for payout");

        generate_blocks(db->get_comment_by_perm("goodie", string("test")).created + STEEMIT_CASHOUT_WINDOW_SECONDS - STEEMIT_BLOCK_INTERVAL);

        db->set_hardfork(STEEMIT_HARDFORK_0_27);

        generate_block();

        BOOST_CHECK_EQUAL(db->get_comment_by_perm("goodie", string("test")).mode, archived);
        BOOST_CHECK_EQUAL(db->get_comment_by_perm("badie", string("test")).mode, archived);

        {
            auto ops = get_last_operations<curation_reward_operation>(10);
            BOOST_CHECK_EQUAL(ops.size(), 1);
        }

        {
            auto ops = get_last_operations<author_reward_operation>(10);
            BOOST_CHECK_EQUAL(ops.size(), 1);
        }
    } FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
