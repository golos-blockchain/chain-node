#include <boost/test/unit_test.hpp>

#include "worker_fixture.hpp"
#include "helpers.hpp"

#include <golos/protocol/worker_operations.hpp>
#include <golos/chain/worker_objects.hpp>

using namespace golos;
using namespace golos::protocol;
using namespace golos::chain;

BOOST_FIXTURE_TEST_SUITE(worker_request_tests, worker_fixture)

BOOST_AUTO_TEST_CASE(worker_authorities) {
    BOOST_TEST_MESSAGE("Testing: worker_authorities");

    {
        worker_request_operation op;
        op.author = "bob";
        op.permlink = "bob-request";
        op.specification_cost = ASSET_GOLOS(6000);
        op.development_cost = ASSET_GOLOS(60000);
        op.payments_interval = 60;
        op.payments_count = 2;
        op.worker = "bob";
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"bob"}));
    }

    {
        worker_request_delete_operation op;
        op.author = "bob";
        op.permlink = "bob-request";
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"bob"}));
    }

    {
        worker_request_approve_operation op;
        op.approver = "cyberfounder";
        op.author = "bob";
        op.permlink = "bob-request";
        op.state = worker_request_approve_state::approve;
        CHECK_OP_AUTHS(op, account_name_set(), account_name_set(), account_name_set({"cyberfounder"}));
    }
}

BOOST_AUTO_TEST_CASE(worker_request_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_request_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_request_operation op;
    op.author = "bob";
    op.permlink = "request-permlink";
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

BOOST_AUTO_TEST_CASE(worker_request_apply_create) {
    BOOST_TEST_MESSAGE("Testing: worker_request_apply_create");

    ACTORS((alice)(bob)(carol)(dave)(eve)(fred)(greta))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Create worker request with no post case");

    worker_request_operation op;
    op.author = "bob";
    op.permlink = "bob-request";
    op.specification_cost = ASSET_GOLOS(6);
    op.development_cost = ASSET_GOLOS(60);
    op.payments_interval = 60*60*24*2;
    op.payments_count = 2;
    op.worker = "bob";
    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("bob", "bob-request"), bob_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Create worker request on comment instead of post case");

    comment_create("alice", alice_private_key, "the-post", "", "the-post");
    comment_create("carol", carol_private_key, "i-am-comment", "alice", "the-post");

    op.author = "carol";
    op.permlink = "i-am-comment";
    GOLOS_CHECK_ERROR_LOGIC(post_is_not_root, carol_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Creating request with not-exist worker");

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    op.author = "bob";
    op.permlink = "bob-request";
    op.worker = "notexistacc";
    GOLOS_CHECK_ERROR_MISSING(account, "notexistacc", bob_private_key, op);

    BOOST_TEST_MESSAGE("-- Normal create worker request case");

    op.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    const auto& wto_post = db->get_comment("bob", string("bob-request"));
    const auto& wto = db->get_worker_request(wto_post.id);
    BOOST_CHECK_EQUAL(wto.post, wto_post.id);
    BOOST_CHECK(wto.state == worker_request_state::created);
    BOOST_CHECK_EQUAL(wto.specification_cost, op.specification_cost);
    BOOST_CHECK_EQUAL(wto.development_cost, op.development_cost);
    BOOST_CHECK_EQUAL(wto.payments_count, op.payments_count);
    BOOST_CHECK_EQUAL(wto.payments_interval, op.payments_interval);
    BOOST_CHECK_EQUAL(wto.worker, op.worker);

    BOOST_CHECK_EQUAL(wto.next_cashout_time, fc::time_point_sec::maximum());
    BOOST_CHECK_EQUAL(wto.finished_payments_count, 0);

    {
        BOOST_TEST_MESSAGE("-- Check cannot create worker request on post outside cashout window");

        comment_create("greta", greta_private_key, "greta-request", "", "greta-request");

        generate_blocks(db->head_block_time() + STEEMIT_CASHOUT_WINDOW_SECONDS + STEEMIT_BLOCK_INTERVAL, true);

        op.author = "greta";
        op.permlink = "greta-request";
        GOLOS_CHECK_ERROR_LOGIC(post_should_be_in_cashout_window, greta_private_key, op);
    }

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_request_apply_modify) {
    BOOST_TEST_MESSAGE("Testing: worker_request_apply_modify");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    worker_request_operation op;
    op.author = "bob";
    op.permlink = "bob-request";
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
        const auto& wto_post = db->get_comment("bob", string("bob-request"));
        const auto& wto = db->get_worker_request(wto_post.id);
        BOOST_CHECK_EQUAL(wto.payments_count, op.payments_count);
        BOOST_CHECK_EQUAL(wto.payments_interval, op.payments_interval);
    }

    BOOST_TEST_MESSAGE("-- Modify payments_count and payments_interval");

    op.specification_cost = ASSET_GOLOS(7);
    op.development_cost = ASSET_GOLOS(70);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    {
        const auto& wto_post = db->get_comment("bob", string("bob-request"));
        const auto& wto = db->get_worker_request(wto_post.id);
        BOOST_CHECK_EQUAL(wto.specification_cost, op.specification_cost);
        BOOST_CHECK_EQUAL(wto.development_cost, op.development_cost);
    }

    BOOST_TEST_MESSAGE("-- Change worker");

    op.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    {
        const auto& wto_post = db->get_comment("bob", string("bob-request"));
        const auto& wto = db->get_worker_request(wto_post.id);
        BOOST_CHECK_EQUAL(wto.worker, "bob");
    }

    BOOST_TEST_MESSAGE("-- Check cannot modify approved request");

    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        const auto name = "approver" + std::to_string(i);
        worker_request_approve_operation wtaop;
        wtaop.approver = name;
        wtaop.author = "bob";
        wtaop.permlink = "bob-request";
        wtaop.state = worker_request_approve_state::approve;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, wtaop));
        generate_block();
    }

    op.development_cost = ASSET_GOLOS(50);
    GOLOS_CHECK_ERROR_LOGIC(incorrect_request_state, bob_private_key, op);

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_request_approve_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_request_approve_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_request_approve_operation op;
    op.approver = "cyberfounder";
    op.author = "bob";
    op.permlink = "request-permlink";
    op.state = worker_request_approve_state::approve;
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect account or permlink case");

    CHECK_PARAM_INVALID(op, approver, "");
    CHECK_PARAM_INVALID(op, author, "");
    CHECK_PARAM_INVALID(op, permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));

    BOOST_TEST_MESSAGE("-- Invalid state case");

    CHECK_PARAM_INVALID(op, state, worker_request_approve_state::_size);
}

BOOST_AUTO_TEST_CASE(worker_request_approve_checking_funds) {
    BOOST_TEST_MESSAGE("Testing: worker_request_approve_checking_funds");

    ACTORS((alice)(bob))
    auto private_key = create_approvers(0, 1);
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Measuring revenue per block");

    const auto before = db->get_dynamic_global_properties().total_worker_fund_steem;
    generate_blocks(1);
    const auto revenue = db->get_dynamic_global_properties().total_worker_fund_steem - before;

    BOOST_TEST_MESSAGE("-- Creating request with cost a bit larger than revenue");

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.specification_cost = db->get_dynamic_global_properties().total_worker_fund_steem;
    wtop.development_cost = revenue * (60*60*37*2 / STEEMIT_BLOCK_INTERVAL + 1);
    wtop.payments_interval = 60*60*37;
    wtop.payments_count = 2;
    wtop.worker = "alice";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));

    BOOST_TEST_MESSAGE("-- Checking cannot approve");

    worker_request_approve_operation op;
    op.approver = "approver0";
    op.author = "bob";
    op.permlink = "bob-request";
    op.state = worker_request_approve_state::approve;
    GOLOS_CHECK_ERROR_LOGIC(insufficient_funds_to_approve, private_key, op);

    BOOST_TEST_MESSAGE("-- Editing it to be equal to revenue");

    wtop.development_cost = revenue * (60*60*36*2 / STEEMIT_BLOCK_INTERVAL);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));

    BOOST_TEST_MESSAGE("-- Checking can approve now");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
}

BOOST_AUTO_TEST_CASE(worker_request_approve_apply_combinations) {
    BOOST_TEST_MESSAGE("Testing: worker_request_approve_apply_combinations");

    ACTORS((alice)(bob))
    auto private_key = create_approvers(0, 1);
    generate_block();

    signed_transaction tx;

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24*2;
    wtop.payments_count = 2;
    wtop.worker = "alice";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    BOOST_TEST_MESSAGE("-- Abstaining non-voted request case");

    worker_request_approve_operation op;
    op.approver = "approver0";
    op.author = "bob";
    op.permlink = "bob-request";
    op.state = worker_request_approve_state::abstain;
    GOLOS_CHECK_ERROR_LOGIC(already_voted_in_similar_way, private_key, op);

    auto check_approves = [&](int approve_count, int disapprove_count) {
        auto approves = db->count_worker_request_approves(db->get_comment("bob", string("bob-request")).id);
        BOOST_CHECK_EQUAL(approves[worker_request_approve_state::approve], approve_count);
        BOOST_CHECK_EQUAL(approves[worker_request_approve_state::disapprove], disapprove_count);
    };

    BOOST_TEST_MESSAGE("-- Approving request (after abstain)");

    check_approves(0, 0);

    op.state = worker_request_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    check_approves(1, 0);

    BOOST_TEST_MESSAGE("-- Repeating approve request case");

    GOLOS_CHECK_ERROR_LOGIC(already_voted_in_similar_way, private_key, op);

    BOOST_TEST_MESSAGE("-- Disapproving request (after approve)");

    op.state = worker_request_approve_state::disapprove;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    check_approves(0, 1);

    BOOST_TEST_MESSAGE("-- Repeating disapprove request case");

    GOLOS_CHECK_ERROR_LOGIC(already_voted_in_similar_way, private_key, op);

    BOOST_TEST_MESSAGE("-- Approving request (after disapprove)");

    op.state = worker_request_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    check_approves(1, 0);

    BOOST_TEST_MESSAGE("-- Abstaining request (after approve)");

    op.state = worker_request_approve_state::abstain;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    check_approves(0, 0);

    BOOST_TEST_MESSAGE("-- Disapproving request (after abstain)");

    op.state = worker_request_approve_state::disapprove;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    check_approves(0, 1);

    BOOST_TEST_MESSAGE("-- Abstaining request (after disapprove)");

    op.state = worker_request_approve_state::abstain;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    check_approves(0, 0);
}

BOOST_AUTO_TEST_CASE(worker_request_approve_top19_updating) {
    BOOST_TEST_MESSAGE("Testing: worker_request_approve_top19_updating");

    ACTORS((alice)(bob)(carol))
    auto private_key = create_approvers(0, 19*2);
    generate_block();

    signed_transaction tx;

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");
    generate_block();

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 2;
    wtop.worker = "alice";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES);

    BOOST_TEST_MESSAGE("-- Disapproving worker request by one witness");

    worker_request_approve_operation op;

    op.author = "bob";
    op.permlink = "bob-request";
    op.state = worker_request_approve_state::disapprove;
    op.approver = "approver0";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    {
        const auto& wto_post = db->get_comment("bob", string("bob-request"));
        BOOST_CHECK(db->count_worker_request_approves(wto_post.id)[worker_request_approve_state::disapprove] == 1);
    }

    BOOST_TEST_MESSAGE("-- Upvoting another witnesses to remove approver from top19");

    push_approvers_top19("carol", carol_private_key, 0, 19, true);
    push_approvers_top19("carol", carol_private_key, 0, 19, false);
    push_approvers_top19("carol", carol_private_key, 19, 19*2, true);
    generate_blocks(STEEMIT_MAX_WITNESSES);

    {
        const auto& wto_post = db->get_comment("bob", string("bob-request"));
        BOOST_CHECK(db->count_worker_request_approves(wto_post.id)[worker_request_approve_state::approve] == 0);
    }
}

BOOST_AUTO_TEST_CASE(worker_request_approve_apply_approve) {
    BOOST_TEST_MESSAGE("Testing: worker_request_approve_apply_approve");

    ACTORS((alice)(bob)(carol)(dave)(eve))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES + 1);
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Approving request by not witness case");

    worker_request_approve_operation op;
    op.approver = "alice";
    op.author = "bob";
    op.permlink = "bob-request";
    op.state = worker_request_approve_state::approve;
    GOLOS_CHECK_ERROR_MISSING(witness, "alice", alice_private_key, op);

    BOOST_TEST_MESSAGE("-- Approving request by witness not in TOP-19 case");

    op.approver = "approver0";
    GOLOS_CHECK_ERROR_LOGIC(approver_is_not_top19_witness, private_key, op);

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    BOOST_TEST_MESSAGE("-- Approving request without post case");

    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("bob", "bob-request"), private_key, op);

    BOOST_TEST_MESSAGE("-- Approving non-existing request case");

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    GOLOS_CHECK_ERROR_MISSING(worker_request_object, make_comment_id("bob", "bob-request"), private_key, op);

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "alice";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    comment_create("eve", eve_private_key, "eve-request", "", "eve-request");

    wtop.author = "eve";
    wtop.permlink = "eve-request";
    wtop.specification_cost = ASSET_GOLOS(0);
    wtop.development_cost = ASSET_GOLOS(0);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, eve_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Disapproving worker request by 1 witness");
    op.approver = "approver" + std::to_string(STEEMIT_MAJOR_VOTED_WITNESSES);
    op.state = worker_request_approve_state::disapprove;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
    generate_block();

    BOOST_TEST_MESSAGE("-- Approving worker request by another witnesses");

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        const auto& wto = db->get_worker_request(db->get_comment("bob", string("bob-request")).id);
        BOOST_CHECK(wto.state == worker_request_state::created);

        const auto name = "approver" + std::to_string(i);
        op.approver = name;
        op.state = worker_request_approve_state::approve;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
        generate_block();
    }

    const auto& wto_post = db->get_comment("bob", string("bob-request"));
    const auto& wto = db->get_worker_request(wto_post.id);
    BOOST_CHECK(wto.state == worker_request_state::payment);

    auto secs = wto.payments_interval * wto.payments_count;
    auto cost = wto.specification_cost + wto.development_cost;
    auto consumption = std::min(cost * 60*60*24 / secs, cost);
    BOOST_CHECK_EQUAL(db->get_dynamic_global_properties().worker_consumption_per_day, consumption);

    BOOST_TEST_MESSAGE("-- Checking approves (they are not deleted since clear is off");

    auto checked_approves = 0;
    auto checked_disapproves = 0;
    const auto& wtao_idx = db->get_index<worker_request_approve_index, by_request_approver>();
    auto wtao_itr = wtao_idx.lower_bound(wto_post.id);
    for (; wtao_itr != wtao_idx.end(); ++wtao_itr) {
       if (wtao_itr->state == worker_request_approve_state::approve) {
           checked_approves++;
           continue;
       }
       checked_disapproves++;
    }
    BOOST_CHECK_EQUAL(checked_approves, STEEMIT_MAJOR_VOTED_WITNESSES);
    BOOST_CHECK_EQUAL(checked_disapproves, 1);
}

BOOST_AUTO_TEST_CASE(worker_request_approve_premade) {
    BOOST_TEST_MESSAGE("Testing: worker_request_approve_premade");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    comment_create("alice", alice_private_key, "alice-request", "", "alice-request");

    worker_request_operation wtop;
    wtop.author = "alice";
    wtop.permlink = "alice-request";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, alice_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Approving request and checking it is not approved early");

    auto approver_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_blocks(STEEMIT_MAX_WITNESSES);
    auto now = approve_request_final("alice", "alice-request", approver_key);

    BOOST_TEST_MESSAGE("-- Checking it is approved");

    const auto& wto = db->get_worker_request(db->get_comment("alice", string("alice-request")).id);
    BOOST_CHECK(wto.state == worker_request_state::payment);
    BOOST_CHECK_EQUAL(wto.next_cashout_time, now + wto.payments_interval);
}

BOOST_AUTO_TEST_CASE(worker_request_approve_apply_disapprove) {
    BOOST_TEST_MESSAGE("Testing: worker_request_approve_apply_disapprove");

    ACTORS((alice)(bob)(carol))
    auto private_key = create_approvers(0, STEEMIT_SUPER_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");
    generate_block();

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    comment_create("carol", carol_private_key, "carol-request", "", "carol-request");

    wtop.author = "carol";
    wtop.permlink = "carol-request";
    wtop.specification_cost = ASSET_GOLOS(0);
    wtop.development_cost = ASSET_GOLOS(0);
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    BOOST_TEST_MESSAGE("-- Disapproving worker request by witnesses");

    worker_request_approve_operation op;

    for (auto i = 0; i < STEEMIT_SUPER_MAJOR_VOTED_WITNESSES; ++i) {
        const auto& wto = db->get_worker_request(db->get_comment("bob", string("bob-request")).id);
        BOOST_CHECK(wto.state == worker_request_state::created);

        const auto name = "approver" + std::to_string(i);
        op.approver = name;
        op.author = "bob";
        op.permlink = "bob-request";
        op.state = worker_request_approve_state::disapprove;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
        generate_block();
    }

    const auto& wto_post = db->get_comment("bob", string("bob-request"));
    const auto& wto = db->get_worker_request(wto_post.id);
    BOOST_CHECK(wto.state == worker_request_state::closed_by_witnesses);

    BOOST_TEST_MESSAGE("-- Checking approves (they are not deleted since clear is off");

    auto checked_approves = 0;
    auto checked_disapproves = 0;
    const auto& wtao_idx = db->get_index<worker_request_approve_index, by_request_approver>();
    auto wtao_itr = wtao_idx.lower_bound(wto_post.id);
    for (; wtao_itr != wtao_idx.end(); ++wtao_itr) {
       if (wtao_itr->state == worker_request_approve_state::approve) {
           checked_approves++;
           continue;
       }
       checked_disapproves++;
    }
    BOOST_CHECK_EQUAL(checked_approves, 0);
    BOOST_CHECK_EQUAL(checked_disapproves, STEEMIT_SUPER_MAJOR_VOTED_WITNESSES);

    BOOST_TEST_MESSAGE("-- Checking cannot approve closed request");

    GOLOS_CHECK_ERROR_LOGIC(incorrect_request_state, private_key, op);

    BOOST_TEST_MESSAGE("-- Checking can approve another request");

    op.author = "carol";
    op.permlink = "carol-request";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
}

BOOST_AUTO_TEST_CASE(worker_request_approve_apply_clear_on_approve) {
    BOOST_TEST_MESSAGE("Testing: worker_request_approve_apply_clear_on_approve");

    ACTORS((alice)(bob)(carol))
    auto private_key = create_approvers(0, STEEMIT_SUPER_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    db->set_clear_old_worker_approves(true);

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");
    generate_block();

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    comment_create("carol", carol_private_key, "carol-request", "", "carol-request");
    generate_block();

    wtop.author = "carol";
    wtop.permlink = "carol-request";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, wtop));
    generate_block();

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    BOOST_TEST_MESSAGE("-- Disapproving carol worker request by witnesses");

    for (auto i = 0; i < STEEMIT_SUPER_MAJOR_VOTED_WITNESSES; ++i) {
        const auto name = "approver" + std::to_string(i);
        worker_request_approve_operation op;
        op.approver = name;
        op.author = "carol";
        op.permlink = "carol-request";
        op.state = worker_request_approve_state::disapprove;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
        generate_block();
    }

    {
        const auto& wto_post = db->get_comment("carol", string("carol-request"));
        const auto& wto = db->get_worker_request(wto_post.id);
        BOOST_CHECK(wto.state == worker_request_state::closed_by_witnesses);

        const auto& wtao_idx = db->get_index<worker_request_approve_index, by_request_approver>();
        BOOST_CHECK(wtao_idx.find(wto_post.id) == wtao_idx.end());
    }

    BOOST_TEST_MESSAGE("-- Approving bob worker request by witnesses");

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        const auto name = "approver" + std::to_string(i);
        worker_request_approve_operation op;
        op.approver = name;
        op.author = "bob";
        op.permlink = "bob-request";
        op.state = worker_request_approve_state::approve;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
        generate_block();
    }

    {
        const auto& wto_post = db->get_comment("bob", string("bob-request"));
        const auto& wto = db->get_worker_request(wto_post.id);
        BOOST_CHECK(wto.state == worker_request_state::payment);

        const auto& wtao_idx = db->get_index<worker_request_approve_index, by_request_approver>();
        BOOST_CHECK(wtao_idx.find(wto_post.id) == wtao_idx.end());
    }
}

BOOST_AUTO_TEST_CASE(worker_request_approve_apply_clear_on_expired) {
    BOOST_TEST_MESSAGE("Testing: worker_request_approve_apply_clear_on_expired");

    ACTORS((alice)(bob)(carol))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Creating bob request");

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));

    BOOST_TEST_MESSAGE("-- Creating carol request in same block");

    comment_create("carol", carol_private_key, "carol-request", "", "carol-request");

    wtop.author = "carol";
    wtop.permlink = "carol-request";
    wtop.worker = "carol";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, carol_private_key, wtop));
    generate_block();

    BOOST_TEST_MESSAGE("-- Approving bob request by 1 witness");

    generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

    worker_request_approve_operation op;
    op.approver = "approver0";
    op.author = "bob";
    op.permlink = "bob-request";
    op.state = worker_request_approve_state::approve;
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));

    BOOST_TEST_MESSAGE("-- Approving carol request by major witnesses");

    for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
        const auto name = "approver" + std::to_string(i);
        op.approver = name;
        op.author = "carol";
        op.permlink = "carol-request";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, private_key, op));
        generate_block();
    }

    BOOST_TEST_MESSAGE("-- Enabling clearing of approves and waiting for (approve_term - 1 block)");

    generate_blocks(db->get_comment("carol", string("carol-request")).created
        + GOLOS_WORKER_REQUEST_APPROVE_TERM_SEC - STEEMIT_BLOCK_INTERVAL, true);

    db->set_clear_old_worker_approves(true);

    BOOST_TEST_MESSAGE("-- Checking bob request opened and approve still exists");

    {
        const auto& wto_post = db->get_comment("bob", string("bob-request"));
        const auto& wto = db->get_worker_request(wto_post.id);
        BOOST_CHECK(wto.state != worker_request_state::closed_by_expiration);

        const auto& wtao_idx = db->get_index<worker_request_approve_index, by_request_approver>();
        BOOST_CHECK(wtao_idx.find(wto_post.id) != wtao_idx.end());
    }

    BOOST_TEST_MESSAGE("-- Waiting for 1 block to expire both requests approve term");

    generate_block();

    BOOST_TEST_MESSAGE("-- Checking bob request closed and approve cleared");

    {
        const auto& wto_post = db->get_comment("bob", string("bob-request"));
        const auto& wto = db->get_worker_request(wto_post.id);
        BOOST_CHECK(wto.state == worker_request_state::closed_by_expiration);

        const auto& wtao_idx = db->get_index<worker_request_approve_index, by_request_approver>();
        BOOST_CHECK(wtao_idx.find(wto_post.id) == wtao_idx.end());

        BOOST_TEST_MESSAGE("-- Checking virtual operation pushed");

        auto teop = get_last_operations<request_expired_operation>(1)[0];
        BOOST_CHECK_EQUAL(teop.author, wto_post.author);
        BOOST_CHECK_EQUAL(teop.permlink, to_string(wto_post.permlink));
    }

    BOOST_TEST_MESSAGE("-- Checking carol request is not closed and has approves");

    {
        const auto& wto_post = db->get_comment("carol", string("carol-request"));
        const auto& wto = db->get_worker_request(wto_post.id);
        BOOST_CHECK(wto.state == worker_request_state::payment);

        BOOST_TEST_MESSAGE("-- Checking carol request has approves (it should, clearing is enabled after final approve)");

        const auto& wtao_idx = db->get_index<worker_request_approve_index, by_request_approver>();
        BOOST_CHECK(wtao_idx.find(wto_post.id) != wtao_idx.end());
    }
}

BOOST_AUTO_TEST_CASE(worker_request_delete_validate) {
    BOOST_TEST_MESSAGE("Testing: worker_request_delete_validate");

    BOOST_TEST_MESSAGE("-- Normal case");

    worker_request_delete_operation op;
    op.author = "bob";
    op.permlink = "bob-request";
    CHECK_OP_VALID(op);

    BOOST_TEST_MESSAGE("-- Incorrect account or permlink case");

    CHECK_PARAM_INVALID(op, author, "");
    CHECK_PARAM_INVALID(op, permlink, std::string(STEEMIT_MAX_PERMLINK_LENGTH+1, ' '));
}

BOOST_AUTO_TEST_CASE(worker_request_delete_apply) {
    BOOST_TEST_MESSAGE("Testing: worker_request_delete_apply");

    ACTORS((alice)(bob))
    generate_block();

    signed_transaction tx;

    BOOST_TEST_MESSAGE("-- Checking cannot delete request with non-existant post");

    worker_request_delete_operation op;
    op.author = "bob";
    op.permlink = "bob-request";
    GOLOS_CHECK_ERROR_MISSING(comment, make_comment_id("bob", "bob-request"), bob_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Checking cannot delete non-existant request");

    comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

    GOLOS_CHECK_ERROR_MISSING(worker_request_object, make_comment_id("bob", "bob-request"), bob_private_key, op);
    generate_block();

    BOOST_TEST_MESSAGE("-- Creating request");

    worker_request_operation wtop;
    wtop.author = "bob";
    wtop.permlink = "bob-request";
    wtop.specification_cost = ASSET_GOLOS(6);
    wtop.development_cost = ASSET_GOLOS(60);
    wtop.payments_interval = 60*60*24;
    wtop.payments_count = 40;
    wtop.worker = "bob";
    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
    generate_block();

    {
        const auto* wto = db->find_worker_request(db->get_comment("bob", string("bob-request")).id);
        BOOST_CHECK(wto);
    }

    BOOST_TEST_MESSAGE("-- Checking cannot delete post with request");

    delete_comment_operation dcop;
    dcop.author = "bob";
    dcop.permlink = "bob-request";
    GOLOS_CHECK_ERROR_LOGIC(cannot_delete_post_with_worker_request, bob_private_key, dcop);
    generate_block();

    BOOST_TEST_MESSAGE("-- Deleting request");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
    generate_block();

    {
        const auto* wto = db->find_worker_request(db->get_comment("bob", string("bob-request")).id);
        BOOST_CHECK(!wto);
    }

    BOOST_TEST_MESSAGE("-- Checking can delete post now");

    BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, dcop));
    generate_block();

    validate_database();
}

BOOST_AUTO_TEST_CASE(worker_request_delete_apply_closing_cases) {
    BOOST_TEST_MESSAGE("Testing: worker_request_delete_apply_closing_cases");

    ACTORS((alice)(bob)(carol)(dave))
    auto private_key = create_approvers(0, STEEMIT_MAJOR_VOTED_WITNESSES);
    generate_block();

    signed_transaction tx;

    db->set_clear_old_worker_approves(true);

    worker_request_operation wtop;

    BOOST_TEST_MESSAGE("-- Creating request without approves");

    {
        comment_create("bob", bob_private_key, "bob-request", "", "bob-request");

        wtop.author = "bob";
        wtop.permlink = "bob-request";
        wtop.specification_cost = ASSET_GOLOS(6);
        wtop.development_cost = ASSET_GOLOS(60);
        wtop.payments_interval = 60*60*24;
        wtop.payments_count = 40;
        wtop.worker = "bob";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
        generate_block();

        BOOST_TEST_MESSAGE("-- Deleting it");

        worker_request_delete_operation op;
        op.author = "bob";
        op.permlink = "bob-request";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
        generate_block();

        {
            BOOST_TEST_MESSAGE("-- Checking it is deleted");

            const auto& wto_post = db->get_comment("bob", string("bob-request"));
            const auto* wto = db->find_worker_request(wto_post.id);
            BOOST_CHECK(!wto);
        }
    }

    BOOST_TEST_MESSAGE("-- Creating request with 1 approve");

    {
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, wtop));
        generate_block();

        generate_blocks(STEEMIT_MAX_WITNESSES); // Enough for approvers to reach TOP-19 and not leave it

        worker_request_approve("approver0", private_key, "bob", "bob-request", worker_request_approve_state::approve);
        generate_block();

        BOOST_TEST_MESSAGE("-- Deleting it");

        worker_request_delete_operation op;
        op.author = "bob";
        op.permlink = "bob-request";
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, bob_private_key, op));
        generate_block();

        check_request_closed(db->get_comment("bob", string("bob-request")).id,
            worker_request_state::closed_by_author, ASSET_GOLOS(0));
    }

    BOOST_TEST_MESSAGE("-- Checking cannot close closed request");

    {
        worker_request_delete_operation op;
        op.author = "bob";
        op.permlink = "bob-request";
        GOLOS_CHECK_ERROR_LOGIC(incorrect_request_state, bob_private_key, op);
    }

    validate_database();
}

BOOST_AUTO_TEST_SUITE_END()
