#include <boost/test/unit_test.hpp>

#include "worker_fixture.hpp"
#include "helpers.hpp"
#include "comment_reward.hpp"

#include <golos/protocol/worker_operations.hpp>
#include <golos/chain/worker_objects.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::chain;

BOOST_FIXTURE_TEST_SUITE(worker_payment_tests, worker_fixture)

BOOST_AUTO_TEST_CASE(worker_fund_emiting) {
    BOOST_TEST_MESSAGE("Testing: worker_fund_emiting");

    generate_block();

    auto old_fund = _db.get_account(STEEMIT_WORKER_POOL_ACCOUNT).balance;

    comment_fund fund(*db);

    generate_block();

    BOOST_CHECK_EQUAL(_db.get_account(STEEMIT_WORKER_POOL_ACCOUNT).balance, fund.worker_fund());
    BOOST_CHECK_GT(_db.get_account(STEEMIT_WORKER_POOL_ACCOUNT).balance, old_fund);
}

BOOST_AUTO_TEST_CASE(worker_fund_transfering) {
    BOOST_TEST_MESSAGE("Testing: worker_fund_transfering");

    signed_transaction tx;

    ACTORS_OLD((alice))
    fund("alice", 15000);

    BOOST_TEST_MESSAGE("-- Normal funding");

    const auto init_balance = _db.get_balance("alice", STEEM_SYMBOL);
    const auto init_fund = _db.get_account(STEEMIT_WORKER_POOL_ACCOUNT).balance;

    generate_block();
    const auto block_add = _db.get_account(STEEMIT_WORKER_POOL_ACCOUNT).balance - init_fund;

    transfer_operation op;
    op.from = "alice";
    op.to = "workers";
    op.amount = ASSET_GOLOS(10);
    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    BOOST_CHECK_EQUAL(_db.get_balance("alice", STEEM_SYMBOL), init_balance - op.amount);
    BOOST_CHECK_EQUAL(_db.get_account(STEEMIT_WORKER_POOL_ACCOUNT).balance, init_fund + block_add*2 + op.amount);
}

BOOST_AUTO_TEST_CASE(worker_request_payment) {
    BOOST_TEST_MESSAGE("Testing: worker_request_payment");

    ACTORS_OLD((alice)(bob)(carol)(dave)(frad))
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Funding GBG");
    fund("alice", ASSET_GBG(1010));

    transfer_operation transfer_op;
    transfer_op.from = "alice";
    transfer_op.to = "workers";
    transfer_op.amount = ASSET_GBG(1000);
    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, transfer_op));
    generate_block();

    BOOST_CHECK_EQUAL(_db.get_account(STEEMIT_WORKER_POOL_ACCOUNT).sbd_balance, ASSET_GBG(1000));
    BOOST_CHECK_EQUAL(_db.get_account("alice").sbd_balance, ASSET_GBG(10));

    BOOST_TEST_MESSAGE("-- Creating bob request in same block");

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    fund("bob", ASSET_GBG(100));
    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.worker = "bob";
    wtop.required_amount_min = ASSET_GBG(6);
    wtop.required_amount_max = ASSET_GBG(60);
    wtop.duration = fc::days(5).to_seconds();
    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));

    BOOST_TEST_MESSAGE("-- Creating carol request in same block");

    comment_create("carol", carol_private_key, "carol-request", "", "carol-request");

    fund("carol", ASSET_GBG(100));
    wtop.author = "carol";
    wtop.permlink = "carol-request";
    wtop.worker = "carol";
    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Creating dave request in same block");

    comment_create("dave", dave_private_key, "dave-request", "", "dave-request");

    fund("dave", ASSET_GBG(100));
    wtop.author = "dave";
    wtop.permlink = "dave-request";
    wtop.worker = "dave";
    wtop.duration = fc::days(30).to_seconds();
    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, dave_private_key, wtop));

    BOOST_TEST_MESSAGE("-- Creating frad request in same block");

    comment_create("frad", frad_private_key, "frad-request", "", "frad-request");

    fund("frad", ASSET_GBG(100));
    wtop.author = "frad";
    wtop.permlink = "frad-request";
    wtop.worker = "frad";
    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, frad_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Upvoting requests by enough stake-holders");

    worker_request_vote_operation op;
    auto upvote_request = [&](account_name_type author, std::string permlink, int16_t vote_percent) {
        op.voter = "bob";
        op.author = author;
        op.permlink = permlink;
        op.vote_percent = vote_percent;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
        op.voter = "carol";
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, op));
        op.voter = "alice";
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    };
    upvote_request("bob", "bob-request", STEEMIT_100_PERCENT);
    upvote_request("carol", "carol-request", STEEMIT_100_PERCENT);
    upvote_request("dave", "dave-request", STEEMIT_100_PERCENT);
    upvote_request("frad", "frad-request", STEEMIT_100_PERCENT);

    generate_blocks(_db.get_comment_by_perm("bob", string("bob-request")).created
        + wtop.duration, true);

    BOOST_TEST_MESSAGE("-- Checking bob and carol requests approved");

    {
        const auto& wro = _db.get_worker_request(_db.get_comment_by_perm("bob", string("bob-request")).id);
        BOOST_CHECK_EQUAL(wro.state, worker_request_state::payment);
        BOOST_CHECK_EQUAL(wro.remaining_payment, wro.required_amount_max);
    }
    {
        const auto& wro = _db.get_worker_request(_db.get_comment_by_perm("carol", string("carol-request")).id);
        BOOST_CHECK_EQUAL(wro.state, worker_request_state::payment);
        BOOST_CHECK_EQUAL(wro.remaining_payment, wro.required_amount_max);
    }

    BOOST_TEST_MESSAGE("-- Waiting for payment");

    generate_blocks(GOLOS_WORKER_CASHOUT_INTERVAL);

    BOOST_CHECK_EQUAL(_db.get_balance("bob", SBD_SYMBOL), ASSET_GBG(60));
    {
        const auto& wro = _db.get_worker_request(_db.get_comment_by_perm("bob", string("bob-request")).id);
        BOOST_CHECK_EQUAL(wro.state, worker_request_state::payment_complete);
    }
    BOOST_CHECK_EQUAL(_db.get_balance("carol", SBD_SYMBOL), ASSET_GBG(60));
    {
        const auto& wro = _db.get_worker_request(_db.get_comment_by_perm("carol", string("carol-request")).id);
        BOOST_CHECK_EQUAL(wro.state, worker_request_state::payment_complete);
    }
}

BOOST_AUTO_TEST_CASE(worker_request_payment_vests) {
    BOOST_TEST_MESSAGE("Testing: worker_request_payment_vests");

    ACTORS_OLD((alice)(bob)(carol)(dave)(frad))
    auto emission_per_block = _db.get_balance(STEEMIT_WORKER_POOL_ACCOUNT, STEEM_SYMBOL);
    generate_block();
    emission_per_block = _db.get_balance(STEEMIT_WORKER_POOL_ACCOUNT, STEEM_SYMBOL) - emission_per_block;

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Creating bob request in same block");

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    fund("bob", ASSET_GBG(100));
    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.worker = "bob";
    wtop.required_amount_min = ASSET_GOLOS(6);
    wtop.required_amount_max = ASSET_GOLOS(6);
    wtop.duration = fc::days(5).to_seconds();
    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));

    BOOST_TEST_MESSAGE("-- Creating carol request in same block");

    comment_create("carol", carol_private_key, "carol-request", "", "carol-request");

    fund("carol", ASSET_GBG(100));
    wtop.author = "carol";
    wtop.permlink = "carol-request";
    wtop.worker = "carol";
    wtop.vest_reward = true;
    GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Upvoting requests by enough stake-holders");

    worker_request_vote_operation op;
    auto upvote_request = [&](account_name_type author, std::string permlink, int16_t vote_percent) {
        op.voter = "bob";
        op.author = author;
        op.permlink = permlink;
        op.vote_percent = vote_percent;
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
        op.voter = "carol";
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, op));
        op.voter = "alice";
        GOLOS_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    };
    upvote_request("bob", "bob-request", STEEMIT_100_PERCENT);
    upvote_request("carol", "carol-request", STEEMIT_100_PERCENT);

    generate_blocks(_db.get_comment_by_perm("bob", string("bob-request")).created
        + wtop.duration, true);

    BOOST_TEST_MESSAGE("-- Checking bob and carol requests approved");

    {
        const auto& wro = _db.get_worker_request(_db.get_comment_by_perm("bob", string("bob-request")).id);
        BOOST_CHECK_EQUAL(wro.state, worker_request_state::payment);
        BOOST_CHECK_EQUAL(wro.remaining_payment, wro.required_amount_max);
    }
    {
        const auto& wro = _db.get_worker_request(_db.get_comment_by_perm("carol", string("carol-request")).id);
        BOOST_CHECK_EQUAL(wro.state, worker_request_state::payment);
        BOOST_CHECK_EQUAL(wro.remaining_payment, wro.required_amount_max);
    }

    BOOST_TEST_MESSAGE("-- Waiting for payment");

    auto funds_need = ASSET_GOLOS(12) - _db.get_balance(STEEMIT_WORKER_POOL_ACCOUNT, STEEM_SYMBOL);
    auto blocks_need = _db.head_block_num() + funds_need.amount.value / emission_per_block.amount.value;
    auto actual_blocks_need = blocks_need / GOLOS_WORKER_CASHOUT_INTERVAL * GOLOS_WORKER_CASHOUT_INTERVAL;
    if (actual_blocks_need != blocks_need)
        actual_blocks_need += GOLOS_WORKER_CASHOUT_INTERVAL;
    generate_blocks(actual_blocks_need);

    BOOST_CHECK_EQUAL(_db.get_balance("bob", STEEM_SYMBOL), ASSET_GOLOS(6));
    {
        const auto& wro = _db.get_worker_request(_db.get_comment_by_perm("bob", string("bob-request")).id);
        BOOST_CHECK_EQUAL(wro.state, worker_request_state::payment_complete);
    }
    BOOST_CHECK_EQUAL(_db.get_balance("carol", STEEM_SYMBOL).amount, 0);
    {
        const auto& wro = _db.get_worker_request(_db.get_comment_by_perm("carol", string("carol-request")).id);
        BOOST_CHECK_EQUAL(wro.state, worker_request_state::payment_complete);
    }
}

BOOST_AUTO_TEST_SUITE_END()
