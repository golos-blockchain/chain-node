#pragma once

#include <golos/protocol/asset.hpp>
#include <golos/protocol/authority.hpp>
#include <golos/protocol/base.hpp>

namespace golos { namespace protocol {

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

    enum class worker_request_approve_state {
        approve,
        disapprove,
        abstain,
        _size
    };

    struct worker_request_approve_operation : public base_operation {
        account_name_type approver;
        account_name_type author;
        std::string permlink;
        worker_request_approve_state state;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(approver);
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

FC_REFLECT(
    (golos::protocol::worker_request_operation),
    (author)(permlink)(worker)(required_amount_min)(required_amount_max)(duration)(extensions))

FC_REFLECT(
    (golos::protocol::worker_request_delete_operation),
    (author)(permlink)(extensions))

FC_REFLECT_ENUM(golos::protocol::worker_request_approve_state, (approve)(disapprove)(abstain)(_size))
FC_REFLECT(
    (golos::protocol::worker_request_approve_operation),
    (approver)(author)(permlink)(state)(extensions))

FC_REFLECT(
    (golos::protocol::worker_fund_operation),
    (sponsor)(amount)(extensions))
