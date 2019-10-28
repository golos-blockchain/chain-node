#pragma once

#include <golos/chain/worker_objects.hpp>

#include "database_fixture.hpp"

namespace golos { namespace chain {

struct worker_fixture : public clean_database_fixture {

    worker_fixture(bool init = true) : clean_database_fixture(init) {
    }

    void worker_request_approve(
            const string& approver, const private_key_type& approver_key,
            const string& author, const string& permlink, worker_request_approve_state state) {
        signed_transaction tx;

        worker_request_approve_operation op;
        op.approver = approver;
        op.author = author;
        op.permlink = permlink;
        op.state = state;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, approver_key, op));
    }

    void check_request_closed(const comment_id_type& post, worker_request_state state, asset consumption_after_close) {
        BOOST_TEST_MESSAGE("---- Checking request is not deleted but closed");

        const auto* wto = db->find_worker_request(post);
        BOOST_CHECK(wto);
        BOOST_CHECK(wto->state == state);

        BOOST_TEST_MESSAGE("---- Checking approves are cleared");

        const auto& wtao_idx = db->get_index<worker_request_approve_index, by_request_approver>();
        BOOST_CHECK(wtao_idx.find(post) == wtao_idx.end());

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

    fc::time_point_sec approve_request_final(const string& author, const string& permlink, const private_key_type& key) {
        fc::time_point_sec now;

        for (auto i = 0; i < STEEMIT_MAJOR_VOTED_WITNESSES; ++i) {
            const auto& wto = db->get_worker_request(db->get_comment(author, permlink).id);
            BOOST_CHECK(wto.state == worker_request_state::created);
            BOOST_CHECK_EQUAL(wto.next_cashout_time, time_point_sec::maximum());

            now = db->head_block_time();

            worker_request_approve("approver" + std::to_string(i), key, author, permlink, worker_request_approve_state::approve);
            generate_block();
        }

        return now;
    }
};

} } // golos:chain
