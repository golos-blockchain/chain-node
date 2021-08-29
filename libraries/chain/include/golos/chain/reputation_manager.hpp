#pragma once

#include <golos/protocol/steem_operations.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/comment_object.hpp>

namespace golos { namespace chain {

class reputation_manager {
public:
    reputation_manager(reputation_manager&&) = default;
    reputation_manager(const reputation_manager&) = delete;

    reputation_manager(database& db) : _db(db) {
    }

    void unvote_reputation(const comment_vote_object& vote, const vote_operation& op) {
        auto reputation_delta = vote.rshares >> 6; // Shift away precision from vests. It is noise

        _db.modify(_db.get_account(op.author), [&](auto& a) {
            a.reputation -= reputation_delta;
        });
    }

    void vote_reputation(const account_object& voter, const vote_operation& op, int64_t rshares, bool with_event = false) {
        // Rule #1: Must have non-negative reputation to effect another user's reputation
        if (voter.reputation < 0) {
            return;
        }

        const auto& author = _db.get_account(op.author); 

        // Rule #2: If you are down voting another user, you must have more reputation than them to impact their reputation
        if (rshares < 0 && voter.reputation <= author.reputation) {
            return;
        }

        auto reputation_delta = rshares >> 6; // // Shift away precision from vests. It is noise

        int64_t reputation_before = author.reputation.value;
        _db.modify(author, [&](auto& a) {
            a.reputation += reputation_delta;
        });

        if (with_event) {
            auto reputation_after = reputation_before + reputation_delta;

            _db.push_event(account_reputation_operation(voter.name,
                author.name,
                reputation_before, reputation_after,
                op.weight));

            if (reputation_before >= 0 && reputation_after < 0) {
                _db.push_event(minus_reputation_operation(voter.name,
                    author.name,
                    reputation_before, reputation_after,
                    op.weight));
            }
        }
    }
private:
    database& _db;
};

} } // golos::chain
