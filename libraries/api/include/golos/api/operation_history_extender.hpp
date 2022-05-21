#pragma once

#include <golos/chain/comment_object.hpp>
#include <golos/chain/database.hpp>

namespace golos { namespace api {

    using namespace golos::protocol;
    using golos::chain::database;
    using golos::chain::to_string;

    struct operation_history_extender {
        typedef bool result_type;

        database& _db;
        operation& _opv;

        operation_history_extender(database& db, operation& opv) : _db(db), _opv(opv) {
        }

        template<typename T>
        bool operator()(const T& op) const {
            return false;
        }

#define OP_COPY(AUTHOR, ALT) \
    auto op = op0; \
    const auto* extras = _db.find_extras(AUTHOR, ALT)

#define EXTRA(TYPE, FIELD, AUTHOR, ALT) \
    if (extras) { \
        FIELD = to_string(TYPE); \
    } else { \
        FIELD = "hash:" + std::to_string(ALT); \
    }

#define PERM(FIELD, AUTHOR, ALT) \
    EXTRA(extras->permlink, FIELD, AUTHOR, ALT)

#define PERM_PARENT(FIELD, AUTHOR, ALT) \
    EXTRA(extras->parent_permlink, FIELD, AUTHOR, ALT)

#define OP_RETURN() \
    _opv = op; \
    return true;

        bool operator()(const author_reward_operation& op0) const {
            OP_COPY(op.author, op.hashlink);
            PERM(op.permlink, op.author, op.hashlink);
            OP_RETURN();
        }

        bool operator()(const curation_reward_operation& op0) const {
            OP_COPY(op.comment_author, op.comment_hashlink);
            PERM(op.comment_permlink, op.comment_author, op.comment_hashlink);
            OP_RETURN();
        }

        bool operator()(const auction_window_reward_operation& op0) const {
            OP_COPY(op.comment_author, op.comment_hashlink);
            PERM(op.comment_permlink, op.comment_author, op.comment_hashlink);
            OP_RETURN();
        }

        bool operator()(const comment_reward_operation& op0) const {
            OP_COPY(op.author, op.hashlink);
            PERM(op.permlink, op.author, op.hashlink);
            OP_RETURN();
        }

        bool operator()(const comment_payout_update_operation& op0) const {
            OP_COPY(op.author, op.hashlink);
            PERM(op.permlink, op.author, op.hashlink);
            OP_RETURN();
        }

        bool operator()(const comment_benefactor_reward_operation& op0) const {
            OP_COPY(op.author, op.hashlink);
            PERM(op.permlink, op.author, op.hashlink);
            OP_RETURN();
        }

        bool operator()(const total_comment_reward_operation& op0) const {
            OP_COPY(op.author, op.hashlink);
            PERM(op.permlink, op.author, op.hashlink);
            OP_RETURN();
        }

        bool operator()(const worker_reward_operation& op0) const {
            OP_COPY(op.worker_request_author, op.worker_request_hashlink);
            PERM(op.worker_request_permlink, op.worker_request_author, op.worker_request_hashlink);
            OP_RETURN();
        }

        bool operator()(const worker_state_operation& op0) const {
            OP_COPY(op.author, op.hashlink);
            PERM(op.permlink, op.author, op.hashlink);
            OP_RETURN();
        }

        bool operator()(const comment_feed_operation& op0) const {
            OP_COPY(op.author, op.hashlink);
            PERM(op.permlink, op.author, op.hashlink);
            PERM_PARENT(op.parent_permlink, op.parent_author, op.parent_hashlink);
            OP_RETURN();
        }

        bool operator()(const comment_reply_operation& op0) const {
            OP_COPY(op.author, op.hashlink);
            PERM(op.permlink, op.author, op.hashlink);
            PERM_PARENT(op.parent_permlink, op.parent_author, op.parent_hashlink);
            OP_RETURN();
        }

        bool operator()(const comment_mention_operation& op0) const {
            OP_COPY(op.author, op.hashlink);
            PERM(op.permlink, op.author, op.hashlink);
            PERM_PARENT(op.parent_permlink, op.parent_author, op.parent_hashlink);
            OP_RETURN();
        }
    };

} } // golos::api
