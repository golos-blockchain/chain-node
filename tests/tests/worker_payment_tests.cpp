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

BOOST_AUTO_TEST_CASE(worker_authorities) {
    BOOST_TEST_MESSAGE("Testing: worker_authorities");

    {
        worker_fund_operation op;
        op.sponsor = "alice";
        op.amount = ASSET_GOLOS(10);
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"alice"}));
    }
}

BOOST_AUTO_TEST_CASE(worker_fund_filling) {
    BOOST_TEST_MESSAGE("Testing: worker_fund_filling");

    generate_block();

    auto old_fund = db->get_dynamic_global_properties().total_worker_fund_steem;

    comment_fund fund(*db);

    generate_block();

    const auto& gpo = db->get_dynamic_global_properties();
    BOOST_CHECK_EQUAL(gpo.total_worker_fund_steem, fund.worker_fund());
}

BOOST_AUTO_TEST_CASE(worker_fund_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_fund_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_fund_operation op;
    op.sponsor = "alice";
    op.amount = ASSET_GOLOS(10);
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect account case");

    CHECK_PARAM_INVALID(op, sponsor, "");

    BOOST_TEST_MESSAGE("-- Invalid symbol cases");

    CHECK_PARAM_INVALID(op, amount, ASSET_GBG(10));
    CHECK_PARAM_INVALID(op, amount, ASSET_GESTS(10));

    BOOST_TEST_MESSAGE("-- Invalid amount cases");

    CHECK_PARAM_INVALID(op, amount, ASSET_GOLOS(0));
    CHECK_PARAM_INVALID(op, amount, ASSET_GOLOS(-10));
}

BOOST_AUTO_TEST_CASE(worker_fund_apply) {
    BOOST_TEST_MESSAGE("Testing: worker_fund_apply");

    signed_transaction tx;

    ACTORS((alice))
    fund("alice", 15000);

    BOOST_TEST_MESSAGE("-- Normal funding");

    const auto init_balance = db->get_balance("alice", STEEM_SYMBOL);
    const auto init_fund = db->get_dynamic_global_properties().total_worker_fund_steem;

    generate_block();
    const auto block_add = db->get_dynamic_global_properties().total_worker_fund_steem - init_fund;

    worker_fund_operation op;
    op.sponsor = "alice";
    op.amount = ASSET_GOLOS(10);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, op));
    generate_block();

    BOOST_CHECK_EQUAL(db->get_balance("alice", STEEM_SYMBOL), init_balance - op.amount);
    BOOST_CHECK_EQUAL(db->get_dynamic_global_properties().total_worker_fund_steem, init_fund + block_add*2 + op.amount);

    BOOST_TEST_MESSAGE("-- Trying fund more than have");

    op.amount = db->get_balance("alice", STEEM_SYMBOL);
    op.amount += ASSET_GOLOS(1);
    GOLOS_CHECK_ERROR_PROPS(push_tx_with_ops(tx, alice_private_key, op),
        CHECK_ERROR(tx_invalid_operation, 0,
            CHECK_ERROR(insufficient_funds, "alice", "fund", op.amount)));
}

BOOST_AUTO_TEST_SUITE_END()
