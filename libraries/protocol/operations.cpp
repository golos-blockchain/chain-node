#include <golos/protocol/operations.hpp>

#include <golos/protocol/operation_util_impl.hpp>

namespace golos {
    namespace protocol {

        struct is_market_op_visitor {
            typedef bool result_type;

            template<typename T>
            bool operator()(T &&v) const {
                return false;
            }

            bool operator()(const limit_order_create_operation &) const {
                return true;
            }

            bool operator()(const limit_order_cancel_operation &) const {
                return true;
            }

            bool operator()(const limit_order_cancel_ex_operation &) const {
                return true;
            }

            bool operator()(const transfer_operation &) const {
                return true;
            }

            bool operator()(const transfer_to_vesting_operation &) const {
                return true;
            }

            bool operator()(const claim_operation &) const {
                return true;
            }

            bool operator()(const donate_operation &) const {
                return true;
            }

            bool operator()(const invite_claim_operation &) const {
                return true;
            }

            bool operator()(const invite_donate_operation &) const {
                return true;
            }

            bool operator()(const invite_transfer_operation &) const {
                return true;
            }

            bool operator()(const paid_subscription_create_operation &) const {
                return true;
            }

            bool operator()(const paid_subscription_transfer_operation &) const {
                return true;
            }
        };

        bool is_market_operation(const operation &op) {
            return op.visit(is_market_op_visitor());
        }

        struct is_vop_visitor {
            typedef bool result_type;

            template<typename T>
            bool operator()(const T &v) const {
                return v.is_virtual();
            }
        };

        bool is_virtual_operation(const operation &op) {
            return op.visit(is_vop_visitor());
        }

        struct is_custom_json_op_visitor {
            typedef bool result_type;

            template<typename T>
            bool operator()(T&& v) const {
                return false;
            }

            bool operator()(const custom_json_operation&) const {
                return true;
            }
        };

        bool is_custom_json_operation(const operation& op) {
            return op.visit(is_custom_json_op_visitor());
        }

        struct is_active_op_visitor {
            typedef bool result_type;

            template<typename T>
            result_type operator()(T&& v) const {
                flat_set<account_name_type> active;
                v.get_required_active_authorities(active);
                if (active.size()) {
                    return true;
                }
                return false;
            }
        };

        bool is_active_operation(const operation& op) {
            return op.visit(is_active_op_visitor());
        }
    }
} // golos::protocol

DEFINE_OPERATION_TYPE(golos::protocol::operation)
