#pragma once

#include <golos/protocol/asset.hpp>
#include <golos/protocol/authority.hpp>
#include <golos/protocol/base.hpp>

namespace golos { namespace protocol {

    struct worker_techspec_operation : public base_operation {
        account_name_type author;
        std::string permlink;
        asset specification_cost;
        asset development_cost;
        account_name_type worker;
        uint16_t payments_count;
        uint32_t payments_interval;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(author);
        }
    };

    struct worker_techspec_delete_operation : public base_operation {
        account_name_type author;
        std::string permlink;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(author);
        }
    };

    enum class worker_techspec_approve_state {
        approve,
        disapprove,
        abstain,
        _size
    };

    struct worker_techspec_approve_operation : public base_operation {
        account_name_type approver;
        account_name_type author;
        std::string permlink;
        worker_techspec_approve_state state;

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
    (golos::protocol::worker_techspec_operation),
    (author)(permlink)(specification_cost)(development_cost)
    (worker)(payments_count)(payments_interval)(extensions))

FC_REFLECT(
    (golos::protocol::worker_techspec_delete_operation),
    (author)(permlink)(extensions))

FC_REFLECT_ENUM(golos::protocol::worker_techspec_approve_state, (approve)(disapprove)(abstain)(_size))
FC_REFLECT(
    (golos::protocol::worker_techspec_approve_operation),
    (approver)(author)(permlink)(state)(extensions))

FC_REFLECT(
    (golos::protocol::worker_fund_operation),
    (sponsor)(amount)(extensions))
