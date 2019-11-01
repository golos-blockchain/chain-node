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
