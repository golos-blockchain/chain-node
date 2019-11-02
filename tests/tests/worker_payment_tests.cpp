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

    auto old_fund = db->get_account(STEEMIT_WORKER_POOL_ACCOUNT).balance;

    comment_fund fund(*db);

    generate_block();

    BOOST_CHECK_EQUAL(db->get_account(STEEMIT_WORKER_POOL_ACCOUNT).balance, fund.worker_fund());
    BOOST_CHECK_GT(db->get_account(STEEMIT_WORKER_POOL_ACCOUNT).balance, old_fund);
}

BOOST_AUTO_TEST_CASE(worker_fund_transfering) {
    BOOST_TEST_MESSAGE("Testing: worker_fund_transfering");

    signed_transaction tx;

    ACTORS((alice))
    fund("alice", 15000);

    BOOST_TEST_MESSAGE("-- Normal funding");

    const auto init_balance = db->get_balance("alice", STEEM_SYMBOL);
    const auto init_fund = db->get_account(STEEMIT_WORKER_POOL_ACCOUNT).balance;

    generate_block();
    const auto block_add = db->get_account(STEEMIT_WORKER_POOL_ACCOUNT).balance - init_fund;

    transfer_operation op;
    op.from = "alice";
    op.to = "workers";
    op.amount = ASSET_GOLOS(10);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    BOOST_CHECK_EQUAL(db->get_balance("alice", STEEM_SYMBOL), init_balance - op.amount);
    BOOST_CHECK_EQUAL(db->get_account(STEEMIT_WORKER_POOL_ACCOUNT).balance, init_fund + block_add*2 + op.amount);
}

BOOST_AUTO_TEST_SUITE_END()
