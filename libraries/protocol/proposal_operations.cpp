#include <golos/protocol/proposal_operations.hpp>
#include <golos/protocol/operations.hpp>
#include <golos/protocol/types.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/validate_helper.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/time.hpp>

// TODO: move somewhere globally accessible
#define GOLOS_PROPOSAL_MAX_TITLE_SIZE   256
#define GOLOS_PROPOSAL_MAX_MEMO_SIZE    4096


namespace golos { namespace protocol {

    void proposal_create_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        GOLOS_CHECK_PARAM(title, {
            GOLOS_CHECK_VALUE_NOT_EMPTY(title);
            GOLOS_CHECK_VALUE_MAX_SIZE(title, GOLOS_PROPOSAL_MAX_TITLE_SIZE);
            GOLOS_CHECK_VALUE_UTF8(title);
        });

        GOLOS_CHECK_PARAM(proposed_operations, {
            GOLOS_CHECK_VALUE(!proposed_operations.empty(), "proposed_operations can't be empty");
            for (const auto& op : proposed_operations) {
                operation_validate(op.op);
            }
        });

        GOLOS_CHECK_PARAM(memo, {
            if (memo.size() > 0) {
                GOLOS_CHECK_VALUE_MAX_SIZE(memo, GOLOS_PROPOSAL_MAX_MEMO_SIZE); // "Memo larger than size limit"
                GOLOS_CHECK_VALUE_UTF8(memo);
            }
        });

        // TODO: time in past can be validated (expiration_time and review_period_time)
        // TODO: combination of posting+active ops van be validated
    }

    void proposal_update_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        GOLOS_CHECK_PARAM(title, {
            GOLOS_CHECK_VALUE_NOT_EMPTY(title);
            GOLOS_CHECK_VALUE_MAX_SIZE(title, GOLOS_PROPOSAL_MAX_TITLE_SIZE);
            GOLOS_CHECK_VALUE_UTF8(title);
        });

        GOLOS_CHECK_LOGIC(0 < (
            owner_approvals_to_add.size()   + owner_approvals_to_remove.size() +
            active_approvals_to_add.size()  + active_approvals_to_remove.size() +
            posting_approvals_to_add.size() + posting_approvals_to_remove.size() +
            key_approvals_to_add.size()     + key_approvals_to_remove.size()),
            logic_exception::empty_approvals,
            "At least one approval add or approval remove must exist");

        auto validate = [&](const auto& to_add, const auto& to_remove) {
            for (const auto& a: to_add) {
                GOLOS_CHECK_LOGIC(to_remove.find(a) == to_remove.end(),
                    logic_exception::add_and_remove_same_approval,
                    "Cannot add and remove approval at the same time");
            }
        };

        validate(owner_approvals_to_add, owner_approvals_to_remove);
        validate(active_approvals_to_add, active_approvals_to_remove);
        validate(posting_approvals_to_add, posting_approvals_to_remove);
        validate(key_approvals_to_add, key_approvals_to_remove);
    }

    void proposal_update_operation::get_required_authorities(std::vector<authority>& o) const {
        authority auth;
        for (const auto& k: key_approvals_to_add) {
            auth.key_auths[k] = 1;
        }
        for (const auto& k: key_approvals_to_remove) {
            auth.key_auths[k] = 1;
        }
        auth.weight_threshold = auth.key_auths.size();

        if (auth.key_auths.size() > 0) {
            o.emplace_back(std::move(auth));
        }
    }

    void proposal_update_operation::get_required_active_authorities(flat_set<account_name_type>& a) const {
        a.insert(active_approvals_to_add.begin(), active_approvals_to_add.end());
        a.insert(active_approvals_to_remove.begin(), active_approvals_to_remove.end());
    }

    void proposal_update_operation::get_required_owner_authorities(flat_set<account_name_type>& a) const {
        a.insert(owner_approvals_to_add.begin(), owner_approvals_to_add.end());
        a.insert(owner_approvals_to_remove.begin(), owner_approvals_to_remove.end());
    }

    void proposal_update_operation::get_required_posting_authorities(flat_set<account_name_type>& a) const {
        a.insert(posting_approvals_to_add.begin(), posting_approvals_to_add.end());
        a.insert(posting_approvals_to_remove.begin(), posting_approvals_to_remove.end());
    }

    void proposal_delete_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(requester);
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        GOLOS_CHECK_PARAM(title, {
            GOLOS_CHECK_VALUE_NOT_EMPTY(title);
            GOLOS_CHECK_VALUE_MAX_SIZE(title, GOLOS_PROPOSAL_MAX_TITLE_SIZE);
            GOLOS_CHECK_VALUE_UTF8(title);
        });
    }

} } // golos::chain
