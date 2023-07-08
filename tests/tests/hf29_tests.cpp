#include <boost/test/unit_test.hpp>

#include "database_fixture.hpp"
#include "helpers.hpp"

using namespace golos;
using namespace golos::chain;
using namespace golos::protocol;

BOOST_FIXTURE_TEST_SUITE(hf29_tests, clean_database_fixture)

    BOOST_AUTO_TEST_CASE(paid_subscriptions) { try {
        BOOST_TEST_MESSAGE("Testing: paid_subscriptions");

        ACTORS((alice)(bob)(carol)(dave))
        generate_block();

        signed_transaction tx;

        BOOST_TEST_MESSAGE("-- Create single-execute paid subscription by alice");

        paid_subscription_create_operation pscop;
        pscop.author = "alice";
        pscop.oid = paid_subscription_id{"alicegram", "access", 1};
        pscop.cost = asset(10000, STEEM_SYMBOL);
        pscop.tip_cost = true;
        pscop.allow_prepaid = false;
        pscop.interval = 0;
        pscop.executions = 0;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, pscop));
        generate_block();

        BOOST_TEST_MESSAGE("-- Subscribe by bob");

        fund("bob", 20000);
        {
            transfer_to_tip_operation totip;
            totip.from = "bob";
            totip.to = "bob";
            totip.amount = asset(20000, STEEM_SYMBOL);
            GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, totip));
        }

        paid_subscription_transfer_operation pstop;
        pstop.from = "bob";
        pstop.to = "alice";
        pstop.oid = pscop.oid;
        pstop.amount = asset(15000, STEEM_SYMBOL);
        pstop.from_tip = true;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, pstop));
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscription");

        {
            const auto& idx = db->get_index<paid_subscriber_index, by_author_oid_subscriber>();
            auto ssingle = idx.find(std::make_tuple("alice", pscop.oid));
            BOOST_CHECK(ssingle != idx.end());
            BOOST_CHECK_EQUAL(ssingle->active, true);
            BOOST_CHECK_EQUAL(ssingle->subscriber, "bob");
            BOOST_CHECK_EQUAL(ssingle->prepaid.amount.value, 0);
            BOOST_CHECK_EQUAL(ssingle->next_payment, fc::time_point_sec(0));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("bob").tip_balance, asset(5000, STEEM_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").tip_balance, asset(15000, STEEM_SYMBOL));

        BOOST_TEST_MESSAGE("-- Create periodic paid subscription by alice");

        pscop.author = "alice";
        pscop.oid = paid_subscription_id{"alicegram", "hugs", 1};
        pscop.cost = asset(500, SBD_SYMBOL);
        pscop.tip_cost = false;
        pscop.allow_prepaid = true;
        pscop.interval = 30;
        pscop.executions = 3;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, pscop));
        generate_block();

        BOOST_TEST_MESSAGE("-- Subscribe by carol, dave");

        fund("carol", ASSET("10.000 GBG"));
        fund("dave", ASSET("0.500 GBG"));

        pstop.from = "carol";
        pstop.to = "alice";
        pstop.oid = pscop.oid;
        pstop.amount = asset(2010, SBD_SYMBOL);
        pstop.from_tip = false;

        paid_subscription_transfer_operation pstop2;
        pstop2.from = "dave";
        pstop2.to = "alice";
        pstop2.oid = pscop.oid;
        pstop2.amount = asset(500, SBD_SYMBOL);
        pstop2.from_tip = false;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, pstop));
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, dave_private_key, pstop2));

        auto created = db->head_block_time();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* dave = db->find_paid_subscriber("dave", "alice", pscop.oid);
            BOOST_CHECK(dave != nullptr);
            BOOST_CHECK_EQUAL(dave->active, true);
            BOOST_CHECK_EQUAL(dave->prepaid.amount.value, 0);
            BOOST_CHECK_EQUAL(dave->next_payment, created + fc::seconds(30));

            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(1510, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30));
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("carol").sbd_balance, asset(7990, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("dave").sbd_balance, asset(0, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, asset(500 + 500, SBD_SYMBOL));

        BOOST_TEST_MESSAGE("-- Wait for next payment");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL - 2);

        {
            const auto* dave = db->find_paid_subscriber("dave", "alice", pscop.oid);
            BOOST_CHECK(dave != nullptr);
            BOOST_CHECK_EQUAL(dave->active, true);
            BOOST_CHECK_EQUAL(dave->next_payment, created + fc::seconds(30));
        }

        validate_database();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* dave = db->find_paid_subscriber("dave", "alice", pscop.oid);
            BOOST_CHECK(dave != nullptr);
            BOOST_CHECK_EQUAL(dave->active, false); // he paid for 1 interval
            BOOST_CHECK_EQUAL(dave->prepaid.amount.value, 0);
            BOOST_CHECK_EQUAL(dave->next_payment, fc::time_point_sec(0));

            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(1010, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30 * 2));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("carol").sbd_balance, asset(7990, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("dave").sbd_balance, asset(0, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, asset(500 + 500 + 500, SBD_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, #2");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(510, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30 * 3));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("carol").sbd_balance, asset(7990, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, asset(500 + 500 + 500 + 500, SBD_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, #3");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30 * 4));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("carol").sbd_balance, asset(7990, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, asset(500 + 500 + 500 + 500 + 510, SBD_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, (subscription end)");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, false);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, fc::time_point_sec(0));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("carol").sbd_balance, asset(7990, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, asset(500 + 500 + 500 + 500 + 510, SBD_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Update subscription, to 0.001 GOLOS");

        paid_subscription_update_operation psuop;
        psuop.author = "alice";
        psuop.oid = paid_subscription_id{"alicegram", "hugs", 1};
        psuop.cost = asset(1, STEEM_SYMBOL);
        psuop.tip_cost = false;
        psuop.interval = 30;
        psuop.executions = 3;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, psuop));
        generate_block();

        BOOST_TEST_MESSAGE("-- Subscribe by carol");

        fund("carol", ASSET("10.000 GOLOS"));

        pstop.from = "carol";
        pstop.to = "alice";
        pstop.oid = pscop.oid;
        pstop.amount = asset(10, STEEM_SYMBOL);
        pstop.from_tip = false;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, pstop));

        auto updated = db->head_block_time();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(10, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, updated + fc::seconds(30));
        }

        BOOST_TEST_MESSAGE("-- Wait for next payment");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL - 1);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(9, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, updated + fc::seconds(30 * 2));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("carol").sbd_balance, asset(7990, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, asset(500 + 500 + 500 + 500 + 510, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").balance, asset(1, STEEM_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, #2");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(8, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, updated + fc::seconds(30 * 3));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("alice").balance, asset(2, STEEM_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, #3");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, updated + fc::seconds(30 * 4));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("alice").balance, asset(10, STEEM_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, end");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, false);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, fc::time_point_sec(0));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("alice").balance, asset(10, STEEM_SYMBOL));

        validate_database();

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(paid_subscriptions_no_prepaid) { try {
        BOOST_TEST_MESSAGE("Testing: paid_subscriptions_no_prepaid");
        return;

        ACTORS((alice)(carol))
        generate_block();

        signed_transaction tx;

        BOOST_TEST_MESSAGE("-- Create periodic paid subscription by alice");

        paid_subscription_create_operation pscop;
        pscop.author = "alice";
        pscop.oid = paid_subscription_id{"alicegram", "hugs", 1};
        pscop.cost = asset(500, SBD_SYMBOL);
        pscop.tip_cost = false;
        pscop.allow_prepaid = false;
        pscop.interval = 30;
        pscop.executions = 2;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, pscop));
        generate_block();

        BOOST_TEST_MESSAGE("-- Subscribe by carol");

        fund("carol", ASSET("10.000 GBG"));

        paid_subscription_transfer_operation pstop;
        pstop.from = "carol";
        pstop.to = "alice";
        pstop.oid = pscop.oid;
        pstop.amount = asset(500, SBD_SYMBOL);
        pstop.from_tip = false;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, pstop));

        auto created = db->head_block_time();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30));
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("carol").sbd_balance, asset(9500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, asset(500, SBD_SYMBOL));

        BOOST_TEST_MESSAGE("-- Wait for next payment");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL - 1);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30 * 2));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("carol").sbd_balance, asset(9000, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, asset(500 + 500, SBD_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, #2");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30 * 3));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("carol").sbd_balance, asset(8500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, asset(500 + 500 + 500, SBD_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, (subscription end)");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, false);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, fc::time_point_sec(0));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("carol").sbd_balance, asset(8500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, asset(500 + 500 + 500, SBD_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Update subscription, to 0.001 GOLOS");

        paid_subscription_update_operation psuop;
        psuop.author = "alice";
        psuop.oid = paid_subscription_id{"alicegram", "hugs", 1};
        psuop.cost = asset(1, STEEM_SYMBOL);
        psuop.tip_cost = false;
        psuop.interval = 30;
        psuop.executions = 2;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, psuop));
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, false);
            BOOST_CHECK_EQUAL(carol->cost, asset(1, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, fc::time_point_sec(0));
        }

        BOOST_TEST_MESSAGE("-- Renew subscribe by carol");

        fund("carol", ASSET("1.000 GOLOS"));

        pstop.from = "carol";
        pstop.to = "alice";
        pstop.oid = pscop.oid;
        pstop.amount = asset(1, STEEM_SYMBOL);
        pstop.from_tip = false;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, pstop));

        auto updated = db->head_block_time();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, updated + fc::seconds(30));
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("carol").sbd_balance, asset(8500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, asset(500 + 500 + 500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("carol").balance, asset(999, STEEM_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").balance, asset(1, STEEM_SYMBOL));

        BOOST_TEST_MESSAGE("-- Wait for next payment");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL - 1);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, updated + fc::seconds(30 * 2));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("carol").sbd_balance, asset(8500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, asset(500 + 500 + 500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("carol").balance, asset(998, STEEM_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").balance, asset(2, STEEM_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Update subscription, again to 0.010 GBG");

        psuop.author = "alice";
        psuop.oid = paid_subscription_id{"alicegram", "hugs", 1};
        psuop.cost = asset(10, SBD_SYMBOL);
        psuop.tip_cost = false;
        psuop.interval = 30;
        psuop.executions = 2;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, psuop));
        generate_block();

        BOOST_TEST_MESSAGE("-- Wait for next payment, #2");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL - 1);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, updated + fc::seconds(30 * 3));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("carol").sbd_balance, asset(8490, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, asset(500 + 500 + 500 + 10, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("carol").balance, asset(998, STEEM_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").balance, asset(2, STEEM_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, (subscription end)");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = db->find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, false);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, fc::time_point_sec(0));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(db->get_account("carol").sbd_balance, asset(8490, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").sbd_balance, asset(500 + 500 + 500 + 10, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("carol").balance, asset(998, STEEM_SYMBOL));
        BOOST_CHECK_EQUAL(db->get_account("alice").balance, asset(2, STEEM_SYMBOL));

        validate_database();

    } FC_LOG_AND_RETHROW() }

BOOST_AUTO_TEST_SUITE_END()
