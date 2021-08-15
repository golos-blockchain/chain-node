#include <boost/test/unit_test.hpp>

#include "database_fixture.hpp"

namespace golos { namespace chain {

struct hf22_database_fixture : public database_fixture {
    hf22_database_fixture() {
        try {
            initialize();
            open_database();

            generate_block();
            db->set_hardfork(STEEMIT_HARDFORK_0_21);
            startup(false);
        } catch (const fc::exception& e) {
            edump((e.to_detail_string()));
            throw;
        }
    }

    ~hf22_database_fixture() {
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
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, voter_key, op));
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
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, voter_key, op));
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

BOOST_AUTO_TEST_SUITE_END()
