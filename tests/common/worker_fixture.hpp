#pragma once

#include <golos/chain/worker_objects.hpp>

#include "database_fixture.hpp"

namespace golos { namespace protocol {

static inline std::ostream& operator<<(std::ostream& out, worker_request_state s) {
    return out << fc::json::to_string(s);
}

} } // golos::protocol

namespace golos { namespace chain {

struct worker_fixture : public clean_database_fixture_wrap {

    worker_fixture(bool init = true, std::function<void()> custom_init = {}) :
    clean_database_fixture_wrap(init, custom_init) {
    }

    void worker_request_vote(
            const string& voter, const private_key_type& voter_key,
            const string& author, const string& permlink, int16_t vote_percent) {
        signed_transaction tx;

        worker_request_vote_operation op;
        op.voter = voter;
        op.author = author;
        op.permlink = permlink;
        op.vote_percent = vote_percent;
        BOOST_CHECK_NO_THROW(push_tx_with_ops(tx, voter_key, op));
    }

    void check_request_closed(const comment_id_type& post, worker_request_state state, asset consumption_after_close) {
        BOOST_TEST_MESSAGE("---- Checking request is not deleted but closed");

        const auto* wro = _db.find_worker_request(post);
        BOOST_CHECK(wro);
        BOOST_CHECK(wro->state == state);

        BOOST_TEST_MESSAGE("---- Checking approves are cleared");

        const auto& wrvo_idx = _db.get_index<worker_request_vote_index, by_request_voter>();
        BOOST_CHECK(wrvo_idx.find(post) == wrvo_idx.end());
    }
};

} } // golos:chain
