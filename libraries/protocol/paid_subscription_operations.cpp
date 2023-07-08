#include <golos/protocol/paid_subscription_operations.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/validate_helper.hpp>

namespace golos { namespace protocol {

    void paid_subscription_id::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(app);
        GOLOS_CHECK_PARAM_ACCOUNT(name);
    }

    void paid_subscription_create_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        oid.validate();
        GOLOS_CHECK_PARAM(cost, {
            GOLOS_CHECK_VALUE(cost.amount > 0, "Cost should not be zero");
            GOLOS_CHECK_VALUE(cost.symbol != VESTS_SYMBOL, "Cost cannot be GESTS");
            if (tip_cost) {
                GOLOS_CHECK_VALUE(cost.symbol != SBD_SYMBOL, "Cost cannot be GBG, if tip_cost is true");
            }
        });
        GOLOS_CHECK_PARAM(allow_prepaid, {
            if (allow_prepaid)
                GOLOS_CHECK_VALUE(executions != 0, "Single-executed subscriptions cannot be prepaid");
        });
        GOLOS_CHECK_PARAM(interval, {
            if (executions > 0)
                GOLOS_CHECK_VALUE(interval > 0, "interval cannot be 0 if executions is not 0");
        });
    }

    void paid_subscription_update_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        oid.validate();
        GOLOS_CHECK_PARAM(cost, {
            GOLOS_CHECK_VALUE(cost.amount > 0, "Cost should not be zero");
            GOLOS_CHECK_VALUE(cost.symbol != VESTS_SYMBOL, "Cost cannot be GESTS");
            if (tip_cost) {
                GOLOS_CHECK_VALUE(cost.symbol != SBD_SYMBOL, "Cost cannot be GBG, if tip_cost is true");
            }
        });
        GOLOS_CHECK_PARAM(interval, {
            if (executions > 0)
                GOLOS_CHECK_VALUE(interval > 0, "interval cannot be 0 if executions is not 0");
        });
    }

    void paid_subscription_delete_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        oid.validate();
    }

    void paid_subscription_transfer_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(from);
        GOLOS_CHECK_PARAM_ACCOUNT(to);
        oid.validate();
        GOLOS_CHECK_PARAM(amount, {
            GOLOS_CHECK_VALUE(amount.amount > 0, "Cannot transfer a negative amount (aka: stealing)");
        });
        GOLOS_CHECK_PARAM(memo, {
            GOLOS_CHECK_VALUE(memo.size() < STEEMIT_MAX_MEMO_SIZE, "Memo is too large");
            GOLOS_CHECK_VALUE(fc::is_utf8(memo), "Memo is not UTF8");
        });
    }

    void paid_subscription_cancel_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(subscriber);
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        oid.validate();
    }

} } // golos::protocol
