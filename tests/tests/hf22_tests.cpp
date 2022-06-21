#include <boost/test/unit_test.hpp>

#include "database_fixture.hpp"
#include "gbg_helper.hpp"

namespace golos { namespace chain {

struct hf22_database_fixture : public database_fixture {
    uint32_t hf = STEEMIT_HARDFORK_0_21;

    hf22_database_fixture() {
        try {
            initialize();
            open_database();
            startup_with_hf(hf);
        } catch (const fc::exception& e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    ~hf22_database_fixture() {
    }

    void startup_with_hf(uint32_t hardfork) {
        generate_block();
        db->set_hardfork(hardfork);
        startup(false);
    }

    void resize_shared_mem(uint64_t size, bool restart = true) {
        db->wipe(data_dir->path(), data_dir->path(), true);
        db->open(data_dir->path(), data_dir->path(), INITIAL_TEST_SUPPLY, size, chainbase::database::read_write);
        if (restart) startup_with_hf(hf);
    }
};

} } // golos::chain

using namespace golos;
using namespace golos::chain;
using namespace golos::protocol;

// Requires build with MAX_19_VOTED_WITNESSES=TRUE
BOOST_FIXTURE_TEST_SUITE(hf22_tests, hf22_database_fixture)

    BOOST_AUTO_TEST_CASE(hf22_witness_vote_staked) { try {
        BOOST_TEST_MESSAGE("Testing: hf22_witness_vote_staked");

        signed_transaction tx;

        auto voter_key = generate_private_key("test");
        std::vector<account_name_type> a;
        for (auto i = 0; i < 3; ++i) {
            const auto name = "voter" + std::to_string(i);
            GOLOS_CHECK_NO_THROW(account_create(name, voter_key.get_public_key(), voter_key.get_public_key()));
            generate_block();
            vest(name, ASSET("10.000 GOLOS"));
            a.push_back(name);
        }
        generate_block();

        auto witness_key = generate_private_key("test");
        std::vector<account_name_type> w;
        for (auto i = 0; i < 6; ++i) {
            const auto name = "witness" + std::to_string(i);
            GOLOS_CHECK_NO_THROW(account_create(name, witness_key.get_public_key(), witness_key.get_public_key()));
            fund(name, 1000);
            GOLOS_CHECK_NO_THROW(witness_create(name, witness_key, "foo.bar", witness_key.get_public_key(), 1000));
            w.push_back(name);
        }
        generate_block();

        auto vote = [&](const account_name_type& voter, const account_name_type& witn, bool approve = true) {
            account_witness_vote_operation op;
            op.account = voter;
            op.witness = witn;
            op.approve = approve;
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, voter_key, op));
        };

        BOOST_TEST_MESSAGE("-- w0 = 1/3 + 1/3");

        vote(a[0], w[0]);
        vote(a[0], w[2]);
        vote(a[0], w[3]);

        vote(a[1], w[0]);
        vote(a[1], w[4]);
        vote(a[1], w[5]);

        BOOST_TEST_MESSAGE("-- w1 = 1");

        vote(a[2], w[1]);

        generate_block();

        BOOST_TEST_MESSAGE("-- Checking votes");

        BOOST_CHECK_GT(db->get_witness(w[0]).votes, db->get_witness(w[1]).votes);

        auto a0_weight = db->get_account(a[0]).vesting_shares.amount.value;
        auto a1_weight = db->get_account(a[1]).vesting_shares.amount.value;
        BOOST_CHECK_EQUAL(db->get_witness(w[0]).votes, a0_weight + a1_weight);
        BOOST_CHECK_EQUAL(db->get_witness(w[2]).votes, a0_weight);
        BOOST_CHECK_EQUAL(db->get_witness(w[3]).votes, a0_weight);
        BOOST_CHECK_EQUAL(db->get_witness(w[4]).votes, a1_weight);
        BOOST_CHECK_EQUAL(db->get_witness(w[5]).votes, a1_weight);

        BOOST_TEST_MESSAGE("-- Retally votes");

        db->set_hardfork(STEEMIT_HARDFORK_0_22);
        generate_block();
        validate_database();

        BOOST_TEST_MESSAGE("-- Checking votes");

        BOOST_CHECK_LT(db->get_witness(w[0]).votes, db->get_witness(w[1]).votes);

        a0_weight = db->get_account(a[0]).vesting_shares.amount.value / 3;
        a1_weight = db->get_account(a[1]).vesting_shares.amount.value / 3;
        BOOST_CHECK_EQUAL(db->get_witness(w[0]).votes, a0_weight + a1_weight);
        BOOST_CHECK_EQUAL(db->get_witness(w[2]).votes, a0_weight);
        BOOST_CHECK_EQUAL(db->get_witness(w[3]).votes, a0_weight);
        BOOST_CHECK_EQUAL(db->get_witness(w[4]).votes, a1_weight);
        BOOST_CHECK_EQUAL(db->get_witness(w[5]).votes, a1_weight);
    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(hf22_witness_idleness) { try {
        BOOST_TEST_MESSAGE("Testing: hf22_witness_idleness");

        signed_transaction tx;

        auto voter_key = generate_private_key("test");
        std::vector<account_name_type> a;
        for (auto i = 0; i < 3; ++i) {
            const auto name = "voter" + std::to_string(i);
            GOLOS_CHECK_NO_THROW(account_create(name, voter_key.get_public_key(), voter_key.get_public_key()));
            generate_block();
            vest(name, ASSET("10.000 GOLOS"));
            a.push_back(name);
        }
        generate_block();

        auto witness_key = generate_private_key("test");
        std::vector<account_name_type> w;
        for (auto i = 0; i < 6; ++i) {
            const auto name = "witness" + std::to_string(i);
            GOLOS_CHECK_NO_THROW(account_create(name, witness_key.get_public_key(), witness_key.get_public_key()));
            fund(name, 1000);
            GOLOS_CHECK_NO_THROW(witness_create(name, witness_key, "foo.bar", witness_key.get_public_key(), 1000));
            w.push_back(name);
        }
        generate_block();

        db_plugin->debug_update([&](database& db) {
            for (auto i = 0; i < 6; ++i) {
                db.modify(db.get_witness("witness" + std::to_string(i)), [&](auto& w) {
                    w.last_confirmed_block_num = 1;
                });
            }
        }, default_skip);

        auto vote = [&](const account_name_type& voter, const account_name_type& witn, bool approve = true) {
            account_witness_vote_operation op;
            op.account = voter;
            op.witness = witn;
            op.approve = approve;
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, voter_key, op));
        };

        BOOST_TEST_MESSAGE("-- w0 = 1/3 + 1/3");

        vote(a[0], w[0]);
        vote(a[0], w[2]);
        vote(a[0], w[3]);

        vote(a[1], w[0]);
        vote(a[1], w[4]);
        vote(a[1], w[5]);

        BOOST_TEST_MESSAGE("-- w1 = 1");

        vote(a[2], w[1]);

        generate_block();

        BOOST_TEST_MESSAGE("-- Checking votes");

        BOOST_CHECK_GT(db->get_witness(w[0]).votes, db->get_witness(w[1]).votes);

        auto a0_weight = db->get_account(a[0]).vesting_shares.amount.value;
        auto a1_weight = db->get_account(a[1]).vesting_shares.amount.value;
        BOOST_CHECK_EQUAL(db->get_witness(w[0]).votes, a0_weight + a1_weight);
        BOOST_CHECK_EQUAL(db->get_witness(w[2]).votes, a0_weight);
        BOOST_CHECK_EQUAL(db->get_witness(w[3]).votes, a0_weight);
        BOOST_CHECK_EQUAL(db->get_witness(w[4]).votes, a1_weight);
        BOOST_CHECK_EQUAL(db->get_witness(w[5]).votes, a1_weight);

        BOOST_TEST_MESSAGE("-- Clear votes");

        db->set_hardfork(STEEMIT_HARDFORK_0_22);
        generate_block();
        validate_database();

        BOOST_TEST_MESSAGE("-- Checking votes");

        BOOST_CHECK_EQUAL(db->get_witness(w[0]).votes, 0);
        BOOST_CHECK_EQUAL(db->get_witness(w[2]).votes, 0);
        BOOST_CHECK_EQUAL(db->get_witness(w[3]).votes, 0);
        BOOST_CHECK_EQUAL(db->get_witness(w[4]).votes, 0);
        BOOST_CHECK_EQUAL(db->get_witness(w[5]).votes, 0);
    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(hf22_sbd_debt_conversion) { try {
        BOOST_TEST_MESSAGE("Testing: hf22_sbd_debt_conversion");

        resize_shared_mem(1024 * 1024 * 24, STEEMIT_HARDFORK_0_17);

        ACTORS((alice)(alice2)(bob)(dave)(dave2))
        generate_block();
        vest("alice", ASSET("10.000 GOLOS"));
        vest("alice2", ASSET("10.000 GOLOS"));
        vest("bob", ASSET("10.000 GOLOS"));
        vest("dave", ASSET("10.000 GOLOS"));
        vest("dave2", ASSET("10.000 GOLOS"));

        set_price_feed(price(asset::from_string("1.000 GOLOS"), asset::from_string("1.000 GBG")));

        BOOST_TEST_MESSAGE("-- Making some GBG");

        comment_create("dave", dave_private_key, "test", "", "test");
        make_vote("bob", bob_private_key, "dave", "test");

        BOOST_CHECK_EQUAL(db->get_account("dave").sbd_balance.amount.value, 0);
        validate_database();

        auto dave_payout_time = db->get_comment_by_perm("dave", string("test")).cashout_time;
        generate_blocks(dave_payout_time, true);
        BOOST_CHECK_GT(db->get_account("dave").sbd_balance.amount.value, 0);

        transfer<transfer_to_savings_operation>("dave", "dave", db->get_account("dave").sbd_balance / 2);

        BOOST_TEST_MESSAGE("-- Making pending interest payout");

        transfer("dave", "dave2", asset(2000, SBD_SYMBOL));

        generate_blocks(db->head_block_time() + fc::days(90));

        transfer<transfer_to_savings_operation>("dave2", "alice", asset(1000, SBD_SYMBOL));
        transfer<transfer_to_savings_operation>("dave2", "alice2", asset(1000, SBD_SYMBOL));

        BOOST_TEST_MESSAGE("-- Making debt");

        BOOST_CHECK_EQUAL(db->get_dynamic_global_properties().sbd_debt_percent, 0);
        validate_database();

        set_price_feed(price(asset::from_string("100000.000 GOLOS"), asset::from_string("1.000 GBG")));

        {
            auto gb = STEEMIT_FEED_INTERVAL_BLOCKS - db->head_block_num() % STEEMIT_FEED_INTERVAL_BLOCKS;
            generate_blocks(gb);
        }

        BOOST_TEST_MESSAGE("-- But sbd_debt_percent should be 0");

        BOOST_CHECK_EQUAL(db->get_dynamic_global_properties().sbd_debt_percent, 0);
        validate_database();

        BOOST_TEST_MESSAGE("-- Applying HF");

        db->set_hardfork(STEEMIT_HARDFORK_0_22);
        generate_block();
        validate_database();

        BOOST_TEST_MESSAGE("-- Check sbd_debt_percent now is large");

        BOOST_CHECK_GE(db->get_dynamic_global_properties().sbd_debt_percent, STEEMIT_SBD_DEBT_CONVERT_THRESHOLD);

        gbg_helper helper(db);

        BOOST_TEST_MESSAGE("-- Last blocks before conversion");

        {
            auto gb = STEEMIT_SBD_DEBT_CONVERT_INTERVAL - db->head_block_num() % STEEMIT_SBD_DEBT_CONVERT_INTERVAL;
            generate_blocks(gb - 1);
        }

        validate_database();
        helper.validate_balances();

        BOOST_TEST_MESSAGE("-- Initiate conversion");

        generate_block();

        helper.trigger_convert();

        BOOST_CHECK_GT(helper.payout("dave").amount.value, 0);
        BOOST_CHECK_GT(helper.savings_payout("dave").amount.value, 0);
        
        validate_database();
    } catch (fc::exception& e) {
        edump((e.to_detail_string()));
        throw;
    }}

BOOST_AUTO_TEST_SUITE_END()
