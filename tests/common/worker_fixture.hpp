#pragma once

#include <golos/chain/worker_objects.hpp>

#include "database_fixture.hpp"

namespace golos { namespace chain {

struct worker_fixture : public clean_database_fixture {

    void initialize(const plugin_options& opts = {}) {
        database_fixture::initialize(opts);
        open_database();
        startup();
    }

    const worker_proposal_object& worker_proposal(
            const string& author, const private_key_type& author_key, const string& permlink, worker_proposal_type type) {
        signed_transaction tx;

        worker_proposal_operation op;
        op.author = author;
        op.permlink = permlink;
        op.type = type;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, author_key, op));

        return db->get_worker_proposal(db->get_comment(author, permlink).id);
    }

    void worker_techspec_approve(
            const string& approver, const private_key_type& approver_key,
            const string& author, const string& permlink, worker_techspec_approve_state state) {
        signed_transaction tx;

        worker_techspec_approve_operation op;
        op.approver = approver;
        op.author = author;
        op.permlink = permlink;
        op.state = state;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));
    }

    void worker_assign(
            const string& assigner, const private_key_type& assigner_key,
            const string& worker_techspec_author, const string& worker_techspec_permlink, const string& worker) {
        signed_transaction tx;

        worker_assign_operation op;
        op.assigner = assigner;
        op.worker_techspec_author = worker_techspec_author;
        op.worker_techspec_permlink = worker_techspec_permlink;
        op.worker = worker;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, assigner_key, op));
    }

    const worker_techspec_object& worker_result(
            const string& author, const private_key_type& author_key,
            const string& permlink, const string& worker_techspec_permlink) {
        signed_transaction tx;

        worker_result_operation op;
        op.author = author,
        op.permlink = permlink;
        op.worker_techspec_permlink = worker_techspec_permlink;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, author_key, op));

        return db->get_worker_result(db->get_comment(author, permlink).id);
    }

    void worker_payment_approve(
            const string& approver, const private_key_type& approver_key,
            const string& worker_techspec_author, const string& worker_techspec_permlink, worker_techspec_approve_state state) {
        signed_transaction tx;

        worker_payment_approve_operation op;
        op.approver = approver;
        op.worker_techspec_author = worker_techspec_author;
        op.worker_techspec_permlink = worker_techspec_permlink;
        op.state = state;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));
    }

    void check_techspec_closed(const comment_id_type& post, worker_techspec_state state, bool was_approved, asset consumption_after_close) {
        BOOST_TEST_MESSAGE("---- Checking techspec is not deleted but closed");

        const auto* wto = db->find_worker_techspec(post);
        BOOST_CHECK(wto);
        BOOST_CHECK(wto->state == state);

        BOOST_TEST_MESSAGE("---- Checking approves are cleared");

        const auto& wtao_idx = db->get_index<worker_techspec_approve_index, by_techspec_approver>();
        BOOST_CHECK(wtao_idx.find(post) == wtao_idx.end());

        const auto& wpao_idx = db->get_index<worker_payment_approve_index, by_techspec_approver>();
        BOOST_CHECK(wpao_idx.find(post) == wpao_idx.end());

        if (was_approved) {
            BOOST_TEST_MESSAGE("---- Checking worker proposal is open");

            const auto& wpo = db->get_worker_proposal(wto->worker_proposal_post);
            BOOST_CHECK(wpo.state == worker_proposal_state::created);
        }

        BOOST_TEST_MESSAGE("---- Checking worker funds are unfrozen");

        const auto& gpo = db->get_dynamic_global_properties();
        BOOST_CHECK_EQUAL(gpo.worker_consumption_per_day, consumption_after_close);
    }

    private_key_type create_approvers(uint16_t first, uint16_t count) {
        auto private_key = generate_private_key("test");
        auto post_key = generate_private_key("test_post");
        for (auto i = first; i < count; ++i) {
            const auto name = "approver" + std::to_string(i);
            GOLOS_CHECK_NO_THROW(account_create(name, private_key.get_public_key(), post_key.get_public_key()));
            GOLOS_CHECK_NO_THROW(witness_create(name, private_key, "foo.bar", private_key.get_public_key(), 1000));
        }
        return private_key;
    }

    void push_approvers_top19(const account_name_type& voter, const private_key_type& voter_key, uint16_t first, uint16_t count, bool up) {
        signed_transaction tx;
        for (auto i = first; i < count; ++i) {
            const auto name = "approver" + std::to_string(i);
            account_witness_vote_operation awvop;
            awvop.account = voter;
            awvop.witness = name;
            awvop.approve = up;
            BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, voter_key, awvop));
        }
    }

    fc::time_point_sec approve_techspec_final(const string& author, const string& permlink, const private_key_type& key) {
        fc::time_point_sec now;

        for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
            const auto& wto = db->get_worker_techspec(db->get_comment(author, permlink).id);
            BOOST_CHECK(wto.state == worker_techspec_state::created);
            BOOST_CHECK_EQUAL(wto.next_cashout_time, time_point_sec::maximum());

            now = db->head_block_time();

            worker_techspec_approve("approver" + std::to_string(i), key, author, permlink, worker_techspec_approve_state::approve);
            generate_block();
        }

        return now;
    }
};

} } // golos:chain
