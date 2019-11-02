#pragma once

#include <golos/protocol/asset.hpp>
#include <golos/protocol/authority.hpp>
#include <golos/protocol/base.hpp>

namespace golos { namespace protocol {

    enum class worker_request_state {
        created,
        payment,
        payment_complete,
        closed_by_author,
        closed_by_expiration,
        closed_by_voters,
        _size
    };

    struct worker_request_operation : public base_operation {
        account_name_type author;
        std::string permlink;
        account_name_type worker;
        asset required_amount_min;
        asset required_amount_max;
        uint32_t duration;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(author);
        }
    };

    struct worker_request_delete_operation : public base_operation {
        account_name_type author;
        std::string permlink;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(author);
        }
    };

    struct worker_request_vote_operation : public base_operation {
        account_name_type voter;
        account_name_type author;
        std::string permlink;
        int16_t vote_percent;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(voter);
        }
    };

    struct worker_fund_operation : public base_operation {
        account_name_type sponsor;
        asset amount;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(sponsor);
        }
    };

} } // golos::protocol

FC_REFLECT_ENUM(
    golos::protocol::worker_request_state,
    (created)(payment)(payment_complete)(closed_by_author)(closed_by_expiration)(closed_by_voters)(_size))

FC_REFLECT(
    (golos::protocol::worker_request_operation),
    (author)(permlink)(worker)(required_amount_min)(required_amount_max)(duration)(extensions))

FC_REFLECT(
    (golos::protocol::worker_request_delete_operation),
    (author)(permlink)(extensions))

FC_REFLECT(
    (golos::protocol::worker_request_vote_operation),
    (voter)(author)(permlink)(vote_percent)(extensions))

FC_REFLECT(
    (golos::protocol::worker_fund_operation),
    (sponsor)(amount)(extensions))
