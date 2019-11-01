#pragma once

#include <golos/protocol/exceptions.hpp>

// State exceptions and related

#define REQUEST_STATE worker_request_state

#define CHECK_REQUEST_STATE(EXPR, MSG) \
    GOLOS_CHECK_LOGIC(EXPR, \
        logic_exception::incorrect_request_state, \
        MSG)

// Some another helpers

#define CHECK_NO_VOTE_REPEAT(STATE1, STATE2) \
    GOLOS_CHECK_LOGIC(STATE1 != STATE2, \
        logic_exception::already_voted_in_similar_way, \
        "You already have voted for this object with this state")

#define CHECK_POST(POST) \
    GOLOS_CHECK_LOGIC(POST.parent_author == STEEMIT_ROOT_POST_PARENT, \
        logic_exception::post_is_not_root, \
        "Can be created only on root post"); \
    GOLOS_CHECK_LOGIC(POST.cashout_time != fc::time_point_sec::maximum(), \
        logic_exception::post_should_be_in_cashout_window, \
        "Post should be in cashout window")

#define CHECK_APPROVER_WITNESS(APPROVER) \
    auto approver_witness = _db.get_witness(APPROVER); \
    GOLOS_CHECK_LOGIC(approver_witness.schedule == witness_object::top19, \
        logic_exception::approver_is_not_top19_witness, \
        "Approver should be in Top 19 of witnesses")
