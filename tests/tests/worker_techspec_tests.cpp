#include <boost/test/unit_test.hpp>

#include "worker_fixture.hpp"
#include "helpers.hpp"

#include <golos/protocol/worker_operations.hpp>
#include <golos/chain/worker_objects.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::chain;

BOOST_FIXTURE_TEST_SUITE(worker_techspec_tests, worker_fixture)

BOOST_AUTO_TEST_CASE(worker_authorities) {
    BOOST_TEST_MESSAGE("Testing: worker_authorities");

    {
        worker_techspec_operation op;
        op.author = "bob";
        op.permlink = "bob-techspec";
        op.specification_cost = ASSET_GOLOS(6000);
        op.development_cost = ASSET_GOLOS(60000);
        op.payments_interval = 60;
        op.payments_count = 2;
        op.worker = "bob";
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"bob"}));
    }

    {
        worker_techspec_delete_operation op;
        op.author = "bob";
        op.permlink = "bob-techspec";
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"bob"}));
    }

    {
        worker_techspec_approve_operation op;
        op.approver = "cyberfounder";
        op.author = "bob";
        op.permlink = "bob-techspec";
        op.state = worker_techspec_approve_state::approve;
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"cyberfounder"}));
    }
}

BOOST_AUTO_TEST_CASE(worker_techspec_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_techspec_operation op;
    op.author = "bob";
    op.permlink = "techspec-permlink";
    op.specification_cost = ASSET_GOLOS(6000);
    op.development_cost = ASSET_GOLOS(60000);
    op.payments_interval = 60*60*24;
    op.payments_count = 2;
    op.worker = "bob";
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect account or permlink case");

    CHECK_PARAM_INVALID(op, author, "");
    CHECK_PARAM_INVALID(op, permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));

    BOOST_TEST_MESSAGE("-- Incorrect worker");

    CHECK_PARAM_INVALID(op, worker, "-");

    BOOST_TEST_MESSAGE("-- No worker");

    CHECK_PARAM_INVALID(op, worker, "");

    BOOST_TEST_MESSAGE("-- Non-GOLOS cost case");

    CHECK_PARAM_INVALID(op, specification_cost, ASSET_GBG(6000));
    CHECK_PARAM_INVALID(op, specification_cost, ASSET_GESTS(6000));
    CHECK_PARAM_INVALID(op, development_cost, ASSET_GBG(60000));
    CHECK_PARAM_INVALID(op, development_cost, ASSET_GESTS(60000));

    BOOST_TEST_MESSAGE("-- Negative cost case");

    CHECK_PARAM_INVALID(op, specification_cost, ASSET_GOLOS(-1));
    CHECK_PARAM_INVALID(op, development_cost, ASSET_GOLOS(-1));

    BOOST_TEST_MESSAGE("-- Zero payments count case");

    CHECK_PARAM_INVALID(op, payments_count, 0);

    BOOST_TEST_MESSAGE("-- Too low payments interval case");

    CHECK_PARAM_INVALID(op, payments_interval, 60*60*24 - 1);

    BOOST_TEST_MESSAGE("-- Single payment with too big interval case");

    op.payments_count = 1;
    CHECK_PARAM_INVALID(op, payments_interval, 60*60*24 + 1);

    BOOST_TEST_MESSAGE("-- Single payment with normal interval case");

    op.payments_count = 1;
    CHECK_PARAM_VALID(op, payments_interval, 60*60*24);
}

BOOST_AUTO_TEST_CASE(worker_techspec_apply_create) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_apply_create");

    ACTORS((alice)(bob)(carol)(dave)(eve)(fred)(greta))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Create worker techspec with no post case");

    worker_techspec_operation op;
    op.author = "bob";
    op.permlink = "bob-techspec";
    op.specification_cost = ASSET_GOLOS(6);
    op.development_cost = ASSET_GOLOS(60);
    op.payments_interval = 60*60*24*2;
    op.payments_count = 2;
    op.worker = "bob";
    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("bob", "bob-techspec"), bob_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Create worker techspec on comment instead of post case");

    comment_create("alice", alice_private_key, "the-post", "", "the-post");
    comment_create("carol", carol_private_key, "i-am-comment", "alice", "the-post");

    op.author = "carol";
    op.permlink = "i-am-comment";
    GOLOS_CHECK_ERROR_LOGIC(post_is_not_root, carol_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Creating techspec with not-exist worker");

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    op.author = "bob";
    op.permlink = "bob-techspec";
    op.worker = "notexistacc";
    GOLOS_CHECK_ERROR_MISSING(account, "notexistacc", bob_private_key, op);

    BOOST_TEST_MESSAGE("-- Normal create worker techspec case");

    op.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    const auto& wto = db->get_worker_techspec(wto_post.id);
    BOOST_CHECK_EQUAL(wto.post, wto_post.id);
    BOOST_CHECK(wto.state == worker_techspec_state::created);
    BOOST_CHECK_EQUAL(wto.specification_cost, op.specification_cost);
    BOOST_CHECK_EQUAL(wto.development_cost, op.development_cost);
    BOOST_CHECK_EQUAL(wto.payments_count, op.payments_count);
    BOOST_CHECK_EQUAL(wto.payments_interval, op.payments_interval);
    BOOST_CHECK_EQUAL(wto.worker, op.worker);

    BOOST_CHECK_EQUAL(wto.next_cashout_time, fc::time_point_sec::maximum());
    BOOST_CHECK_EQUAL(wto.finished_payments_count, 0);

    {
        BOOST_TEST_MESSAGE("-- Check cannot create worker techspec on post outside cashout window");

        comment_create("greta", greta_private_key, "greta-techspec", "", "greta-techspec");

        generate_blocks(db->head_block_time() + STEEMIT_CASHOUT_WINDOW_SECONDS + STEEMIT_BLOCK_INTERVAL, true);

        op.author = "greta";
        op.permlink = "greta-techspec";
        GOLOS_CHECK_ERROR_LOGIC(post_should_be_in_cashout_window, greta_private_key, op);
    }

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_techspec_apply_modify) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_apply_modify");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation op;
    op.author = "bob";
    op.permlink = "bob-techspec";
    op.specification_cost = ASSET_GOLOS(6);
    op.development_cost = ASSET_GOLOS(60);
    op.payments_interval = 60*60*24*2;
    op.payments_count = 2;
    op.worker = "alice";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    BOOST_TEST_MESSAGE("-- Modify payments_count and payments_interval");

    op.payments_interval = 60*60*24*2;
    op.payments_count = 2;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    {
        const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
        const auto& wto = db->get_worker_techspec(wto_post.id);
        BOOST_CHECK_EQUAL(wto.payments_count, op.payments_count);
        BOOST_CHECK_EQUAL(wto.payments_interval, op.payments_interval);
    }

    BOOST_TEST_MESSAGE("-- Modify payments_count and payments_interval");

    op.specification_cost = ASSET_GOLOS(7);
    op.development_cost = ASSET_GOLOS(70);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    {
        const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
        const auto& wto = db->get_worker_techspec(wto_post.id);
        BOOST_CHECK_EQUAL(wto.specification_cost, op.specification_cost);
        BOOST_CHECK_EQUAL(wto.development_cost, op.development_cost);
    }

    BOOST_TEST_MESSAGE("-- Change worker");

    op.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    {
        const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
        const auto& wto = db->get_worker_techspec(wto_post.id);
        BOOST_CHECK_EQUAL(wto.worker, "bob");
    }

    BOOST_TEST_MESSAGE("-- Check cannot modify approved techspec");

    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        const auto name = "approver" + std::to_string(i);
        worker_techspec_approve_operation wtaop;
        wtaop.approver = name;
        wtaop.author = "bob";
        wtaop.permlink = "bob-techspec";
        wtaop.state = worker_techspec_approve_state::approve;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, wtaop));
        generate_block();
    }

    op.development_cost = ASSET_GOLOS(50);
    GOLOS_CHECK_ERROR_LOGIC(incorrect_techspec_state, bob_private_key, op);

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_techspec_approve_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_approve_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_techspec_approve_operation op;
    op.approver = "cyberfounder";
    op.author = "bob";
    op.permlink = "techspec-permlink";
    op.state = worker_techspec_approve_state::approve;
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect account or permlink case");

    CHECK_PARAM_INVALID(op, approver, "");
    CHECK_PARAM_INVALID(op, author, "");
    CHECK_PARAM_INVALID(op, permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));

    BOOST_TEST_MESSAGE("-- Invalid state case");

    CHECK_PARAM_INVALID(op, state, worker_techspec_approve_state::_size);
}

BOOST_AUTO_TEST_CASE(worker_techspec_approve_checking_funds) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_approve_checking_funds");

    ACTORS((alice)(bob))
    auto private_key = create_approvers(0, 1);
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Measuring revenue per block");

    const auto before = db->get_dynamic_global_properties().total_worker_fund_steem;
    generate_blocks(1);
    const auto revenue = db->get_dynamic_global_properties().total_worker_fund_steem - before;

    BOOST_TEST_MESSAGE("-- Creating techspec with cost a bit larger than revenue");

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.specification_cost = db->get_dynamic_global_properties().total_worker_fund_steem;
    wtop.development_cost = revenue * (60*60*37*2 / STEEMIT_BLOCK_INTERVAL + 1);
    wtop.payments_interval = 60*60*37;
    wtop.payments_count = 2;
    wtop.worker = "alice";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));

    BOOST_TEST_MESSAGE("-- Checking cannot approve");

    worker_techspec_approve_operation op;
    op.approver = "approver0";
    op.author = "bob";
    op.permlink = "bob-techspec";
    op.state = worker_techspec_approve_state::approve;
    GOLOS_CHECK_ERROR_LOGIC(insufficient_funds_to_approve, private_key, op);

    BOOST_TEST_MESSAGE("-- Editing it to be equal to revenue");

    wtop.development_cost = revenue * (60*60*36*2 / STEEMIT_BLOCK_INTERVAL);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));

    BOOST_TEST_MESSAGE("-- Checking can approve now");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
}

BOOST_AUTO_TEST_CASE(worker_techspec_approve_apply_combinations) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_approve_apply_combinations");

    ACTORS((alice)(bob))
    auto private_key = create_approvers(0, 1);
    generate_block();

    signed_transaction tx;

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24*2;
    wtop.payments_count = 2;
    wtop.worker = "alice";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    BOOST_TEST_MESSAGE("-- Abstaining non-voted techspec case");

    worker_techspec_approve_operation op;
    op.approver = "approver0";
    op.author = "bob";
    op.permlink = "bob-techspec";
    op.state = worker_techspec_approve_state::abstain;
    GOLOS_CHECK_ERROR_LOGIC(already_voted_in_similar_way, private_key, op);

    auto check_approves = [&](int approve_count, int disapprove_count) {
        auto approves = db->count_worker_techspec_approves(db->get_comment("bob", string("bob-techspec")).id);
        BOOST_CHECK_EQUAL(approves[worker_techspec_approve_state::approve], approve_count);
        BOOST_CHECK_EQUAL(approves[worker_techspec_approve_state::disapprove], disapprove_count);
    };

    BOOST_TEST_MESSAGE("-- Approving techspec (after abstain)");

    check_approves(0, 0);

    op.state = worker_techspec_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    check_approves(1, 0);

    BOOST_TEST_MESSAGE("-- Repeating approve techspec case");

    GOLOS_CHECK_ERROR_LOGIC(already_voted_in_similar_way, private_key, op);

    BOOST_TEST_MESSAGE("-- Disapproving techspec (after approve)");

    op.state = worker_techspec_approve_state::disapprove;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    check_approves(0, 1);

    BOOST_TEST_MESSAGE("-- Repeating disapprove techspec case");

    GOLOS_CHECK_ERROR_LOGIC(already_voted_in_similar_way, private_key, op);

    BOOST_TEST_MESSAGE("-- Approving techspec (after disapprove)");

    op.state = worker_techspec_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    check_approves(1, 0);

    BOOST_TEST_MESSAGE("-- Abstaining techspec (after approve)");

    op.state = worker_techspec_approve_state::abstain;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    check_approves(0, 0);

    BOOST_TEST_MESSAGE("-- Disapproving techspec (after abstain)");

    op.state = worker_techspec_approve_state::disapprove;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    check_approves(0, 1);

    BOOST_TEST_MESSAGE("-- Abstaining techspec (after disapprove)");

    op.state = worker_techspec_approve_state::abstain;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    check_approves(0, 0);
}

BOOST_AUTO_TEST_CASE(worker_techspec_approve_top19_updating) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_approve_top19_updating");

    ACTORS((alice)(bob)(carol))
    auto private_key = create_approvers(0, 19*2);
    generate_block();

    signed_transaction tx;

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");
    generate_block();

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 2;
    wtop.worker = "alice";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES);

    BOOST_TEST_MESSAGE("-- Disapproving worker techspec by one witness");

    worker_techspec_approve_operation op;

    op.author = "bob";
    op.permlink = "bob-techspec";
    op.state = worker_techspec_approve_state::disapprove;
    op.approver = "approver0";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    {
        const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
        BOOST_CHECK(db->count_worker_techspec_approves(wto_post.id)[worker_techspec_approve_state::disapprove] == 1);
    }

    BOOST_TEST_MESSAGE("-- Upvoting another witnesses to remove approver from top19");

    push_approvers_top19("carol", carol_private_key, 0, 19, true);
    push_approvers_top19("carol", carol_private_key, 0, 19, false);
    push_approvers_top19("carol", carol_private_key, 19, 19*2, true);
    generate_blocks(STEEMIT_MAX_WITNESSES);

    {
        const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
        BOOST_CHECK(db->count_worker_techspec_approves(wto_post.id)[worker_techspec_approve_state::approve] == 0);
    }
}

BOOST_AUTO_TEST_CASE(worker_techspec_approve_apply_approve) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_approve_apply_approve");

    ACTORS((alice)(bob)(carol)(dave)(eve))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES + 1);
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Approving techspec by not witness case");

    worker_techspec_approve_operation op;
    op.approver = "alice";
    op.author = "bob";
    op.permlink = "bob-techspec";
    op.state = worker_techspec_approve_state::approve;
    GOLOS_CHECK_ERROR_MISSING(witness, "alice", alice_private_key, op);

    BOOST_TEST_MESSAGE("-- Approving techspec by witness not in TOP-19 case");

    op.approver = "approver0";
    GOLOS_CHECK_ERROR_LOGIC(approver_is_not_top19_witness, private_key, op);

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    BOOST_TEST_MESSAGE("-- Approving techspec without post case");

    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("bob", "bob-techspec"), private_key, op);

    BOOST_TEST_MESSAGE("-- Approving non-existing techspec case");

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    GOLOS_CHECK_ERROR_MISSING(worker_techspec_object, make_comment_id("bob", "bob-techspec"), private_key, op);

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "alice";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    comment_create("eve", eve_private_key, "eve-techspec", "", "eve-techspec");

    wtop.author = "eve";
    wtop.permlink = "eve-techspec";
    wtop.specification_cost = ASSET_GOLOS(0);
    wtop.development_cost = ASSET_GOLOS(0);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, eve_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Disapproving worker techspec by 1 witness");
    op.approver = "approver" + std::to_string(STEEMIT_MAJOR_VOTED_WITNESSES);
    op.state = worker_techspec_approve_state::disapprove;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    BOOST_TEST_MESSAGE("-- Approving worker techspec by another witnesses");

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        const auto& wto = db->get_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
        BOOST_CHECK(wto.state == worker_techspec_state::created);

        const auto name = "approver" + std::to_string(i);
        op.approver = name;
        op.state = worker_techspec_approve_state::approve;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
        generate_block();
    }

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    const auto& wto = db->get_worker_techspec(wto_post.id);
    BOOST_CHECK(wto.state == worker_techspec_state::payment);

    auto secs = wto.payments_interval * wto.payments_count;
    auto cost = wto.specification_cost + wto.development_cost;
    auto consumption = std::min(cost * 60*60*24 / secs, cost);
    BOOST_CHECK_EQUAL(db->get_dynamic_global_properties().worker_consumption_per_day, consumption);

    BOOST_TEST_MESSAGE("-- Checking approves (they are not deleted since clear is off");

    auto checked_approves = 0;
    auto checked_disapproves = 0;
    const auto& wtao_idx = db->get_index<worker_techspec_approve_index, by_techspec_approver>();
    auto wtao_itr = wtao_idx.lower_bound(wto_post.id);
    for (; wtao_itr != wtao_idx.end(); ++wtao_itr) {
       if (wtao_itr->state == worker_techspec_approve_state::approve) {
           checked_approves++;
           continue;
       }
       checked_disapproves++;
    }
    BOOST_CHECK_EQUAL(checked_approves, STEEMIT_MAJOR_VOTED_WITNESSES);
    BOOST_CHECK_EQUAL(checked_disapproves, 1);
}

BOOST_AUTO_TEST_CASE(worker_techspec_approve_premade) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_approve_premade");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    comment_create("alice", alice_private_key, "alice-techspec", "", "alice-techspec");

    worker_techspec_operation wtop;
    wtop.author = "alice";
    wtop.permlink = "alice-techspec";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Approving techspec and checking it is not approved early");

    auto approver_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_blocks(STEEMIT_MAX_WITNESSES);
    auto now = approve_techspec_final("alice", "alice-techspec", approver_key);

    BOOST_TEST_MESSAGE("-- Checking it is approved");

    const auto& wto = db->get_worker_techspec(db->get_comment("alice", string("alice-techspec")).id);
    BOOST_CHECK(wto.state == worker_techspec_state::payment);
    BOOST_CHECK_EQUAL(wto.next_cashout_time, now + wto.payments_interval);
}

BOOST_AUTO_TEST_CASE(worker_techspec_approve_apply_disapprove) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_approve_apply_disapprove");

    ACTORS((alice)(bob)(carol))
    auto private_key = create_approvers(0, STEEMIT_SUPER_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");
    generate_block();

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    comment_create("carol", carol_private_key, "carol-techspec", "", "carol-techspec");

    wtop.author = "carol";
    wtop.permlink = "carol-techspec";
    wtop.specification_cost = ASSET_GOLOS(0);
    wtop.development_cost = ASSET_GOLOS(0);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    BOOST_TEST_MESSAGE("-- Disapproving worker techspec by witnesses");

    worker_techspec_approve_operation op;

    for (auto i = 0; i < STEEMIT_SUPER_MAJOR_VOTED_WITNESSES; ++i) {
        const auto& wto = db->get_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
        BOOST_CHECK(wto.state == worker_techspec_state::created);

        const auto name = "approver" + std::to_string(i);
        op.approver = name;
        op.author = "bob";
        op.permlink = "bob-techspec";
        op.state = worker_techspec_approve_state::disapprove;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
        generate_block();
    }

    const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
    const auto& wto = db->get_worker_techspec(wto_post.id);
    BOOST_CHECK(wto.state == worker_techspec_state::closed_by_witnesses);

    BOOST_TEST_MESSAGE("-- Checking approves (they are not deleted since clear is off");

    auto checked_approves = 0;
    auto checked_disapproves = 0;
    const auto& wtao_idx = db->get_index<worker_techspec_approve_index, by_techspec_approver>();
    auto wtao_itr = wtao_idx.lower_bound(wto_post.id);
    for (; wtao_itr != wtao_idx.end(); ++wtao_itr) {
       if (wtao_itr->state == worker_techspec_approve_state::approve) {
           checked_approves++;
           continue;
       }
       checked_disapproves++;
    }
    BOOST_CHECK_EQUAL(checked_approves, 0);
    BOOST_CHECK_EQUAL(checked_disapproves, STEEMIT_SUPER_MAJOR_VOTED_WITNESSES);

    BOOST_TEST_MESSAGE("-- Checking cannot approve closed techspec");

    GOLOS_CHECK_ERROR_LOGIC(incorrect_techspec_state, private_key, op);

    BOOST_TEST_MESSAGE("-- Checking can approve another techspec");

    op.author = "carol";
    op.permlink = "carol-techspec";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
}

BOOST_AUTO_TEST_CASE(worker_techspec_approve_apply_clear_on_approve) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_approve_apply_clear_on_approve");

    ACTORS((alice)(bob)(carol))
    auto private_key = create_approvers(0, STEEMIT_SUPER_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    db->set_clear_old_worker_approves(true);

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");
    generate_block();

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    comment_create("carol", carol_private_key, "carol-techspec", "", "carol-techspec");
    generate_block();

    wtop.author = "carol";
    wtop.permlink = "carol-techspec";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    BOOST_TEST_MESSAGE("-- Disapproving carol worker techspec by witnesses");

    for (auto i = 0; i < STEEMIT_SUPER_MAJOR_VOTED_WITNESSES; ++i) {
        const auto name = "approver" + std::to_string(i);
        worker_techspec_approve_operation op;
        op.approver = name;
        op.author = "carol";
        op.permlink = "carol-techspec";
        op.state = worker_techspec_approve_state::disapprove;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
        generate_block();
    }

    {
        const auto& wto_post = db->get_comment("carol", string("carol-techspec"));
        const auto& wto = db->get_worker_techspec(wto_post.id);
        BOOST_CHECK(wto.state == worker_techspec_state::closed_by_witnesses);

        const auto& wtao_idx = db->get_index<worker_techspec_approve_index, by_techspec_approver>();
        BOOST_CHECK(wtao_idx.find(wto_post.id) == wtao_idx.end());
    }

    BOOST_TEST_MESSAGE("-- Approving bob worker techspec by witnesses");

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        const auto name = "approver" + std::to_string(i);
        worker_techspec_approve_operation op;
        op.approver = name;
        op.author = "bob";
        op.permlink = "bob-techspec";
        op.state = worker_techspec_approve_state::approve;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
        generate_block();
    }

    {
        const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
        const auto& wto = db->get_worker_techspec(wto_post.id);
        BOOST_CHECK(wto.state == worker_techspec_state::payment);

        const auto& wtao_idx = db->get_index<worker_techspec_approve_index, by_techspec_approver>();
        BOOST_CHECK(wtao_idx.find(wto_post.id) == wtao_idx.end());
    }
}

BOOST_AUTO_TEST_CASE(worker_techspec_approve_apply_clear_on_expired) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_approve_apply_clear_on_expired");

    ACTORS((alice)(bob)(carol))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Creating bob techspec");

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));

    BOOST_TEST_MESSAGE("-- Creating carol techspec in same block");

    comment_create("carol", carol_private_key, "carol-techspec", "", "carol-techspec");

    wtop.author = "carol";
    wtop.permlink = "carol-techspec";
    wtop.worker = "carol";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Approving bob techspec by 1 witness");

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    worker_techspec_approve_operation op;
    op.approver = "approver0";
    op.author = "bob";
    op.permlink = "bob-techspec";
    op.state = worker_techspec_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));

    BOOST_TEST_MESSAGE("-- Approving carol techspec by major witnesses");

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        const auto name = "approver" + std::to_string(i);
        op.approver = name;
        op.author = "carol";
        op.permlink = "carol-techspec";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
        generate_block();
    }

    BOOST_TEST_MESSAGE("-- Enabling clearing of approves and waiting for (approve_term - 1 block)");

    generate_blocks(db->get_comment("carol", string("carol-techspec")).created
        + GOLOS_WORKER_TECHSPEC_APPROVE_TERM_SEC - STEEMIT_BLOCK_INTERVAL, true);

    db->set_clear_old_worker_approves(true);

    BOOST_TEST_MESSAGE("-- Checking bob techspec opened and approve still exists");

    {
        const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
        const auto& wto = db->get_worker_techspec(wto_post.id);
        BOOST_CHECK(wto.state != worker_techspec_state::closed_by_expiration);

        const auto& wtao_idx = db->get_index<worker_techspec_approve_index, by_techspec_approver>();
        BOOST_CHECK(wtao_idx.find(wto_post.id) != wtao_idx.end());
    }

    BOOST_TEST_MESSAGE("-- Waiting for 1 block to expire both techspecs approve term");

    generate_block();

    BOOST_TEST_MESSAGE("-- Checking bob techspec closed and approve cleared");

    {
        const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
        const auto& wto = db->get_worker_techspec(wto_post.id);
        BOOST_CHECK(wto.state == worker_techspec_state::closed_by_expiration);

        const auto& wtao_idx = db->get_index<worker_techspec_approve_index, by_techspec_approver>();
        BOOST_CHECK(wtao_idx.find(wto_post.id) == wtao_idx.end());

        BOOST_TEST_MESSAGE("-- Checking virtual operation pushed");

        auto teop = get_last_operations<techspec_expired_operation>(1)[0];
        BOOST_CHECK_EQUAL(teop.author, wto_post.author);
        BOOST_CHECK_EQUAL(teop.permlink, to_string(wto_post.permlink));
    }

    BOOST_TEST_MESSAGE("-- Checking carol techspec is not closed and has approves");

    {
        const auto& wto_post = db->get_comment("carol", string("carol-techspec"));
        const auto& wto = db->get_worker_techspec(wto_post.id);
        BOOST_CHECK(wto.state == worker_techspec_state::payment);

        BOOST_TEST_MESSAGE("-- Checking carol techspec has approves (it should, clearing is enabled after final approve)");

        const auto& wtao_idx = db->get_index<worker_techspec_approve_index, by_techspec_approver>();
        BOOST_CHECK(wtao_idx.find(wto_post.id) != wtao_idx.end());
    }
}

BOOST_AUTO_TEST_CASE(worker_techspec_delete_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_delete_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_techspec_delete_operation op;
    op.author = "bob";
    op.permlink = "bob-techspec";
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect account or permlink case");

    CHECK_PARAM_INVALID(op, author, "");
    CHECK_PARAM_INVALID(op, permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));
}

BOOST_AUTO_TEST_CASE(worker_techspec_delete_apply) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_delete_apply");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Checking cannot delete techspec with non-existant post");

    worker_techspec_delete_operation op;
    op.author = "bob";
    op.permlink = "bob-techspec";
    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("bob", "bob-techspec"), bob_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking cannot delete non-existant techspec");

    comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

    GOLOS_CHECK_ERROR_MISSING(worker_techspec_object, make_comment_id("bob", "bob-techspec"), bob_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Creating techspec");

    worker_techspec_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-techspec";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    {
        const auto* wto = db->find_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
        BOOST_CHECK(wto);
    }

    BOOST_TEST_MESSAGE("-- Checking cannot delete post with techspec");

    delete_comment_operation dcop;
    dcop.author = "bob";
    dcop.permlink = "bob-techspec";
    GOLOS_CHECK_ERROR_LOGIC(cannot_delete_post_with_worker_techspec, bob_private_key, dcop);
    generate_block();

    BOOST_TEST_MESSAGE("-- Deleting techspec");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    {
        const auto* wto = db->find_worker_techspec(db->get_comment("bob", string("bob-techspec")).id);
        BOOST_CHECK(!wto);
    }

    BOOST_TEST_MESSAGE("-- Checking can delete post now");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, dcop));
    generate_block();

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_techspec_delete_apply_closing_cases) {
    BOOST_TEST_MESSAGE("Testing: worker_techspec_delete_apply_closing_cases");

    ACTORS((alice)(bob)(carol)(dave))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    db->set_clear_old_worker_approves(true);

    worker_techspec_operation wtop;

    BOOST_TEST_MESSAGE("-- Creating techspec without approves");

    {
        comment_create("bob", bob_private_key, "bob-techspec", "", "bob-techspec");

        wtop.author = "bob";
        wtop.permlink = "bob-techspec";
        wtop.specification_cost = ASSET_GOLOS(6);
        wtop.development_cost = ASSET_GOLOS(60);
        wtop.payments_interval = 60*60*24;
        wtop.payments_count = 40;
        wtop.worker = "bob";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
        generate_block();

        BOOST_TEST_MESSAGE("-- Deleting it");

        worker_techspec_delete_operation op;
        op.author = "bob";
        op.permlink = "bob-techspec";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
        generate_block();

        {
            BOOST_TEST_MESSAGE("-- Checking it is deleted");

            const auto& wto_post = db->get_comment("bob", string("bob-techspec"));
            const auto* wto = db->find_worker_techspec(wto_post.id);
            BOOST_CHECK(!wto);
        }
    }

    BOOST_TEST_MESSAGE("-- Creating techspec with 1 approve");

    {
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
        generate_block();

        generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

        worker_techspec_approve("approver0", private_key, "bob", "bob-techspec", worker_techspec_approve_state::approve);
        generate_block();

        BOOST_TEST_MESSAGE("-- Deleting it");

        worker_techspec_delete_operation op;
        op.author = "bob";
        op.permlink = "bob-techspec";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
        generate_block();

        check_techspec_closed(db->get_comment("bob", string("bob-techspec")).id,
            worker_techspec_state::closed_by_author, ASSET_GOLOS(0));
    }

    BOOST_TEST_MESSAGE("-- Checking cannot close closed techspec");

    {
        worker_techspec_delete_operation op;
        op.author = "bob";
        op.permlink = "bob-techspec";
        GOLOS_CHECK_ERROR_LOGIC(incorrect_techspec_state, bob_private_key, op);
    }

    validate_database();
}

BOOST_AUTO_TEST_SUITE_END()
