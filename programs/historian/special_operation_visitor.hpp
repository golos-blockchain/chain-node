#pragma once

#include <functional>

#include <golos/chain/database.hpp>

#include "./program_options.hpp"

class special_operation_visitor {
private:
    const program_options& po;
    std::function<void(const std::string&)> save_found;
public:
    using result_type = void;
    special_operation_visitor(const program_options& _po,
            std::function<void(const std::string&)> _save_found)
            : po(_po), save_found(_save_found) {
    }

    void operator()(const golos::protocol::custom_json_operation& op) const {
        if (po.search_text.size()) {
            return;
        }
        fc::variant v = fc::json::from_string(op.json);
        if (!v.is_array() || !v.size() || v.get_array()[0].is_array()) return;
        auto op_type = v.get_array()[0].get_string();
        if (op_type.find(po.search_op) != std::string::npos) {
            auto& vo = v.get_array()[1].get_object();
            save_found(op_type + " " + fc::json::to_string(vo));
        }
    }

    bool check_for_compomise(const golos::protocol::operation& op, const std::string& field) const {
        if (!po.search_text.size()) {
            return true;
        }
        if (field.find(po.search_text) != std::string::npos) {
            save_found(fc::json::to_string(op));
            return true;
        }
        return false;
    }

    void operator()(const golos::protocol::comment_operation& op) const {
        if (check_for_compomise(op, op.title)) return;
        if (check_for_compomise(op, op.body)) return;
        if (check_for_compomise(op, op.json_metadata)) return;
    }

    void operator()(const golos::protocol::transfer_operation& op) const {
        if (check_for_compomise(op, op.memo)) return;
    }

    void operator()(const golos::protocol::transfer_to_savings_operation& op) const {
        if (check_for_compomise(op, op.memo)) return;
    }

    void operator()(const golos::protocol::transfer_from_savings_operation& op) const {
        if (check_for_compomise(op, op.memo)) return;
    }

    void operator()(const golos::protocol::donate_operation& op) const {
        if (!op.memo.comment) return;
        if (check_for_compomise(op, *op.memo.comment)) return;
    }

    void operator()(const golos::protocol::transfer_to_tip_operation& op) const {
        if (check_for_compomise(op, op.memo)) return;
    }

    void operator()(const golos::protocol::transfer_from_tip_operation& op) const {
        if (check_for_compomise(op, op.memo)) return;
    }

    void operator()(const golos::protocol::override_transfer_operation& op) const {
        if (check_for_compomise(op, op.memo)) return;
    }

    void operator()(const golos::protocol::invite_donate_operation& op) const {
        if (check_for_compomise(op, op.memo)) return;
    }

    void operator()(const golos::protocol::invite_transfer_operation& op) const {
        if (check_for_compomise(op, op.memo)) return;
    }

    template<typename Op>
    void operator()(Op&&) const {
    } /// ignore all other ops
};
