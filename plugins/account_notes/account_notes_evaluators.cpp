#include <golos/plugins/account_notes/account_notes_operations.hpp>
#include <golos/plugins/account_notes/account_notes_objects.hpp>
#include <golos/plugins/account_notes/account_notes_evaluators.hpp>
#include <golos/chain/account_object.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace golos { namespace plugins { namespace account_notes {

using golos::chain::from_string;

bool set_value_evaluator::is_tracked_account(const account_name_type& account) {
    // Check if account occures in whitelist if whitelist is set in config
    if (!settings_->tracked_accounts.empty()
        && settings_->tracked_accounts.find(std::string(account)) == settings_->tracked_accounts.end()) {
        return false;
    }

    if (settings_->untracked_accounts.find(std::string(account)) != settings_->untracked_accounts.end()) {
        return false;
    }

    return true;
}

void merge_objects(fc::mutable_variant_object& res, const fc::variant_object& add) {
    for (auto& entry : add ) {
        if (entry.value().is_null()) {
            res.erase(entry.key());
            continue;
        }
        res[entry.key()] = entry.value();
    }
}

std::pair<account_name_type, std::set<account_name_type>> get_global_value_acs(const database& _db, std::string key) {
    std::pair<account_name_type, std::set<account_name_type>> res;

    const auto& notes_idx = _db.get_index<account_note_index, by_global_key>();
    auto notes_itr = notes_idx.find(key + ".acs");
    if (notes_itr != notes_idx.end()) {
        res.first = notes_itr->account;
        try {
            auto v = fc::json::from_string(to_string(notes_itr->value));
            fc::from_variant(v, res.second);
        } catch (...) {
            // Incorrect JSON = no acs
        }
    }

    return res;
}

void set_value_evaluator::do_apply(const set_value_operation& op) {
    try {
        if (op.key.size() < 1 || op.key.size() > settings_->max_key_length) {
            return;
        }

        // TODO: optimize
        auto is_global = boost::algorithm::starts_with(op.key, "g.");
        auto is_acs = boost::algorithm::ends_with(op.key, ".acs");
        auto is_lst = boost::algorithm::ends_with(op.key, ".lst");

        auto acc = op.account;
        if (is_global) {
            auto gv_acs = get_global_value_acs(_db, op.key);
            if (gv_acs.first != account_name_type() && gv_acs.first != acc && !gv_acs.second.count(acc)) {
                return;
            }
            acc = gv_acs.first;
        }

        if (!is_tracked_account(acc)) {
            return;
        };

        const auto& notes_idx = _db.get_index<account_note_index, by_account_key>();
        auto notes_itr = notes_idx.find(std::make_tuple(acc, op.key));

        const auto& stats_idx = _db.get_index<account_note_stats_index, by_account>();
        auto stats_itr = stats_idx.find(acc);

        if (op.value.empty()) { // Delete case
            if (notes_itr != notes_idx.end()) {
                _db.remove(*notes_itr);

                _db.modify(*stats_itr, [&](auto& ns) {
                    ns.note_count--;
                });
            }
            return;
        }

        if (is_acs) {
            auto v = fc::json::from_string(op.value);
            std::set<account_name_type> value_acs;
            fc::from_variant(v, value_acs);
            for (auto& ac : value_acs) {
                _db.get_account(ac);
            }
        }

        auto res_str = op.value;
        if (is_lst) {
            fc::mutable_variant_object res;
            auto add = fc::json::from_string(op.value).get_object();
            if (add.size()) {
                if (notes_itr != notes_idx.end()) {
                    try {
                        res = fc::json::from_string(to_string(notes_itr->value)).get_object();
                    } catch (...) {
                        // If cannot parse - just recreate it as JSON
                    }
                }
                merge_objects(res, add);
                res_str = fc::json::to_string(res);
            } else {
                res_str = "{}";
            }

            if (res_str.size() > settings_->max_value_length) {
                return;
            }
        }

        if (notes_itr != notes_idx.end()) { // Edit case
            _db.modify(*notes_itr, [&](auto& n) {
                from_string(n.value, res_str);
            });
        } else { // Create case
            if (stats_itr != stats_idx.end()) {
                if (stats_itr->note_count >= settings_->max_note_count) {
                    return;
                }

                _db.modify(*stats_itr, [&](auto& ns) {
                    ns.note_count++;
                });
            } else {
                _db.create<account_note_stats_object>([&](auto& n) {
                    n.account = acc;
                    n.note_count = 1;
                });
            }

            _db.create<account_note_object>([&](auto& n) {
                n.account = acc;
                from_string(n.key, op.key);
                from_string(n.value, res_str);
            });
        }
    }
    FC_CAPTURE_AND_RETHROW((op))
}

} } } // golos::plugins::account_notes
