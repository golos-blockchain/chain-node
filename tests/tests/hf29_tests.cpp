#include <boost/test/unit_test.hpp>

#include "database_fixture.hpp"
#include "helpers.hpp"

using namespace golos;
using namespace golos::chain;
using namespace golos::protocol;

BOOST_FIXTURE_TEST_SUITE(hf29_tests, clean_database_fixture_wrap)

    BOOST_AUTO_TEST_CASE(paid_subscription_create_validate) { try {
        BOOST_TEST_MESSAGE("Testing: paid_subscription_create_validate");

        BOOST_TEST_MESSAGE("--- success");

        paid_subscription_create_operation pscop;
        pscop.author = "alice";
        pscop.oid = paid_subscription_id{"alicegram", "access", 1};
        pscop.cost = asset(10000, STEEM_SYMBOL);
        pscop.tip_cost = true;
        pscop.allow_prepaid = false;
        pscop.interval = 0;
        pscop.executions = 0;
        GOLOS_CHECK_NO_THROW(pscop.validate());

        BOOST_TEST_MESSAGE("--- throw if negative");
        pscop.cost = asset(-10000, STEEM_SYMBOL);
        GOLOS_CHECK_ERROR_PROPS(pscop.validate(),
            CHECK_ERROR(invalid_parameter, "cost"));

        BOOST_TEST_MESSAGE("--- throw if GESTS");
        pscop.cost = asset(10000, VESTS_SYMBOL);
        GOLOS_CHECK_ERROR_PROPS(pscop.validate(),
            CHECK_ERROR(invalid_parameter, "cost"));

        BOOST_TEST_MESSAGE("--- throw if GBG and tip_cost");
        pscop.cost = asset(10000, SBD_SYMBOL);
        GOLOS_CHECK_ERROR_PROPS(pscop.validate(),
            CHECK_ERROR(invalid_parameter, "cost"));

        BOOST_TEST_MESSAGE("--- success if GBG and no tip_cost");
        pscop.tip_cost = false;
        GOLOS_CHECK_NO_THROW(pscop.validate());

        BOOST_TEST_MESSAGE("--- throw if wrong oid");
        pscop.oid = paid_subscription_id{"alicegram", "", 1};
        GOLOS_CHECK_ERROR_PROPS(pscop.validate(),
            CHECK_ERROR(invalid_parameter, "name"));

        BOOST_TEST_MESSAGE("--- throw if wrong oid 2");
        pscop.oid = paid_subscription_id{"", "access", 1};
        GOLOS_CHECK_ERROR_PROPS(pscop.validate(),
            CHECK_ERROR(invalid_parameter, "app"));

        BOOST_TEST_MESSAGE("--- throw if wrong oid 3");
        pscop.oid = paid_subscription_id{"alicegram", "access", 0};
        GOLOS_CHECK_ERROR_PROPS(pscop.validate(),
            CHECK_ERROR(invalid_parameter, "version"));

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(paid_subscription_update_validate) { try {
        BOOST_TEST_MESSAGE("Testing: paid_subscription_update_validate");

        BOOST_TEST_MESSAGE("--- success");

        paid_subscription_update_operation psuop;
        psuop.author = "alice";
        psuop.oid = paid_subscription_id{"alicegram", "access", 1};
        psuop.cost = asset(10000, STEEM_SYMBOL);
        psuop.tip_cost = true;
        psuop.interval = 0;
        psuop.executions = 0;
        GOLOS_CHECK_NO_THROW(psuop.validate());

        BOOST_TEST_MESSAGE("--- throw if negative");
        psuop.cost = asset(-10000, STEEM_SYMBOL);
        GOLOS_CHECK_ERROR_PROPS(psuop.validate(),
            CHECK_ERROR(invalid_parameter, "cost"));

        BOOST_TEST_MESSAGE("--- throw if GESTS");
        psuop.cost = asset(10000, VESTS_SYMBOL);
        GOLOS_CHECK_ERROR_PROPS(psuop.validate(),
            CHECK_ERROR(invalid_parameter, "cost"));

        BOOST_TEST_MESSAGE("--- throw if GBG and tip_cost");
        psuop.cost = asset(10000, SBD_SYMBOL);
        GOLOS_CHECK_ERROR_PROPS(psuop.validate(),
            CHECK_ERROR(invalid_parameter, "cost"));

        BOOST_TEST_MESSAGE("--- success if GBG and no tip_cost");
        psuop.tip_cost = false;
        GOLOS_CHECK_NO_THROW(psuop.validate());

        BOOST_TEST_MESSAGE("--- throw if wrong oid");
        psuop.oid = paid_subscription_id{"alicegram", "", 1};
        GOLOS_CHECK_ERROR_PROPS(psuop.validate(),
            CHECK_ERROR(invalid_parameter, "name"));

        BOOST_TEST_MESSAGE("--- throw if wrong oid 2");
        psuop.oid = paid_subscription_id{"", "access", 1};
        GOLOS_CHECK_ERROR_PROPS(psuop.validate(),
            CHECK_ERROR(invalid_parameter, "app"));

        BOOST_TEST_MESSAGE("--- throw if wrong oid 3");
        psuop.oid = paid_subscription_id{"alicegram", "access", 0};
        GOLOS_CHECK_ERROR_PROPS(psuop.validate(),
            CHECK_ERROR(invalid_parameter, "version"));
    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(paid_subscription_delete_validate) { try {
        BOOST_TEST_MESSAGE("Testing: paid_subscription_delete_validate");

        BOOST_TEST_MESSAGE("--- success");

        paid_subscription_delete_operation psdop;
        psdop.author = "alice";
        psdop.oid = paid_subscription_id{"alicegram", "access", 1};
        GOLOS_CHECK_NO_THROW(psdop.validate());

        BOOST_TEST_MESSAGE("--- throw if wrong oid");
        psdop.oid = paid_subscription_id{"", "access", 1};
        GOLOS_CHECK_ERROR_PROPS(psdop.validate(),
            CHECK_ERROR(invalid_parameter, "app"));
    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(paid_subscription_transfer_validate) { try {
        BOOST_TEST_MESSAGE("Testing: paid_subscription_transfer_validate");

        BOOST_TEST_MESSAGE("--- success");

        paid_subscription_transfer_operation pstop;
        pstop.from = "alice";
        pstop.to = "alice";
        pstop.oid = paid_subscription_id{"alicegram", "access", 1};
        pstop.amount = asset(10000, STEEM_SYMBOL);
        GOLOS_CHECK_NO_THROW(pstop.validate());

        BOOST_TEST_MESSAGE("--- throw if wrong oid");
        pstop.oid = paid_subscription_id{"", "access", 1};
        GOLOS_CHECK_ERROR_PROPS(pstop.validate(),
            CHECK_ERROR(invalid_parameter, "app"));

        BOOST_TEST_MESSAGE("--- throw if negative");
        pstop.oid = paid_subscription_id{"alicegram", "access", 1};
        pstop.amount = asset(-10000, STEEM_SYMBOL);
        GOLOS_CHECK_ERROR_PROPS(pstop.validate(),
            CHECK_ERROR(invalid_parameter, "amount"));
    } FC_LOG_AND_RETHROW() }

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
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 1);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto& idx = _db.get_index<paid_subscriber_index, by_author_oid_subscriber>();
            auto ssingle = idx.find(std::make_tuple("alice", pscop.oid));
            BOOST_CHECK(ssingle != idx.end());
            BOOST_CHECK_EQUAL(ssingle->active, true);
            BOOST_CHECK_EQUAL(ssingle->subscriber, "bob");
            BOOST_CHECK_EQUAL(ssingle->prepaid.amount.value, 0);
            BOOST_CHECK_EQUAL(ssingle->next_payment, fc::time_point_sec(0));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("bob").tip_balance, asset(5000, STEEM_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").tip_balance, asset(15000, STEEM_SYMBOL));

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

        auto created = _db.head_block_time();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 2);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 2);

            const auto* dave = _db.find_paid_subscriber("dave", "alice", pscop.oid);
            BOOST_CHECK(dave != nullptr);
            BOOST_CHECK_EQUAL(dave->active, true);
            BOOST_CHECK_EQUAL(dave->prepaid.amount.value, 0);
            BOOST_CHECK_EQUAL(dave->next_payment, created + fc::seconds(30));

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(1510, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30));
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(7990, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("dave").sbd_balance, asset(0, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500, SBD_SYMBOL));

        BOOST_TEST_MESSAGE("-- Wait for next payment");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL - 2);

        {
            const auto* dave = _db.find_paid_subscriber("dave", "alice", pscop.oid);
            BOOST_CHECK(dave != nullptr);
            BOOST_CHECK_EQUAL(dave->active, true);
            BOOST_CHECK_EQUAL(dave->next_payment, created + fc::seconds(30));
        }

        validate_database();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 2);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* dave = _db.find_paid_subscriber("dave", "alice", pscop.oid);
            BOOST_CHECK(dave != nullptr);
            BOOST_CHECK_EQUAL(dave->active, false); // he paid for 1 interval
            BOOST_CHECK_EQUAL(dave->prepaid.amount.value, 0);
            BOOST_CHECK_EQUAL(dave->next_payment, fc::time_point_sec(0));

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(1010, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30 * 2));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(7990, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("dave").sbd_balance, asset(0, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500 + 500, SBD_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, #2");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 2);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(510, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30 * 3));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(7990, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500 + 500 + 500, SBD_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, #3");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 2);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30 * 4));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(7990, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500 + 500 + 500 + 510, SBD_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, (subscription end)");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", paid_subscription_id{"alicegram", "hugs", 1});
            BOOST_CHECK_EQUAL(pso.subscribers, 2);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 0);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, false);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, fc::time_point_sec(0));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(7990, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500 + 500 + 500 + 510, SBD_SYMBOL));

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

        auto updated = _db.head_block_time();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 2);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(10, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, updated + fc::seconds(30));
        }

        BOOST_TEST_MESSAGE("-- Wait for next payment");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL - 1);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 2);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(9, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, updated + fc::seconds(30 * 2));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(7990, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500 + 500 + 500 + 510, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").balance, asset(1, STEEM_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, #2");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 2);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(8, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, updated + fc::seconds(30 * 3));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("alice").balance, asset(2, STEEM_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, #3");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 2);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, updated + fc::seconds(30 * 4));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("alice").balance, asset(10, STEEM_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, end");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 2);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 0);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, false);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, fc::time_point_sec(0));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("alice").balance, asset(10, STEEM_SYMBOL));

        validate_database();

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(paid_subscriptions_no_prepaid) { try {
        BOOST_TEST_MESSAGE("Testing: paid_subscriptions_no_prepaid");

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

        auto created = _db.head_block_time();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 1);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30));
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(9500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500, SBD_SYMBOL));

        BOOST_TEST_MESSAGE("-- Wait for next payment");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL - 1);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 1);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30 * 2));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(9000, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500, SBD_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, #2");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 1);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30 * 3));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(8500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500 + 500, SBD_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, (subscription end)");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 1);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 0);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, false);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, fc::time_point_sec(0));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(8500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500 + 500, SBD_SYMBOL));

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
            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
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

        auto updated = _db.head_block_time();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 1);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, updated + fc::seconds(30));
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(8500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500 + 500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("carol").balance, asset(999, STEEM_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").balance, asset(1, STEEM_SYMBOL));

        BOOST_TEST_MESSAGE("-- Wait for next payment");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL - 1);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 1);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, updated + fc::seconds(30 * 2));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(8500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500 + 500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("carol").balance, asset(998, STEEM_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").balance, asset(2, STEEM_SYMBOL));

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
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 1);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, updated + fc::seconds(30 * 3));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(8490, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500 + 500 + 10, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("carol").balance, asset(998, STEEM_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").balance, asset(2, STEEM_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment, (subscription end)");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 1);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 0);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, false);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, fc::time_point_sec(0));
        }

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(8490, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500 + 500 + 10, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("carol").balance, asset(998, STEEM_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").balance, asset(2, STEEM_SYMBOL));

        validate_database();

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(paid_subscriptions_single_update) { try {
        BOOST_TEST_MESSAGE("Testing: paid_subscriptions_single_update");

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
        pscop.interval = 0;
        pscop.executions = 0;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, pscop));
        generate_block();

        BOOST_TEST_MESSAGE("-- Subscribe by carol");

        fund("carol", ASSET("10.000 GBG"));
        fund("carol", ASSET("1.000 GOLOS"));

        paid_subscription_transfer_operation pstop;
        pstop.from = "carol";
        pstop.to = "alice";
        pstop.oid = pscop.oid;
        pstop.amount = asset(500, SBD_SYMBOL);
        pstop.from_tip = false;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, pstop));

        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 1);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, fc::time_point_sec(0));
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(9500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500, SBD_SYMBOL));

        BOOST_TEST_MESSAGE("-- Update subscription, to 0.001 GOLOS");

        paid_subscription_update_operation psuop;
        psuop.author = "alice";
        psuop.oid = paid_subscription_id{"alicegram", "hugs", 1};
        psuop.cost = asset(1, STEEM_SYMBOL);
        psuop.tip_cost = false;
        psuop.interval = 0;
        psuop.executions = 0;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, psuop));
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 1);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 0);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, false);
            BOOST_CHECK_EQUAL(carol->cost, psuop.cost);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, fc::time_point_sec(0));
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Renew by carol");

        pstop.from = "carol";
        pstop.to = "alice";
        pstop.oid = pscop.oid;
        pstop.amount = asset(1, STEEM_SYMBOL);
        pstop.from_tip = false;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, pstop));

        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 1);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->cost, psuop.cost);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(0, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, fc::time_point_sec(0));
        }

        validate_database();
    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(paid_subscription_update) { try {
        BOOST_TEST_MESSAGE("Testing: paid_subscription_update");

        ACTORS((alice)(carol))
        generate_block();

        signed_transaction tx;

        BOOST_TEST_MESSAGE("-- Create periodic paid subscription by alice");

        paid_subscription_create_operation pscop;
        pscop.author = "alice";
        pscop.oid = paid_subscription_id{"alicegram", "hugs", 1};
        pscop.cost = asset(500, SBD_SYMBOL);
        pscop.tip_cost = false;
        pscop.allow_prepaid = true;
        pscop.interval = 30;
        pscop.executions = 3;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, pscop));
        generate_block();

        BOOST_TEST_MESSAGE("-- Subscribe by carol");

        fund("carol", ASSET("10.000 GBG"));

        paid_subscription_transfer_operation pstop;
        pstop.from = "carol";
        pstop.to = "alice";
        pstop.oid = pscop.oid;
        pstop.amount = asset(2010, SBD_SYMBOL);
        pstop.from_tip = false;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, pstop));

        auto created = _db.head_block_time();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        fc::time_point_sec next_payment;
        {
            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(1510, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30));
            next_payment = carol->next_payment;
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(7990, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500, SBD_SYMBOL));

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

        BOOST_TEST_MESSAGE("-- Increase prepayment by carol");

        fund("carol", ASSET("10.000 GOLOS"));

        pstop.from = "carol";
        pstop.to = "alice";
        pstop.oid = pscop.oid;
        pstop.amount = asset(10, STEEM_SYMBOL);
        pstop.from_tip = false;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, pstop));

        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(10, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, next_payment);
        }

        BOOST_TEST_MESSAGE("-- Wait for next payment");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL - 1);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(9, STEEM_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, next_payment + fc::seconds(30));
        }

        BOOST_TEST_MESSAGE("-- Check balances (incl. check GBG moneyback)");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(9500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("carol").balance, asset(9990, STEEM_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").balance, asset(1, STEEM_SYMBOL));

        validate_database();

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(paid_subscription_inactive) { try {
        BOOST_TEST_MESSAGE("Testing: paid_subscription_inactive");

        ACTORS((alice)(carol))
        generate_block();

        signed_transaction tx;

        BOOST_TEST_MESSAGE("-- Create periodic paid subscription by alice");

        paid_subscription_create_operation pscop;
        pscop.author = "alice";
        pscop.oid = paid_subscription_id{"alicegram", "hugs", 1};
        pscop.cost = asset(500, SBD_SYMBOL);
        pscop.tip_cost = false;
        pscop.allow_prepaid = true;
        pscop.interval = 30;
        pscop.executions = 10;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, pscop));
        generate_block();

        BOOST_TEST_MESSAGE("-- Subscribe by carol");

        fund("carol", ASSET("2.020 GBG"));

        paid_subscription_transfer_operation pstop;
        pstop.from = "carol";
        pstop.to = "alice";
        pstop.oid = pscop.oid;
        pstop.amount = asset(2010, SBD_SYMBOL);
        pstop.from_tip = false;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, pstop));

        generate_block();

        BOOST_TEST_MESSAGE("-- Wait for next payment");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL - 1);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(1010, SBD_SYMBOL));
        }

        BOOST_TEST_MESSAGE("-- Wait for next payment #2");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(510, SBD_SYMBOL));
        }

        BOOST_TEST_MESSAGE("-- Wait for next payment #3");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(10, SBD_SYMBOL));
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait for next payment #4");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, false);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(10, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, fc::time_point_sec(0));
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Wait another interval (nothing should happen)");

        generate_blocks(30 / STEEMIT_BLOCK_INTERVAL);

        validate_database();

        BOOST_TEST_MESSAGE("-- Prolong subscription");

        fund("carol", ASSET("2.010 GBG"));

        pstop.from = "carol";
        pstop.to = "alice";
        pstop.oid = pscop.oid;
        pstop.amount = asset(2010, SBD_SYMBOL);
        pstop.from_tip = false;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, pstop));

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(paid_subscription_cancel_delete) { try {
        BOOST_TEST_MESSAGE("Testing: paid_subscription_cancel_delete");

        ACTORS((alice)(carol)(dave))
        generate_block();

        signed_transaction tx;

        BOOST_TEST_MESSAGE("-- Create periodic paid subscription by alice");

        paid_subscription_create_operation pscop;
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
        fund("dave", ASSET("10.000 GBG"));

        paid_subscription_transfer_operation pstop;
        pstop.from = "carol";
        pstop.to = "alice";
        pstop.oid = pscop.oid;
        pstop.amount = asset(2000, SBD_SYMBOL);
        pstop.from_tip = false;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, pstop));

        pstop.from = "dave";
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, dave_private_key, pstop));

        auto created = _db.head_block_time();
        generate_block();

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 2);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 2);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol != nullptr);
            BOOST_CHECK_EQUAL(carol->active, true);
            BOOST_CHECK_EQUAL(carol->prepaid, asset(1500, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(carol->next_payment, created + fc::seconds(30));

            const auto* dave = _db.find_paid_subscriber("dave", "alice", pscop.oid);
            BOOST_CHECK(dave != nullptr);
            BOOST_CHECK_EQUAL(dave->active, true);
            BOOST_CHECK_EQUAL(dave->prepaid, asset(1500, SBD_SYMBOL));
            BOOST_CHECK_EQUAL(dave->next_payment, created + fc::seconds(30));
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Check balances");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(8000, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("dave").sbd_balance, asset(8000, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500, SBD_SYMBOL));

        validate_database();

        BOOST_TEST_MESSAGE("-- Cancel subscription by carol");

        paid_subscription_cancel_operation pscaop;
        pscaop.subscriber = "carol";
        pscaop.author = "alice",
        pscaop.oid = pscop.oid;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, pscaop));

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto& pso = _db.get_paid_subscription("alice", pscop.oid);
            BOOST_CHECK_EQUAL(pso.subscribers, 1);
            BOOST_CHECK_EQUAL(pso.active_subscribers, 1);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol == nullptr);

            const auto* dave = _db.find_paid_subscriber("dave", "alice", pscop.oid);
            BOOST_CHECK(dave != nullptr);
            BOOST_CHECK_EQUAL(dave->active, true);
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Check balances (carol should receive 1.500 GBG moneyback)");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(9500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("dave").sbd_balance, asset(8000, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500, SBD_SYMBOL));

        BOOST_TEST_MESSAGE("-- Delete subscription by alice");

        paid_subscription_delete_operation psdop;
        psdop.author = "alice";
        psdop.oid = pscop.oid;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, psdop));

        BOOST_TEST_MESSAGE("-- Check subscriptions");

        {
            const auto* pso = _db.find_paid_subscription("alice", pscop.oid);
            BOOST_CHECK(pso == nullptr);

            const auto* carol = _db.find_paid_subscriber("carol", "alice", pscop.oid);
            BOOST_CHECK(carol == nullptr);

            const auto* dave = _db.find_paid_subscriber("dave", "alice", pscop.oid);
            BOOST_CHECK(dave == nullptr);
        }

        validate_database();

        BOOST_TEST_MESSAGE("-- Check balances (carol and dave should receive moneyback)");

        BOOST_CHECK_EQUAL(_db.get_account("carol").sbd_balance, asset(9500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("dave").sbd_balance, asset(9500, SBD_SYMBOL));
        BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, asset(500 + 500, SBD_SYMBOL));

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(nft_collection) { try {
        BOOST_TEST_MESSAGE("Testing: nft_collection");

        ACTORS((alice))
        generate_block();

        signed_transaction tx;

        BOOST_TEST_MESSAGE("-- Check wrong collection name");

        nft_collection_operation ncop;
        ncop.creator = "alice";
        ncop.name = "ABCDEFGHJKLMNOPQR";
        ncop.json_metadata = "{}";
        ncop.max_token_count = 4294967295;
        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, ncop),
            CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "name")));

        BOOST_TEST_MESSAGE("-- Check wrong collection name #2");

        ncop.name = " COOLCAT";
        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, ncop),
            CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "name")));

        BOOST_TEST_MESSAGE("-- Check wrong collection name #3");

        ncop.name = "COOLCAT ";
        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, ncop),
            CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "name")));

        BOOST_TEST_MESSAGE("-- Check wrong collection name #4");

        ncop.name = "\tCOOLCAT";
        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, ncop),
            CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "name")));

        BOOST_TEST_MESSAGE("-- Check wrong collection name #5");

        ncop.name = "\0COOLCAT";
        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, ncop),
            CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "name")));

        BOOST_TEST_MESSAGE("-- Check wrong max_token_count");

        ncop.name = "COOLCAT";
        ncop.max_token_count = 0;
        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, ncop),
            CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "max_token_count")));

        BOOST_TEST_MESSAGE("-- Check wrong json_metadata");

        ncop.max_token_count = 5;
        ncop.json_metadata = "{";
        GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, ncop),
            CHECK_ERROR(tx_invalid_operation, 0,
                    CHECK_ERROR(invalid_parameter, "json_metadata")));

        BOOST_TEST_MESSAGE("-- Check correct way");

        ncop.json_metadata = "";
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, ncop));

        validate_database();

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(nft_test) { try {
        BOOST_TEST_MESSAGE("Testing: nft_test");

        ACTORS((alice)(bob)(carol))
        generate_block();

        signed_transaction tx;

        BOOST_TEST_MESSAGE("-- Create NFT collection");

        nft_collection_operation ncop;
        ncop.creator = "alice";
        ncop.name = "COOLGAME";
        ncop.json_metadata = "{}";
        ncop.max_token_count = 4294967295;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, ncop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Delete NFT collection");

        nft_collection_delete_operation ncdop;
        ncdop.creator = "alice";
        ncdop.name = "COOLGAME";
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, ncdop));

        validate_database();

        generate_block();

        BOOST_TEST_MESSAGE("-- Create NFT collection again");

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, ncop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Issue NFT");

        fund("alice", ASSET("20.000 GBG"));

        nft_issue_operation niop;
        niop.creator = "alice";
        niop.name = "COOLGAME";
        niop.to = "bob";
        niop.json_metadata = "{\"health\":10}";

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, niop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Transfer NFT to carol");

        nft_transfer_operation ntop;
        ntop.from = "bob";
        ntop.to = "carol";
        ntop.token_id = 1;
        ntop.memo = "";

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, ntop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Sell NFT");

        nft_sell_operation nsop;
        nsop.seller = "carol";
        nsop.token_id = 1;
        nsop.buyer = "";
        nsop.price = asset(1000, SBD_SYMBOL);
        nsop.order_id = 2;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, nsop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Cancel NFT order");

        nft_cancel_order_operation ncoop;
        ncoop.owner = "carol";
        ncoop.order_id = 2;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, ncoop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Sell NFT again");

        generate_block();

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, nsop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Buy NFT");

        fund("bob", ASSET("1.000 GBG"));

        nft_buy_operation nbop;
        nbop.buyer = "bob";
        nbop.name = "";
        nbop.token_id = 1;
        nbop.order_id = 2;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, nbop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Transfer NFT to null - burn it");

        nft_transfer_operation ntop2;
        ntop2.from = "bob";
        ntop2.to = "null";
        ntop2.token_id = 1;
        ntop2.memo = "";

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, ntop2));

        validate_database();

    } FC_LOG_AND_RETHROW() }

    BOOST_AUTO_TEST_CASE(nft_test_buy_first) { try {
        BOOST_TEST_MESSAGE("Testing: nft_test_buy_first");

        ACTORS((alice)(bob)(carol))
        generate_block();

        signed_transaction tx;

        BOOST_TEST_MESSAGE("-- Create NFT collection");

        nft_collection_operation ncop;
        ncop.creator = "alice";
        ncop.name = "COOLGAME";
        ncop.json_metadata = "{}";
        ncop.max_token_count = 4294967295;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, ncop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Delete NFT collection");

        nft_collection_delete_operation ncdop;
        ncdop.creator = "alice";
        ncdop.name = "COOLGAME";
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, ncdop));

        validate_database();

        generate_block();

        BOOST_TEST_MESSAGE("-- Create NFT collection again");

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, ncop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Issue NFT");

        fund("alice", ASSET("20.000 GBG"));

        nft_issue_operation niop;
        niop.creator = "alice";
        niop.name = "COOLGAME";
        niop.to = "bob";
        niop.json_metadata = "{\"health\":10}";

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, niop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Transfer NFT to carol");

        nft_transfer_operation ntop;
        ntop.from = "bob";
        ntop.to = "carol";
        ntop.token_id = 1;
        ntop.memo = "";

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, ntop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Buy NFT");

        nft_buy_operation nbop;
        nbop.buyer = "bob";
        nbop.name = "COOLGAME";
        nbop.price = asset(2000, SBD_SYMBOL);
        nbop.order_id = 2;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, nbop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Sell NFT");

        fund("bob", ASSET("2.000 GBG"));

        nft_sell_operation nsop;
        nsop.seller = "carol";
        nsop.token_id = 1;
        nsop.buyer = "bob";
        nsop.price = asset(1000, SBD_SYMBOL);
        nsop.order_id = 2;

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, nsop));

        validate_database();

        BOOST_TEST_MESSAGE("-- Transfer NFT to null - burn it");

        nft_transfer_operation ntop2;
        ntop2.from = "bob";
        ntop2.to = "null";
        ntop2.token_id = 1;
        ntop2.memo = "";

        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, ntop2));

        validate_database();

    } FC_LOG_AND_RETHROW() }
BOOST_AUTO_TEST_SUITE_END()
