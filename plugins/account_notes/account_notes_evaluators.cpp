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

std::pair<account_name_type, std::set<account_name_type>> get_global_value_accs(const database& _db, std::string key) {
    std::pair<account_name_type, std::set<account_name_type>> res;

    const auto& notes_idx = _db.get_index<account_note_index, by_global_key>();
    auto notes_itr = notes_idx.find(key + ".accs");
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
        auto is_accs = boost::algorithm::ends_with(op.key, ".accs");
        auto is_lst = boost::algorithm::ends_with(op.key, ".lst");

        auto acc = op.account;
        if (is_global) {
            auto gv_accs = get_global_value_accs(_db, op.key);
            if (gv_accs.first != account_name_type()) {
                if (gv_accs.first != acc && !gv_accs.second.count(acc)) return;
                acc = gv_accs.first;
            }
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

        auto res_str = op.value;
        if (is_accs) {
            auto v = fc::json::from_string(op.value);
            std::set<account_name_type> value_accs;
            fc::from_variant(v, value_accs);
            std::set<account_name_type> res_accs;
            for (auto& ac : value_accs) {
                if (_db.find_account(ac)) {
                    res_accs.insert(ac);
                }
            }
            res_str = fc::json::to_string(res_accs);
        } else if (is_lst) {
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
        }

        if (res_str.size() > settings_->max_value_length) {
            return;
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
