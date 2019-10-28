#include <golos/protocol/worker_operations.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/validate_helper.hpp>

namespace golos { namespace protocol {

    void worker_request_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));

        GOLOS_CHECK_PARAM(specification_cost, {
            GOLOS_CHECK_ASSET_GOLOS(specification_cost);
            GOLOS_CHECK_VALUE_GE(specification_cost.amount, 0);
        });
        GOLOS_CHECK_PARAM(development_cost, {
            GOLOS_CHECK_ASSET_GOLOS(development_cost);
            GOLOS_CHECK_VALUE_GE(development_cost.amount, 0);
        });

        GOLOS_CHECK_PARAM_ACCOUNT(worker);

        GOLOS_CHECK_PARAM(payments_count, {
            GOLOS_CHECK_VALUE_GE(payments_count, 1);
        });
        GOLOS_CHECK_PARAM(payments_interval, {
            auto day_sec = fc::days(1).to_seconds();

            GOLOS_CHECK_VALUE_GE(payments_interval, day_sec);

            if (payments_count == 1) {
                GOLOS_CHECK_VALUE_EQ(payments_interval, day_sec);
            }
        });
    }

    void worker_request_delete_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));
    }

    void worker_request_approve_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(approver);
        GOLOS_CHECK_PARAM_ACCOUNT(author);
        GOLOS_CHECK_PARAM(permlink, validate_permlink(permlink));

        GOLOS_CHECK_PARAM(state, {
            GOLOS_CHECK_VALUE(state < worker_request_approve_state::_size, "This value is reserved");
        });
    }

    void worker_fund_operation::validate() const {
        GOLOS_CHECK_PARAM_ACCOUNT(sponsor);
        GOLOS_CHECK_PARAM(amount, GOLOS_CHECK_ASSET_GT0(amount, GOLOS));
    }

} } // golos::protocol