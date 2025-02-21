#include <golos/plugins/database_api/plugin.hpp>
#include <golos/plugins/database_api/api_objects/account_query.hpp>
#include <golos/plugins/json_rpc/plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/protocol/get_config.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/chain/operation_notification.hpp>

#include <fc/smart_ref_impl.hpp>

#include <boost/range/iterator_range.hpp>
#include <boost/algorithm/string.hpp>
#include <memory>
#include <golos/api/callback_info.hpp>


namespace golos { namespace plugins { namespace database_api {

using golos::api::annotated_signed_block;
using golos::api::block_operations;
using golos::api::callback_info;

using block_applied_callback_info = callback_info<const signed_block&>;
using block_applied_callback = block_applied_callback_info::callback_t;
using pending_tx_callback_info = callback_info<const signed_transaction&>;
using pending_tx_callback = pending_tx_callback_info::callback_t;


struct virtual_operations {
    virtual_operations(uint32_t block_num, block_operations ops): block_num(block_num), operations(ops) {
    };

    uint32_t block_num;
    block_operations operations;
};

enum block_applied_callback_result_type {
    block       = 0,        // send signed blocks
    header      = 1,        // send only block headers
    virtual_ops = 2,        // send only virtual operations
    full        = 3         // send signed block + virtual operations
};


struct plugin::api_impl final {
public:
    api_impl();
    ~api_impl();

    void startup() {
    }

    // Subscriptions
    void set_block_applied_callback(block_applied_callback cb);
    void set_pending_tx_callback(pending_tx_callback cb);
    void clear_outdated_callbacks(bool clear_blocks);
    void op_applied_callback(const operation_notification& o);

    // Blocks and transactions
    optional<timed_block_header> get_block_header(uint32_t block_num) const;
    optional<timed_signed_block> get_block(uint32_t block_num) const;

    // Globals
    fc::variant_object get_config() const;
    dynamic_global_property_api_object get_dynamic_global_properties() const;

    // Accounts
    std::vector<account_api_object> get_accounts(const std::vector<std::string>& names, const account_query& query) const;
    std::vector<optional<account_api_object>> lookup_account_names(const std::vector<std::string> &account_names, bool include_frozen = false) const;
    std::set<std::string> lookup_accounts(const std::string &lower_bound_name, uint32_t limit, account_select_legacy select = false) const;
    uint64_t get_account_count() const;
    optional<account_recovery_request_api_object> get_recovery_request(account_name_type account) const;

    // Authority / validation
    std::set<public_key_type> get_required_signatures(const signed_transaction &trx, const flat_set<public_key_type> &available_keys) const;
    std::set<public_key_type> get_potential_signatures(const signed_transaction &trx) const;
    bool verify_authority(const signed_transaction &trx) const;
    bool verify_account_authority(const std::string &name_or_id, const flat_set<public_key_type> &signers) const;

    std::vector<withdraw_route> get_withdraw_routes(std::string account, withdraw_route_type type) const;
    std::vector<proposal_api_object> get_proposed_transactions(const std::string&, uint32_t, uint32_t) const;

    std::vector<asset_api_object> get_assets(const std::string& creator, const std::vector<std::string>& symbols, const std::string& from, uint32_t limit, asset_api_sort sort, const assets_query& query) const ;
    std::vector<account_balances_map_api_object> get_accounts_balances(const std::vector<std::string>& account_names, const balances_query& query) const;

    golos::chain::database& database() const {
        return _db;
    }

    // Callbacks
    block_applied_callback_info::cont active_block_applied_callback;
    block_applied_callback_info::cont free_block_applied_callback;
    pending_tx_callback_info::cont active_pending_tx_callback;
    pending_tx_callback_info::cont free_pending_tx_callback;

    block_operations& get_block_vops() {
        return _block_virtual_ops;
    }

private:
    golos::chain::database& _db;

    uint32_t _block_virtual_ops_block_num = 0;
    block_operations _block_virtual_ops;
};


//////////////////////////////////////////////////////////////////////
//                                                                  //
// Constructors                                                     //
//                                                                  //
//////////////////////////////////////////////////////////////////////

plugin::plugin()  {
}

plugin::~plugin() {
}

plugin::api_impl::api_impl() : _db(appbase::app().get_plugin<chain::plugin>().db()) {
    wlog("creating database plugin ${x}", ("x", int64_t(this)));
}

plugin::api_impl::~api_impl() {
    elog("freeing database plugin ${x}", ("x", int64_t(this)));
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Blocks and transactions                                          //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_API(plugin, get_block_header) {
    PLUGIN_API_VALIDATE_ARGS(
        (uint32_t, block_num)
    );
    return my->database().with_weak_read_lock([&]() {
        return my->get_block_header(block_num);
    });
}

optional<timed_block_header> plugin::api_impl::get_block_header(uint32_t block_num) const {
    auto result = database().fetch_block_by_number(block_num);
    if (result) {
        return *result;
    }
    return {};
}

DEFINE_API(plugin, get_block) {
    PLUGIN_API_VALIDATE_ARGS(
        (uint32_t, block_num)
    );
    return my->database().with_weak_read_lock([&]() {
        return my->get_block(block_num);
    });
}

optional<timed_signed_block> plugin::api_impl::get_block(uint32_t block_num) const {
    return database().fetch_block_by_number(block_num);
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Subscriptions                                                    //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_API(plugin, set_block_applied_callback) {
    auto n_args = args.args->size();
    GOLOS_ASSERT(n_args == 1, golos::invalid_arguments_count, "Expected 1 parameter, received ${n}", ("n", n_args)("required",1));

    // Use default value in case of converting errors to preserve
    // previous HF behaviour, where 1st argument can be any integer
    block_applied_callback_result_type type = block;
    auto arg = args.args->at(0);
    try {
        type = arg.as<block_applied_callback_result_type>();
    } catch (...) {
        ilog("Bad argument (${a}) passed to set_block_applied_callback, using default", ("a",arg));
    }

    // Delegate connection handlers to callback
    msg_pack_transfer transfer(args);

    my->database().with_weak_read_lock([&]{
        my->set_block_applied_callback([this,type,msg = transfer.msg()](const signed_block& block) {
            fc::variant r;
            switch (type) {
                case block_applied_callback_result_type::block:
                    r = fc::variant(block);
                    break;
                case header:
                    r = fc::variant(block_header(block));
                    break;
                case virtual_ops:
                    r = fc::variant(virtual_operations(block.block_num(), my->get_block_vops()));
                    break;
                case full:
                    r = fc::variant(annotated_signed_block(block, my->get_block_vops()));
                    break;
                default:
                    break;
            }
            msg->unsafe_result(r);
        });
    });

    transfer.complete();

    return {};
}

DEFINE_API(plugin, set_pending_transaction_callback) {
    // Delegate connection handlers to callback
    msg_pack_transfer transfer(args);
    my->database().with_weak_read_lock([&]{
        my->set_pending_tx_callback([this,msg = transfer.msg()](const signed_transaction& tx) {
            msg->unsafe_result(fc::variant(tx));
        });
    });
    transfer.complete();
    return {};
}

void plugin::api_impl::set_block_applied_callback(block_applied_callback callback) {
    auto info_ptr = std::make_shared<block_applied_callback_info>();
    active_block_applied_callback.push_back(info_ptr);
    info_ptr->it = std::prev(active_block_applied_callback.end());
    info_ptr->connect(database().applied_block, free_block_applied_callback, callback);
}

void plugin::api_impl::set_pending_tx_callback(pending_tx_callback callback) {
    auto info_ptr = std::make_shared<pending_tx_callback_info>();
    active_pending_tx_callback.push_back(info_ptr);
    info_ptr->it = std::prev(active_pending_tx_callback.end());
    info_ptr->connect(database().on_pending_transaction, free_pending_tx_callback, callback);
}

void plugin::api_impl::clear_outdated_callbacks(bool clear_blocks) {
    auto clear_bad = [&](auto& free_list, auto& active_list) {
        for (auto& info: free_list) {
            active_list.erase(info->it);
        }
        free_list.clear();
    };
    if (clear_blocks) {
        clear_bad(free_block_applied_callback, active_block_applied_callback);
    } else {
        clear_bad(free_pending_tx_callback, active_pending_tx_callback);
    }
}

void plugin::api_impl::op_applied_callback(const operation_notification& o) {
    if (o.block != _block_virtual_ops_block_num) {
        _block_virtual_ops.clear();
        _block_virtual_ops_block_num = o.block;
    }
    if (is_virtual_operation(o.op)) {
        _block_virtual_ops.push_back(o);
    }
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Globals                                                          //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_API(plugin, get_config) {
    PLUGIN_API_VALIDATE_ARGS();
    return my->database().with_weak_read_lock([&]() {
        return my->get_config();
    });
}

fc::variant_object plugin::api_impl::get_config() const {
    return golos::protocol::get_config();
}

DEFINE_API(plugin, get_dynamic_global_properties) {
    PLUGIN_API_VALIDATE_ARGS();
    return my->database().with_weak_read_lock([&]() {
        return my->get_dynamic_global_properties();
    });
}

DEFINE_API(plugin, get_chain_properties) {
    PLUGIN_API_VALIDATE_ARGS();
    return my->database().with_weak_read_lock([&]() {
        return chain_api_properties(my->database().get_witness_schedule_object().median_props, my->database());
    });
}

dynamic_global_property_api_object plugin::api_impl::get_dynamic_global_properties() const {
    return dynamic_global_property_api_object(database().get(dynamic_global_property_object::id_type()), database());
}

DEFINE_API(plugin, get_hardfork_version) {
    PLUGIN_API_VALIDATE_ARGS();
    return my->database().with_weak_read_lock([&]() {
        return my->database().get(hardfork_property_object::id_type()).current_hardfork_version;
    });
}

DEFINE_API(plugin, get_next_scheduled_hardfork) {
    PLUGIN_API_VALIDATE_ARGS();
    return my->database().with_weak_read_lock([&]() {
        scheduled_hardfork shf;
        const auto &hpo = my->database().get(hardfork_property_object::id_type());
        shf.hf_version = hpo.next_hardfork;
        shf.live_time = hpo.next_hardfork_time;
        return shf;
    });
}

//////////////////////////////////////////////////////////////////////
//                                                                  //
// Accounts                                                         //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_API(plugin, get_accounts) {
    PLUGIN_API_VALIDATE_ARGS(
        (vector<std::string>, account_names)
        (account_query, query, account_query())
    );
    return my->database().with_weak_read_lock([&]() {
        return my->get_accounts(account_names, query);
    });
}

std::vector<account_api_object> plugin::api_impl::get_accounts(const std::vector<std::string>& names, const account_query& query) const {
    const auto &idx = _db.get_index<account_index>().indices().get<by_name>();
    const auto &vidx = _db.get_index<witness_vote_index>().indices().get<by_account_witness>();
    std::vector<account_api_object> results;

    for (auto name: names) {
        auto itr = idx.find(name);
        if (itr != idx.end()) {
            results.push_back(account_api_object(*itr, _db));
            auto vitr = vidx.lower_bound(boost::make_tuple(itr->id, witness_id_type()));
            while (vitr != vidx.end() && vitr->account == itr->id) {
                results.back().witness_votes.insert(_db.get(vitr->witness).owner);
                ++vitr;
            }
            if (query.current != account_name_type()) {
                results.back().relations = golos::api::current_get_relations(
                    _db, query.current, itr->name);
            }
        }
    }

    return results;
}

DEFINE_API(plugin, lookup_account_names) {
    PLUGIN_API_VALIDATE_ARGS(
        (vector<std::string>, account_names)
        (bool, include_frozen, false)
    );
    return my->database().with_weak_read_lock([&]() {
        return my->lookup_account_names(account_names, include_frozen);
    });
}

std::vector<optional<account_api_object>> plugin::api_impl::lookup_account_names(
    const std::vector<std::string> &account_names,
    bool include_frozen
) const {
    std::vector<optional<account_api_object>> result;
    result.reserve(account_names.size());

    for (auto &name : account_names) {
        auto itr = database().find<account_object, by_name>(name);

        if (itr && (include_frozen || !itr->frozen)) {
            result.push_back(account_api_object(*itr, database()));
        }else {
            result.push_back(optional<account_api_object>());
        }
    }

    return result;
}

DEFINE_API(plugin, lookup_accounts) {
    PLUGIN_API_VALIDATE_ARGS(
        (account_name_type, lower_bound_name)
        (uint32_t,          limit)
        (account_select_legacy, select, false)
    );
    return my->database().with_weak_read_lock([&]() {
        return my->lookup_accounts(lower_bound_name, limit, select);
    });
}

std::set<std::string> plugin::api_impl::lookup_accounts(
    const std::string &lower_bound_name,
    uint32_t limit,
    account_select_legacy select
) const {
    GOLOS_CHECK_LIMIT_PARAM(limit, 1000);

    account_select_query query;
    if (select.is_bool()) {
        query.include_frozen = select.as_bool();
    } else if (select.is_null()) {
        query.include_frozen = false;
    } else if (select.is_string()) {
        query.include_frozen = (select.as_string() == "true");
    } else {
        query = select.as<account_select_query>(); // or it will throw bad cast
    }

    const auto& accounts_by_name = _db.get_index<account_index, by_name>();
    std::set<std::string> result;
    for (auto itr = accounts_by_name.lower_bound(lower_bound_name);
            limit-- && itr != accounts_by_name.end(); ++itr) {
        if ((!query.include_frozen && itr->frozen)
                || query.filter_accounts.count(itr->name)) {
            ++limit;
            continue;
        }
        result.insert(itr->name);
    }

    return result;
}

DEFINE_API(plugin, get_account_count) {
    return my->database().with_weak_read_lock([&]() {
        return my->get_account_count();
    });
}

uint64_t plugin::api_impl::get_account_count() const {
    return database().get_index<account_index>().indices().size();
}

DEFINE_API(plugin, get_owner_history) {
    PLUGIN_API_VALIDATE_ARGS(
        (string, account)
    );
    return my->database().with_weak_read_lock([&]() {
        std::vector<owner_authority_history_api_object> results;
        const auto &hist_idx = my->database().get_index<owner_authority_history_index>().indices().get<
                by_account>();
        auto itr = hist_idx.lower_bound(account);

        while (itr != hist_idx.end() && itr->account == account) {
            results.push_back(owner_authority_history_api_object(*itr));
            ++itr;
        }

        return results;
    });
}

DEFINE_API(plugin, get_recovery_request) {
    PLUGIN_API_VALIDATE_ARGS(
        (account_name_type, account)
    );
    return my->database().with_weak_read_lock([&]() {
        return my->get_recovery_request(account);
    });
}

optional<account_recovery_request_api_object> plugin::api_impl::get_recovery_request(account_name_type account) const {
    optional<account_recovery_request_api_object> result;

    const auto &rec_idx = database().get_index<account_recovery_request_index, by_account>();
    auto req = rec_idx.find(account);

    if (req != rec_idx.end()) {
        result = account_recovery_request_api_object(*req);
    }

    return result;
}

DEFINE_API(plugin, get_recovery_info) {
    PLUGIN_API_VALIDATE_ARGS(
        (account_recovery_query, query)
    );

    GOLOS_CHECK_LIMIT_PARAM(query.accounts.size(), 3);

    return my->database().with_weak_read_lock([&]() {
        account_recovery_api_object result;

        const auto& crar_idx = my->database().get_index<change_recovery_account_request_index, by_account>();

        auto now = my->database().head_block_time();

        for (const auto& acc : query.accounts) {
            const auto* account = my->database().find_account(acc);
            if (!account) {
                continue;
            }

            account_recovery_info ari;
            ari.head_block_time = now;

            if (query.fill.count(account_recovery_fill::change_partner)) {
                auto crar_itr = crar_idx.find(acc);
                if (crar_itr != crar_idx.end()) {
                    ari.change_partner_request = *crar_itr;
                }
                ari.recovery_account = account->recovery_account;
            }

            if (query.fill.count(account_recovery_fill::recovery)) {
                ari.recovery_request = my->get_recovery_request(acc);

                ari.recovery_account = account->recovery_account;
                ari.last_account_recovery = account->last_account_recovery;

                const auto& auth = my->database().get_authority(acc);
                ari.owner_authority = authority(auth.owner);
                ari.last_owner_update = auth.last_owner_update;
                ari.recovered_owner = ari.last_owner_update != fc::time_point_sec::min() && ari.last_owner_update == ari.last_account_recovery;

                ari.next_owner_update_possible = auth.last_owner_update + STEEMIT_OWNER_UPDATE_LIMIT;
                ari.can_update_owner_now = (*ari.next_owner_update_possible <= now);

                auto meta = my->database().find<account_metadata_object, by_account>(acc);
                if (meta != nullptr) {
                    ari.json_metadata = golos::chain::to_string(meta->json_metadata);
                } else {
                    ari.json_metadata = "";
                }
            }

            result[acc] = ari;
        }

        return result;
    });
}

DEFINE_API(plugin, get_escrow) {
    PLUGIN_API_VALIDATE_ARGS(
        (account_name_type, from)
        (uint32_t,          escrow_id)
    );
    return my->database().with_weak_read_lock([&]() {
        optional<escrow_api_object> result;

        try {
            result = my->database().get_escrow(from, escrow_id);
        } catch (...) {
        }

        return result;
    });
}

std::vector<withdraw_route> plugin::api_impl::get_withdraw_routes(
    std::string account,
    withdraw_route_type type
) const {
    std::vector<withdraw_route> result;

    const auto &acc = database().get_account(account);

    if (type == outgoing || type == all) {
        const auto &by_route = database().get_index<withdraw_vesting_route_index>().indices().get<
                by_withdraw_route>();
        auto route = by_route.lower_bound(acc.id);

        while (route != by_route.end() && route->from_account == acc.id) {
            withdraw_route r;
            r.from_account = account;
            r.to_account = database().get(route->to_account).name;
            r.percent = route->percent;
            r.auto_vest = route->auto_vest;

            result.push_back(r);

            ++route;
        }
    }

    if (type == incoming || type == all) {
        const auto &by_dest = database().get_index<withdraw_vesting_route_index>().indices().get<by_destination>();
        auto route = by_dest.lower_bound(acc.id);

        while (route != by_dest.end() && route->to_account == acc.id) {
            withdraw_route r;
            r.from_account = database().get(route->from_account).name;
            r.to_account = account;
            r.percent = route->percent;
            r.auto_vest = route->auto_vest;

            result.push_back(r);

            ++route;
        }
    }

    return result;
}


DEFINE_API(plugin, get_withdraw_routes) {
    PLUGIN_API_VALIDATE_ARGS(
        (string,               account)
        (withdraw_route_type,  type, incoming)
    );
    return my->database().with_weak_read_lock([&]() {
        return my->get_withdraw_routes(account, type);
    });
}

DEFINE_API(plugin, get_account_bandwidth) {
    PLUGIN_API_VALIDATE_ARGS(
        (string,         account)
        (bandwidth_type, type)
    );
    optional<account_bandwidth_api_object> result;
    auto band = my->database().find<account_bandwidth_object, by_account_bandwidth_type>(
            boost::make_tuple(account, type));
    if (band != nullptr) {
        result = *band;
    }

    return result;
}


//////////////////////////////////////////////////////////////////////
//                                                                  //
// Authority / validation                                           //
//                                                                  //
//////////////////////////////////////////////////////////////////////

DEFINE_API(plugin, get_transaction_digest) {
    PLUGIN_API_VALIDATE_ARGS(
        (transaction, trx)
    );
    static const auto chain_id = STEEMIT_CHAIN_ID;
    return trx.sig_digest(chain_id).str();
}

DEFINE_API(plugin, get_transaction_hex) {
    PLUGIN_API_VALIDATE_ARGS(
        (transaction, trx)
    );
    return fc::to_hex(fc::raw::pack(trx));

}

DEFINE_API(plugin, get_required_signatures) {
    PLUGIN_API_VALIDATE_ARGS(
        (signed_transaction,        trx)
        (flat_set<public_key_type>, available_keys)
    );
    return my->database().with_weak_read_lock([&]() {
        return my->get_required_signatures(trx, available_keys);
    });
}

std::set<public_key_type> plugin::api_impl::get_required_signatures(
    const signed_transaction &trx,
    const flat_set<public_key_type> &available_keys
) const {
    //   wdump((trx)(available_keys));
    auto result = trx.get_required_signatures(
        STEEMIT_CHAIN_ID, available_keys,
        [&](std::string account_name) {
            return authority(database().get_authority(account_name).active);
        },
        [&](std::string account_name) {
            return authority(database().get_authority(account_name).owner);
        },
        [&](std::string account_name) {
            return authority(database().get_authority(account_name).posting);
        },
        STEEMIT_MAX_SIG_CHECK_DEPTH
    );
    //   wdump((result));
    return result;
}

DEFINE_API(plugin, get_potential_signatures) {
    PLUGIN_API_VALIDATE_ARGS(
        (signed_transaction, trx)
    );
    return my->database().with_weak_read_lock([&]() {
        return my->get_potential_signatures(trx);
    });
}

std::set<public_key_type> plugin::api_impl::get_potential_signatures(const signed_transaction &trx) const {
    //   wdump((trx));
    std::set<public_key_type> result;
    trx.get_required_signatures(STEEMIT_CHAIN_ID, flat_set<public_key_type>(),
        [&](account_name_type account_name) {
            const auto &auth = database().get_authority(account_name).active;
            for (const auto &k : auth.get_keys()) {
                result.insert(k);
            }
            return authority(auth);
        },
        [&](account_name_type account_name) {
            const auto &auth = database().get_authority(account_name).owner;
            for (const auto &k : auth.get_keys()) {
                result.insert(k);
            }
            return authority(auth);
        },
        [&](account_name_type account_name) {
            const auto &auth = database().get_authority(account_name).posting;
            for (const auto &k : auth.get_keys()) {
                result.insert(k);
            }
            return authority(auth);
        },
        STEEMIT_MAX_SIG_CHECK_DEPTH
    );

    //   wdump((result));
    return result;
}

DEFINE_API(plugin, verify_authority) {
    PLUGIN_API_VALIDATE_ARGS(
        (signed_transaction, trx)
    );
    return my->database().with_weak_read_lock([&]() {
        return my->verify_authority(trx);
    });
}

bool plugin::api_impl::verify_authority(const signed_transaction &trx) const {
    trx.verify_authority(STEEMIT_CHAIN_ID, [&](std::string account_name) {
        return authority(database().get_authority(account_name).active);
    }, [&](std::string account_name) {
        return authority(database().get_authority(account_name).owner);
    }, [&](std::string account_name) {
        return authority(database().get_authority(account_name).posting);
    }, STEEMIT_MAX_SIG_CHECK_DEPTH);
    return true;
}

DEFINE_API(plugin, verify_account_authority) {
    PLUGIN_API_VALIDATE_ARGS(
        (account_name_type,         name)
        (flat_set<public_key_type>, keys)
    );
    return my->database().with_weak_read_lock([&]() {
        return my->verify_account_authority(name, keys);
    });
}

bool plugin::api_impl::verify_account_authority(
    const std::string &name,
    const flat_set<public_key_type> &keys
) const {
    GOLOS_CHECK_PARAM(name, GOLOS_CHECK_VALUE(name.size() > 0, "Account must be not empty"));
    auto account = database().get_account(name);

    /// reuse trx.verify_authority by creating a dummy transfer
    signed_transaction trx;
    transfer_operation op;
    op.from = account.name;
    trx.operations.emplace_back(op);

    return verify_authority(trx);
}

DEFINE_API(plugin, get_conversion_requests) {
    PLUGIN_API_VALIDATE_ARGS(
        (string, account)
    );
    return my->database().with_weak_read_lock([&]() {
        const auto &idx = my->database().get_index<convert_request_index>().indices().get<by_owner>();
        std::vector<convert_request_api_object> result;
        auto itr = idx.lower_bound(account);
        while (itr != idx.end() && itr->owner == account) {
            result.emplace_back(*itr);
            ++itr;
        }
        return result;
    });
}


DEFINE_API(plugin, get_savings_withdraw_from) {
    PLUGIN_API_VALIDATE_ARGS(
        (string, account)
    );
    return my->database().with_weak_read_lock([&]() {
        std::vector<savings_withdraw_api_object> result;

        const auto &from_rid_idx = my->database().get_index<savings_withdraw_index>().indices().get<by_from_rid>();
        auto itr = from_rid_idx.lower_bound(account);
        while (itr != from_rid_idx.end() && itr->from == account) {
            result.push_back(savings_withdraw_api_object(*itr));
            ++itr;
        }
        return result;
    });
}

DEFINE_API(plugin, get_savings_withdraw_to) {
    PLUGIN_API_VALIDATE_ARGS(
        (string, account)
    );
    return my->database().with_weak_read_lock([&]() {
        std::vector<savings_withdraw_api_object> result;

        const auto &to_complete_idx = my->database().get_index<savings_withdraw_index>().indices().get<by_to_complete>();
        auto itr = to_complete_idx.lower_bound(account);
        while (itr != to_complete_idx.end() && itr->to == account) {
            result.push_back(savings_withdraw_api_object(*itr));
            ++itr;
        }
        return result;
    });
}

//vector<vesting_delegation_api_obj> get_vesting_delegations(string account, string from, uint32_t limit, delegations_type type = delegated) const;
DEFINE_API(plugin, get_vesting_delegations) {
    PLUGIN_API_VALIDATE_ARGS(
        (string,           account)
        (string,           from)
        (uint32_t,         limit, 100)
        (delegations_type, type, delegated)
    );
    bool sent = type == delegated;
    GOLOS_CHECK_LIMIT_PARAM(limit, 1000);

    vector<vesting_delegation_api_object> result;
    result.reserve(limit);
    return my->database().with_weak_read_lock([&]() {
        auto fill_result = [&](const auto& idx) {
            auto i = idx.lower_bound(std::make_tuple(account, from));
            while (result.size() < limit && i != idx.end() && account == (sent ? i->delegator : i->delegatee)) {
                result.push_back(*i);
                ++i;
            }
        };
        if (sent)
            fill_result(my->database().get_index<vesting_delegation_index, by_delegation>());
        else
            fill_result(my->database().get_index<vesting_delegation_index, by_received>());
        return result;
    });
}

//vector<vesting_delegation_expiration_api_obj> get_expiring_vesting_delegations(string account, time_point_sec from, uint32_t limit = 100) const;
DEFINE_API(plugin, get_expiring_vesting_delegations) {
    PLUGIN_API_VALIDATE_ARGS(
        (string,         account)
        (time_point_sec, from)
        (uint32_t,       limit, 100)
    );
    GOLOS_CHECK_LIMIT_PARAM(limit, 1000);

    return my->database().with_weak_read_lock([&]() {
        vector<vesting_delegation_expiration_api_object> result;
        result.reserve(limit);
        const auto& idx = my->database().get_index<vesting_delegation_expiration_index, by_account_expiration>();
        auto itr = idx.lower_bound(std::make_tuple(account, from));
        while (result.size() < limit && itr != idx.end() && itr->delegator == account) {
            result.push_back(*itr);
            ++itr;
        }
        return result;
    });
}

DEFINE_API(plugin, get_database_info) {
    PLUGIN_API_VALIDATE_ARGS();
    // read lock doesn't seem needed...

    database_info info;
    auto& db = my->database();

    info.free_size = db.free_memory();
    info.total_size = db.max_memory();
    info.reserved_size = db.reserved_memory();
    info.used_size = info.total_size - info.free_size - info.reserved_size;

    info.index_list.reserve(db.index_list_size());

    for (auto it = db.index_list_begin(), et = db.index_list_end(); et != it; ++it) {
        info.index_list.push_back({(*it)->name(), (*it)->size()});
    }

    return info;
}

std::vector<proposal_api_object> plugin::api_impl::get_proposed_transactions(
    const std::string& a, uint32_t from, uint32_t limit
) const {
    std::vector<proposal_api_object> result;
    std::set<proposal_object_id_type> id_set;
    result.reserve(limit);

    // list of published proposals
    {
        auto& idx = database().get_index<proposal_index>().indices().get<by_account>();
        auto itr = idx.lower_bound(a);

        for (; idx.end() != itr && itr->author == a && result.size() < limit; ++itr) {
            id_set.insert(itr->id);
            if (id_set.size() >= from) {
                result.emplace_back(*itr);
            }
        }
    }

    // list of requested proposals
    if (result.size() < limit) {
        auto& idx = database().get_index<required_approval_index>().indices().get<by_account>();
        auto& pidx = database().get_index<proposal_index>().indices().get<by_id>();
        auto itr = idx.lower_bound(a);

        for (; idx.end() != itr && itr->account == a && result.size() < limit; ++itr) {
            if (!id_set.count(itr->proposal)) {
                id_set.insert(itr->proposal);
                auto pitr = pidx.find(itr->proposal);
                if (pidx.end() != pitr && id_set.size() >= from) {
                    result.emplace_back(*pitr);
                }
            }
        }
    }

    return result;
}

DEFINE_API(plugin, get_proposed_transactions) {
    PLUGIN_API_VALIDATE_ARGS(
        (string, account)
        (uint32_t, from)
        (uint32_t, limit)
    );
    GOLOS_CHECK_LIMIT_PARAM(limit, 100);

    return my->database().with_weak_read_lock([&]() {
        return my->get_proposed_transactions(account, from, limit);
    });
}

DEFINE_API(plugin, get_invite) {
    PLUGIN_API_VALIDATE_ARGS(
        (public_key_type,           invite_key)
    );
    return my->database().with_weak_read_lock([&]() {
        optional<invite_api_object> result;

        try {
            result = my->database().get_invite(invite_key);
        } catch (...) {
        }

        return result;
    });
}

std::vector<asset_api_object> plugin::api_impl::get_assets(
        const std::string& creator,
        const std::vector<std::string>& symbols,
        const std::string& from, uint32_t limit,
        asset_api_sort sort,
        const assets_query& query) const {
    std::vector<asset_api_object> results;
    results.reserve(limit);

    auto go_page = [&](const auto& idx, auto& itr) {
        if (from == account_name_type()) return;
        for (; itr != idx.end() && itr->symbol_name() != from; ++itr);
    };

    auto fill_assets = [&](const auto& idx, auto& itr, auto&& verify) {
        for (; itr != idx.end() && verify(itr); ++itr) {
            if (results.size() == limit) break;
            results.push_back(asset_api_object(*itr, _db));
        }
    };

    bool with_golos = query.system;
    bool with_gbg = query.system;

    auto add_golos = [&]() {
        results.push_back(asset_api_object::golos(_db));
        with_golos = false;
    };
    auto add_gbg = [&]() {
        results.push_back(asset_api_object::gbg(_db));
        with_gbg = false;
    };
    auto add_system = [&]() {
        if (with_golos) add_golos();
        if (with_gbg) add_gbg();
    };

    const auto& indices = _db.get_index<asset_index>().indices();

    if (creator.size()) {
        auto verify = [&](auto& itr) { return itr->creator == creator; };
        if (sort == asset_api_sort::by_symbol_name) {
            const auto& idx = indices.get<by_creator_symbol_name>();
            auto itr = idx.lower_bound(std::make_tuple(creator, from));

            fill_assets(idx, itr, verify);
        } else {
            const auto& idx = indices.get<by_creator_marketed>();
            auto itr = idx.lower_bound(std::make_tuple(creator));

            go_page(idx, itr);
            fill_assets(idx, itr, verify);
        }

        add_system();
    } else {
        if (symbols.size()) {
            const auto& sym_idx = indices.get<by_symbol_name>();
            for (auto symbol : symbols) {
                GOLOS_CHECK_PARAM(symbols, {
                    GOLOS_CHECK_VALUE(symbol.size() >= 3 && symbol.size() <= 14, "symbol must be between 3 and 14");
                });
                auto symbol_name = symbol;
                boost::to_upper(symbol_name);
                auto itr = sym_idx.find(symbol_name);
                if (itr != sym_idx.end()) {
                    results.push_back(asset_api_object(*itr, _db));
                } else if (symbol == "GOLOS") {
                    add_golos();
                } else if (symbol == "GBG") {
                    add_gbg();
                }
            }

            add_system();

            if (sort == asset_api_sort::by_marketed) {
                std::sort(results.begin(), results.end(), [&](auto& lhs, auto& rhs) {
                    if (lhs.marketed == rhs.marketed)
                        return lhs.id > rhs.id;
                    return lhs.marketed > rhs.marketed;
                });
            }
        } else {
            auto verify = [&](auto& itr) { return true; };
            if (sort == asset_api_sort::by_symbol_name) {
                const auto& idx = indices.get<by_symbol_name>();
                auto itr = idx.lower_bound(from);

                fill_assets(idx, itr, verify);
            } else {
                const auto& idx = indices.get<by_marketed>();
                auto itr = idx.begin();

                go_page(idx, itr);
                fill_assets(idx, itr, verify);
            }

            add_system();
        }
    }

    return results;
}

std::vector<asset_api_object> plugin::get_assets_inner(
    const std::string& creator,
    const std::vector<std::string>& symbols,
    const std::string& from, uint32_t limit,
    asset_api_sort sort,
    const assets_query& query) const {
    return my->database().with_weak_read_lock([&]() {
        return my->get_assets(creator, symbols, from, limit, sort, query);
    });
}

DEFINE_API(plugin, get_assets) {
    PLUGIN_API_VALIDATE_ARGS(
        (std::string, creator, std::string())
        (std::vector<std::string>, symbols, std::vector<std::string>())
        (std::string, from, std::string())
        (uint32_t, limit, 20)
        (asset_api_sort, sort, asset_api_sort::by_symbol_name)
        (assets_query, query, assets_query())
    );
    GOLOS_CHECK_LIMIT_PARAM(limit, 5000);

    return get_assets_inner(creator, symbols, from, limit, sort, query);
}

std::vector<account_balances_map_api_object> plugin::api_impl::get_accounts_balances(const std::vector<std::string>& account_names, const balances_query& query) const {
    std::vector<account_balances_map_api_object> results;

    const auto& token_holders = query.token_holders;
    if (token_holders.size()) {
        auto sym = _db.get_asset(token_holders).symbol();

        const auto& idx = _db.get_index<account_balance_index, by_symbol_account>();
        
        auto itr = idx.find(sym);
        uint32_t i = 0;
        for (; itr != idx.end() && itr->symbol() == sym; ++itr, ++i) {
            if (itr->balance.amount != 0 || itr->tip_balance.amount != 0
                    || itr->market_balance.amount != 0) {
                account_balances_map_api_object acc_balances;
                acc_balances[token_holders] = account_balance_api_object(*itr);
                results.push_back(acc_balances);
            }

            if (results.size() == 500 || (i > 10000 && results.size())) {
                break;
            }
        }

        std::sort(results.begin(), results.end(), [&](auto& lhs, auto& rhs) {
            auto lbal = lhs.begin()->second;
            auto rbal = rhs.begin()->second;
            auto lsum = lbal.balance + lbal.tip_balance + lbal.market_balance;
            auto rsum = rbal.balance + rbal.tip_balance + rbal.market_balance;
            return lsum > rsum;
        });

        return results;
    }

    const auto& idx = _db.get_index<account_balance_index, by_account_symbol>();

    for (auto account_name : account_names) {
        account_balances_map_api_object acc_balances;

        const auto& symbols = query.symbols;

        auto itr = idx.lower_bound(account_name);
        for (; itr != idx.end() && itr->account == account_name; ++itr) {
            if (symbols.size()) {
                const auto& sym = itr->balance.symbol_name();
                if (symbols.find(sym) == symbols.end()) {
                    continue;
                }
            }
            acc_balances[itr->balance.symbol_name()] = account_balance_api_object(*itr);
        }

        auto add_golos = [&]() {
            acc_balances["GOLOS"] = account_balance_api_object::golos(account_name, _db);
        };
        auto add_gbg = [&]() {
            acc_balances["GBG"] = account_balance_api_object::gbg(account_name, _db);
        };

        if (query.system) {
            add_golos();
            add_gbg();
        } else if (symbols.size()) {
            if (symbols.find("GOLOS") != symbols.end()) {
                add_golos();
            }
            if (symbols.find("GBG") != symbols.end()) {
                add_gbg();
            }
        }

        results.push_back(acc_balances);
    }

    return results;
}

DEFINE_API(plugin, get_accounts_balances) {
    PLUGIN_API_VALIDATE_ARGS(
        (vector<std::string>, account_names)
        (balances_query, query, balances_query())
    );
    return my->database().with_weak_read_lock([&]() {
        return my->get_accounts_balances(account_names, query);
    });
}

void plugin::plugin_initialize(const boost::program_options::variables_map& options) {
    ilog("database_api plugin: plugin_initialize() begin");
    my = std::make_unique<api_impl>();
    JSON_RPC_REGISTER_API(plugin_name)
    auto& db = my->database();
    db.applied_block.connect([&](const signed_block&) {
        my->clear_outdated_callbacks(true);
    });
    db.on_pending_transaction.connect([&](const signed_transaction& tx) {
        my->clear_outdated_callbacks(false);
    });
    db.pre_apply_operation.connect([&](const operation_notification& o) {
        my->op_applied_callback(o);
    });
    ilog("database_api plugin: plugin_initialize() end");
}

void plugin::plugin_startup() {
    my->startup();
}

} } } // golos::plugins::database_api

FC_REFLECT((golos::plugins::database_api::virtual_operations), (block_num)(operations))
FC_REFLECT_ENUM(golos::plugins::database_api::block_applied_callback_result_type,
    (block)(header)(virtual_ops)(full))
