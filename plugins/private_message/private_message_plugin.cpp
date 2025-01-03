#include <golos/plugins/private_message/private_message_plugin.hpp>
#include <golos/plugins/private_message/private_message_evaluators.hpp>
#include <golos/plugins/private_message/private_message_objects.hpp>
#include <golos/plugins/private_message/private_message_queries.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/cryptor/cryptor.hpp>
#include <appbase/application.hpp>
#include <golos/protocol/donate_targets.hpp>
#include <golos/api/account_api_object.hpp>

#include <golos/chain/index.hpp>
#include <golos/chain/custom_operation_interpreter.hpp>
#include <golos/chain/generic_custom_operation_interpreter.hpp>
#include <golos/chain/operation_notification.hpp>

#include <fc/smart_ref_impl.hpp>

#include <mutex>

//
template<typename T>
T dejsonify(const std::string &s) {
    return fc::json::from_string(s).as<T>();
}

#define LOAD_VALUE_SET(options, name, container, type) \
if( options.count(name) ) { \
    const std::vector<std::string>& ops = options[name].as<std::vector<std::string>>(); \
    std::transform(ops.begin(), ops.end(), std::inserter(container, container.end()), &dejsonify<type>); \
}
//

namespace golos { namespace plugins { namespace private_message {

    struct api_result {
        std::string status = "ok";
        std::string login_error;
        std::string error;
        uint32_t decrypt_processed = 0;
    };

    struct callback_info final {
        callback_query query;
        std::shared_ptr<json_rpc::msg_pack> msg;

        callback_info() = default;
        callback_info(callback_query&& q, std::shared_ptr<json_rpc::msg_pack> m)
            : query(q),
              msg(m) {

        }
    };

    struct post_operation_visitor {
        golos::chain::database& _db;

        post_operation_visitor(golos::chain::database& db) : _db(db) {
        }

        using result_type = void;

        template<typename T>
        result_type operator()(const T&) const {
        }

        result_type operator()(const authority_updated_operation& op) const {
            if (!op.memo_key.valid())
                return;

            const auto& idx = _db.get_index<contact_index, by_owner>();
            auto itr = idx.find(op.account);
            for (; itr != idx.end() && itr->owner == op.account; ++itr) {
                _db.modify(*itr, [&](auto& pco) {
                    pco.is_hidden = true;
                });
            }
        }

        result_type operator()(const donate_operation& op) const {
            try {
                auto md = get_message_donate(op);
                if (!md || md->wrong) {
                    return;
                }

                auto& id_idx = _db.get_index<message_index, by_nonce>();
                auto id_itr = id_idx.find(std::make_tuple(md->group, md->from, md->to, md->nonce));
                if (id_itr != id_idx.end()) {
                    _db.modify(*id_itr, [&](auto& o) {
                        if (op.amount.symbol == STEEM_SYMBOL) {
                            o.donates += op.amount;
                        } else {
                            o.donates_uia += (op.amount.amount / op.amount.precision());
                        }
                    });
                }
            } catch (...) {}
        }
    };

    class private_message_plugin::private_message_plugin_impl final {
    public:
        private_message_plugin_impl(private_message_plugin& plugin)
            : _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {

            custom_operation_interpreter_ = std::make_shared
                    <generic_custom_operation_interpreter<private_message::private_message_plugin_operation>>(_db);

            auto coi = custom_operation_interpreter_.get();

            coi->register_evaluator<private_message_evaluator>(&plugin);
            coi->register_evaluator<private_delete_message_evaluator>(&plugin);
            coi->register_evaluator<private_mark_message_evaluator>(&plugin);
            coi->register_evaluator<private_settings_evaluator>(&plugin);
            coi->register_evaluator<private_contact_evaluator>(&plugin);
            coi->register_evaluator<private_group_delete_evaluator>(&plugin);
            coi->register_evaluator<private_group_evaluator>(&plugin);
            coi->register_evaluator<private_group_member_evaluator>(&plugin);

            _db.set_custom_operation_interpreter(plugin.name(), custom_operation_interpreter_);
        }

        void post_operation(const operation_notification& note) const;

        message_account_api_object get_msg_account(account_name_type account, account_name_type current = account_name_type(), std::string group = "") const;
        bool fill_msg_account(message_accounts& accounts, account_name_type account, account_name_type current = account_name_type(), std::string group = "") const;

        template <typename Direction, typename Filter>
        std::vector<message_api_object> get_message_box(
            const std::string& account, const message_box_query&, Filter&&) const;

        template<typename OutItr, typename InItr>
        void get_messages_bidirectional(const message_thread_query& query,
            std::vector<message_api_object>& result,
            OutItr& out_itr, const OutItr& out_etr,
            InItr& in_itr, const InItr& in_etr,
            std::function<bool (const message_object& msg)> filter) const;

        api_result decrypt_messages(std::vector<message_api_object*>& ptrs, const login_data& query, decrypt_ignore ign = decrypt_ignore()) const;
        api_result decrypt_messages(std::vector<message_api_object>& vec, const login_data& query, decrypt_ignore ign = decrypt_ignore()) const;

        std::vector<message_api_object> get_thread(const message_thread_query&) const;

        settings_api_object get_settings(const std::string& owner) const;

        contact_api_object get_contact_item(const contact_object& o) const;

        contact_api_object get_contact_info(const private_contact_query& query) const;

        contacts_size_api_object get_contacts_size(const std::string& owner) const;

        std::vector<contact_api_object> get_contacts(const contact_query& query) const;

        std::vector<private_group_api_object> get_groups(const private_group_query& query) const;

        private_group_api_object get_group_info(const std::string& group) const;

        std::vector<private_group_member_api_object> get_group_members(const private_group_member_query& query) const;

        bool can_call_callbacks() const;

        void call_callbacks(
            callback_event_type event, const account_name_type& from, const account_name_type& to, fc::variant r);

        ~private_message_plugin_impl() = default;

        bool is_tracked_account(const account_name_type& name) const;

        std::shared_ptr<generic_custom_operation_interpreter<private_message_plugin_operation>> custom_operation_interpreter_;
        flat_map<std::string, std::string> tracked_account_ranges_;
        flat_set<std::string> tracked_account_list_;

        golos::chain::database& _db;

        std::mutex callbacks_mutex_;
        std::list<callback_info> callbacks_;
    };


    void private_message_plugin::private_message_plugin_impl::post_operation(const operation_notification& note) const {
        try {
            note.op.visit(post_operation_visitor(_db));
        } catch (const fc::assert_exception&) {
            if (_db.is_producing()) {
                throw;
            }
        }
    }

    template <typename Direction, typename GetAccount>
    std::vector<message_api_object> private_message_plugin::private_message_plugin_impl::get_message_box(
        const std::string& to, const message_box_query& query, GetAccount&& get_account
    ) const {
        std::vector<message_api_object> result;
        const auto& idx = _db.get_index<message_index, Direction>();
        auto newest_date = query.newest_date;

        if (newest_date == time_point_sec::min()) {
            newest_date = _db.head_block_time();
        }

        auto itr = idx.lower_bound(std::make_tuple(to, newest_date));
        auto etr = idx.upper_bound(std::make_tuple(to, min_create_date()));
        auto offset = query.offset;

        auto filter = [&](const message_object& o) -> bool {
            auto& account = get_account(o);
            return
                (query.select_accounts.empty() || query.select_accounts.count(account)) &&
                (query.filter_accounts.empty() || !query.filter_accounts.count(account)) &&
                (!query.unread_only || o.read_date == time_point_sec::min());
        };

        for (; itr != etr && offset; ++itr) {
            if (filter(*itr)){
                --offset;
            }
        }

        auto limit = query.limit;

        if (!limit) {
            limit = PRIVATE_DEFAULT_LIMIT;
        }

        result.reserve(limit);
        for (; itr != etr && result.size() < limit; ++itr) {
            if (filter(*itr)) {
                result.emplace_back(*itr);
            }
        }

        return result;
    }

    message_account_api_object private_message_plugin::private_message_plugin_impl::get_msg_account(
        account_name_type account, account_name_type current, std::string group) const {
        message_account_api_object res;

        const auto* acc = _db.find_account(account);
        if (!acc) return res;
        res.name = account;

        auto meta = _db.find<account_metadata_object, by_account>(account);
        if (meta != nullptr) {
            res.json_metadata = to_string(meta->json_metadata);
        }

        fc::time_point_sec last_bandwidth_update;
        auto forum = _db.find<account_bandwidth_object, by_account_bandwidth_type>(std::make_tuple(account, bandwidth_type::forum));
        if (forum != nullptr) {
            last_bandwidth_update = forum->last_bandwidth_update;
        }

        res.last_seen = std::max(acc->created, last_bandwidth_update);
        res.memo_key = acc->memo_key;

        if (current != account_name_type()) {
            res.relations = golos::api::current_get_relations(_db, current, account);
        }

        if (!group.size()) return res;
        const auto* pgm = _db.find<private_group_member_object, by_account_group>(std::make_tuple(account, group));
        if (pgm) {
            res.member_type = pgm->member_type;
        } else {
            const auto* pgo = _db.find<private_group_object, by_name>(group);
            if (pgo && pgo->owner == account) {
                res.member_type = private_group_member_type::moder;
            }
        }

        return res;
    }

    bool private_message_plugin::private_message_plugin_impl::fill_msg_account(
        message_accounts& accounts, account_name_type account, account_name_type current, std::string group) const {
        if (account == account_name_type()) {
            return false;
        }
        auto itr = accounts.find(account);
        if (itr == accounts.end()) {
            accounts[account] = get_msg_account(account, current, group);
            return true;
        } else {
            // Merging, but only what really need to merge, due to code flow
            if (current != account_name_type() && !itr->second.relations.valid()) {
                itr->second.relations = get_msg_account(account, current, group).relations;
            }
        }
        return false;
    }

    template<typename OutItr, typename InItr>
    void private_message_plugin::private_message_plugin_impl::get_messages_bidirectional(const message_thread_query& query,
            std::vector<message_api_object>& result, OutItr& out_itr, const OutItr& out_etr,
            InItr& in_itr, const InItr& in_etr,
            std::function<bool (const message_object& msg)> filter) const {
        auto itr_to_message = [&](auto& itr) -> const message_object& {
            const message_object& result = *itr;
            ++itr;
            return result;
        };

        auto select_message = [&]() -> const message_object& {
            if (out_itr != out_etr) {
                if (in_itr == in_etr || out_itr->id > in_itr->id) {
                    return itr_to_message(out_itr);
                }
            }
            return itr_to_message(in_itr);
        };

        auto is_not_done = [&]() -> bool {
            return out_itr != out_etr || in_itr != in_etr;
        };

        auto offset = query.offset;
        while (is_not_done() && offset) {
            auto& message = select_message();
            if (query.is_good(message) && filter(message)) {
                --offset;
            }
        }

        result.reserve(query.limit);
        while (is_not_done() && result.size() < query.limit) {
            auto& message = select_message();
            if (query.is_good(message) && filter(message)) {
                result.emplace_back(message);
            }
        }
    };

    api_result private_message_plugin::private_message_plugin_impl::decrypt_messages(
        std::vector<message_api_object*>& ptrs, const login_data& query, decrypt_ignore ign
    ) const {
        api_result res;

        try { try {
            if (query.account != account_name_type()) {
                using golos::plugins::cryptor::cryptor;
                using golos::plugins::cryptor::cryptor_status;
                using golos::plugins::cryptor::decrypt_messages_query;

                cryptor* cry = appbase::app().find_plugin<cryptor>();
                FC_ASSERT(cry != nullptr, "No Cryptor plugin, when `account` requires it");

                decrypt_messages_query dmq = query;
                std::vector<size_t> idxs;
                for (size_t i = 0; i < ptrs.size(); ++i) {
                    const auto* msg = ptrs[i];
                    FC_ASSERT(msg != nullptr);
                    if (!msg->group.valid()) continue;
                    auto ignore_key = std::to_string(msg->nonce) + "|" + msg->receive_date.to_iso_string() + "|" +
                        msg->from + "|" + msg->to;
                    if (ign.count(ignore_key)) {
                        continue;
                    }

                    dmq.entries.emplace_back(*(msg->group), msg->encrypted_message);
                    idxs.push_back(i);
                    res.decrypt_processed++;
                }

                if (!dmq.entries.size()) return res;

                auto dobj = cry->our_decrypt_messages(dmq);
                if (!!dobj.login_error) {
                    res.login_error = *(dobj.login_error);
                } else if (!!dobj.error) {
                    res.error = *(dobj.error);
                }
                if (dobj.status == cryptor_status::err) {
                    res.status = "err";
                } else {
                    for (size_t i = 0; i < dobj.results.size(); ++i) {
                        auto idx = idxs[i];
                        auto* msg = ptrs[idx];
                        FC_ASSERT(msg != nullptr);

                        const auto& dr = dobj.results[i];
                        if (!!dr.err) {
                            msg->error = *(dr.err);
                        } else if (!!dr.body) {
                            const auto& body = *(dr.body);
                            msg->decrypted = std::vector<char>(body.begin(), body.end());
                            msg->decrypt_date = msg->receive_date;
                        }
                    }
                }
            }
        } FC_CAPTURE_LOG_AND_RETHROW(()) } catch (...) {
            res.status = "err";
            res.error = "internal";
        }
        return res;
    }

    api_result private_message_plugin::private_message_plugin_impl::decrypt_messages(
        std::vector<message_api_object>& vec, const login_data& query, decrypt_ignore ign
    ) const {
        std::vector<message_api_object*> ptrs;
        for (auto& msg : vec) {
            ptrs.push_back(&msg);
        }
        return decrypt_messages(ptrs, query, ign);
    }

    std::vector<message_api_object> private_message_plugin::private_message_plugin_impl::get_thread(
        const message_thread_query& query
    ) const {
        auto from = query.from;
        auto to = query.to;
        auto group = query.group;

        std::vector<message_api_object> result;

        if (group.size()) {
            if (!from.size() && !to.size()) {
                const auto& idx = _db.get_index<message_index, by_group>();
                auto itr = idx.lower_bound(std::make_tuple(group, query.newest_date));

                auto offset = query.offset;
                for (; itr != idx.end() && to_string(itr->group) == group && offset; ++itr) {
                    if (query.is_good(*itr)) {
                        --offset;
                    }
                }

                result.reserve(query.limit);
                for (; itr != idx.end() && to_string(itr->group) == group && result.size() < query.limit; ++itr) {
                    if (query.is_good(*itr)) {
                        result.emplace_back(*itr);
                    }
                }

                return result;
            } else if (from.size()) {
                GOLOS_CHECK_VALUE(false, "If `group` and `from` set, you should set also `to`.");
            } else if (to.size()) {
                GOLOS_CHECK_VALUE(false, "If `group` and `from` set, you should set also `to`.");
            } else {
                const auto& idx = _db.get_index<message_index, by_outbox>();

                auto out_itr = idx.lower_bound(std::make_tuple(from, query.newest_date));
                auto out_etr = idx.upper_bound(std::make_tuple(from, min_create_date()));
                auto in_itr = idx.lower_bound(std::make_tuple(to, query.newest_date));
                auto in_etr = idx.upper_bound(std::make_tuple(to, min_create_date()));

                bool prev_good = true;
                get_messages_bidirectional(query, result, out_itr, out_etr, in_itr, in_etr,
                    [&](const message_object& msg) -> bool {
                        bool strict = msg.from == from ?
                            (msg.to == to) : (msg.to == from);
                        bool good = strict || (prev_good && msg.to == account_name_type());
                        prev_good = strict;
                        return good && to_string(msg.group) == group;
                    });
            }
        } else {
            GOLOS_CHECK_VALUE(from.size() && to.size(), "`from` and `to` are required in query object.");

            const auto& outbox_idx = _db.get_index<message_index, by_outbox_account>();
            const auto& inbox_idx = _db.get_index<message_index, by_inbox_account>();

            group = "";
            auto outbox_itr = outbox_idx.lower_bound(std::make_tuple(group, from, to, query.newest_date));
            auto outbox_etr = outbox_idx.upper_bound(std::make_tuple(group, from, to, min_create_date()));
            auto inbox_itr = inbox_idx.lower_bound(std::make_tuple(group, from, to, query.newest_date));
            auto inbox_etr = inbox_idx.upper_bound(std::make_tuple(group, from, to, min_create_date()));

            get_messages_bidirectional(query, result, outbox_itr, outbox_etr, inbox_itr, inbox_etr,
                [&](const message_object& msg) -> bool {
                    return true;
                });
        }

        return result;
    }

    settings_api_object private_message_plugin::private_message_plugin_impl::get_settings(
        const std::string& owner
    ) const {
        const auto& idx = _db.get_index<settings_index, by_owner>();
        auto itr = idx.find(owner);
        if (itr != idx.end()) {
            return settings_api_object(*itr);
        }

        return settings_api_object();
    }

    contact_api_object private_message_plugin::private_message_plugin_impl::get_contact_item(
        const contact_object& o
    ) const {
        bool is_group = o.kind == contact_kind::group;

        contact_api_object result(o, _db);

        if (!is_group) {
            auto owner = account_name_type(result.contact);
            auto contact = with_dog(o.owner);

            const auto& idx = _db.get_index<contact_index, by_contact>();
            auto itr = idx.find(std::make_tuple(contact, owner));

            if (itr != idx.end()) {
                result.remote_type = itr->type;
            }
        }

        message_thread_query query;
        if (is_group) {
            query.group = result.contact;
        } else {
            query.from = o.owner;
            query.to = account_name_type(result.contact);
        }
        query.limit = 1;
        query.newest_date = _db.head_block_time();
        auto messages = get_thread(query);
        if (messages.size()) {
            result.last_message = std::move(messages[0]);
        }

        return result;
    }

    contact_api_object private_message_plugin::private_message_plugin_impl::get_contact_info(
        const private_contact_query& query
    ) const {
        std::string contact = query.group.size() ?
            query.group : with_dog(query.contact);
        const auto& idx = _db.get_index<contact_index, by_contact>();
        auto itr = idx.find(std::make_tuple(contact, query.owner));

        if (itr != idx.end()) {
            return get_contact_item(*itr);
        }
        return contact_api_object();
    }

    contacts_size_api_object private_message_plugin::private_message_plugin_impl::get_contacts_size(
        const std::string& owner
    ) const {
        contacts_size_api_object result;

        const auto& idx = _db.get_index<contact_size_index, by_owner>();
        auto itr = idx.lower_bound(std::make_tuple(owner, unknown));
        auto etr = idx.upper_bound(std::make_tuple(owner, private_contact_type_size));

        for (; etr != itr; ++itr) {
            result.size[itr->type] = itr->size;
        }

        for (uint8_t i = unknown; i < private_contact_type_size; ++i) {
            auto t = static_cast<private_contact_type>(i);
            if (!result.size.count(t)) {
                result.size[t] = contacts_size_info();
            }
        }

        return result;
    }

    std::vector<contact_api_object> private_message_plugin::private_message_plugin_impl::get_contacts(
        const contact_query& query
    ) const {
        std::vector<contact_api_object> result;

        result.reserve(query.limit);

        const auto& idx = _db.get_index<contact_index, by_owner>();
        auto itr = idx.lower_bound(std::make_tuple(query.owner, query.type));
        auto etr = idx.upper_bound(std::make_tuple(query.owner, query.type));

        auto offset = query.offset;
        for (; itr != etr && offset; ++itr, --offset);

        for (; itr != etr && result.size() < query.limit; ++itr) {
            if (itr->is_hidden) {
                continue;
            }
            if (query.kinds.size() && !query.kinds.count(itr->kind)) {
                continue;
            }
            result.push_back(get_contact_item(*itr));
        }

        std::sort(result.begin(), result.end(), [&](auto& lhs, auto& rhs) {
            return lhs.last_message.receive_date > rhs.last_message.receive_date;
        });

        return result;
    }

    std::vector<private_group_api_object> private_message_plugin::private_message_plugin_impl::get_groups(const private_group_query& query) const {
        std::vector<private_group_api_object> res;

        if (!!query.with_members) {
            const auto& pgms = query.with_members;
            GOLOS_CHECK_LIMIT_PARAM(pgms->accounts.size(), 20);
            GOLOS_CHECK_PARAM(query.with_members, {
                GOLOS_CHECK_VALUE(!pgms->accounts.size() || pgms->start == account_name_type(),
                    "You cannot set with_members.start if you using with_members.accounts");
            });
        }

        if (query.member == account_name_type()) {
            GOLOS_CHECK_PARAM(query.member_types, {
                GOLOS_CHECK_VALUE(query.member_types.size() == 0, "If member is empty (select all groups) - member_types should be [] empty too.");
            });

            if (query.sort == private_group_sort::by_popularity) {
                const auto& idx = _db.get_index<private_group_index, by_popularity>();
                auto itr = idx.begin();
                bool reached = !query.start_group.size();
                for (; itr != idx.end() && res.size() < query.limit; ++itr) {
                    if (!reached && to_string(itr->name) != query.start_group) continue;
                    reached = true;
                    res.emplace_back(*itr, _db, query.with_members);
                }
            } else {
                const auto& idx = _db.get_index<private_group_index,  by_name>();
                auto itr = idx.lower_bound(query.start_group);
                for (; itr != idx.end() && res.size() < query.limit; ++itr) {
                    res.emplace_back(*itr, _db, query.with_members);
                }
            }

            return res;
        }

        if (query.member_types.size()) {
            const auto& idx = _db.get_index<private_group_member_index, by_account_group>();
            auto itr = idx.lower_bound(query.member);
            bool reached = !query.start_group.size();
            for (; itr != idx.end() && itr->account == query.member
                    && res.size() < query.limit; ++itr) {
                bool has = query.member_types.count(itr->member_type);
                if (!has) continue;

                if (!reached && to_string(itr->group) != query.start_group) {
                    continue;
                }
                reached = true;

                const auto& pgo = _db.get<private_group_object, by_name>(itr->group);
                res.emplace_back(pgo, _db, query.with_members);
            }

            return res;
        }

        const auto& idx = _db.get_index<private_group_index, by_owner>();
        auto itr = idx.lower_bound(query.member);
        bool reached = !query.start_group.size();
        for (; itr != idx.end() && itr->owner == query.member
                && res.size() < query.limit; ++itr) {
            if (!reached && to_string(itr->name) != query.start_group) {
                continue;
            }
            reached = true;

            res.emplace_back(*itr, _db, query.with_members);
        }

        return res;
    }

    private_group_api_object private_message_plugin::private_message_plugin_impl::get_group_info(const std::string& group) const {
        return _db.with_weak_read_lock([&](){
            const auto* pgo = _db.find<private_group_object, by_name>(group);
            if (!pgo) {
                return private_group_api_object();
            }
            return private_group_api_object();//*group, _db, fc::optional<private_group_members>());
        });
    }

    std::vector<private_group_member_api_object> private_message_plugin::private_message_plugin_impl::get_group_members(const private_group_member_query& query) const {
        std::vector<private_group_member_api_object> res;

        bool reached_start = query.start_member == account_name_type();

        auto query_types = query.member_types;
        if (!query_types.size()) {
            for (uint8_t i = 0; i < uint8_t(private_group_member_type::_size); ++i) {
                query_types.insert(private_group_member_type(i));
            }
        }

        std::vector<private_group_member_sort_condition> sort_ups;
        std::vector<private_group_member_sort_condition> sort_downs;
        for (const auto& cond : query.sort_conditions) {
            query_types.erase(cond.member_type);
            if (cond.direction == private_group_member_sort_direction::up) {
                sort_ups.push_back(cond);
            } else {
                sort_downs.push_back(cond);
            }
        }

        auto fill_member_types = [&](const std::set<private_group_member_type>& member_types) {
            return _db.with_weak_read_lock([&](){
                const auto& idx = _db.get_index<private_group_member_index, by_group_updated>();
                auto itr = idx.lower_bound(query.group);
                for (; itr != idx.end() && to_string(itr->group) == query.group; ++itr) {
                    if (res.size() == query.limit) return true;

                    if (member_types.size() && !member_types.count(itr->member_type)) {
                        continue;
                    }

                    if (!reached_start && itr->account != query.start_member) {
                        continue;
                    }
                    reached_start = true;

                    res.emplace_back(*itr);
                    if (query.accounts) {
                        res.back().account_data = get_msg_account(itr->account, account_name_type(), query.group);
                    }
                }
                return false;
            });
        };

        for (const auto& cond : sort_ups) {
            if (fill_member_types({cond.member_type})) {
                return res;
            }
        }

        fill_member_types(query_types);

        for (const auto& cond : sort_downs) {
            if (fill_member_types({cond.member_type})) {
                return res;
            }
        }

        return res;
    }

    bool private_message_plugin::private_message_plugin_impl::can_call_callbacks() const {
        return !_db.is_producing() && !_db.is_generating() && !callbacks_.empty();
    }

    void private_message_plugin::private_message_plugin_impl::call_callbacks(
        callback_event_type event, const account_name_type& from, const account_name_type& to, fc::variant r
    ) {
        std::lock_guard<std::mutex> lock(callbacks_mutex_);
        for (auto itr = callbacks_.begin(); itr != callbacks_.end(); ) {
            auto& info = *itr;

            if (info.query.filter_events.count(event) ||
                (!info.query.select_events.empty() && !info.query.select_events.count(event)) ||
                info.query.filter_accounts.count(from) ||
                info.query.filter_accounts.count(to) ||
                (!info.query.select_accounts.empty() &&
                 !info.query.select_accounts.count(to) &&
                 !info.query.select_accounts.count(from))
            ) {
                ++itr;
                continue;
            }

            try {
                info.msg->unsafe_result(r);
                ++itr;
            } catch (...) {
                callbacks_.erase(itr++);
            }
        }
    }

    private_message_plugin::private_message_plugin() = default;

    private_message_plugin::~private_message_plugin() = default;

    const std::string& private_message_plugin::name() {
        static std::string name = "private_message";
        return name;
    }

    bool private_message_plugin::is_tracked_account(const account_name_type& name) { 
        return my->is_tracked_account(name);
    }

    bool private_message_plugin::can_call_callbacks()  {
        return my->can_call_callbacks();
    }

    void private_message_plugin::call_callbacks(
        callback_event_type event, const account_name_type& from, const account_name_type& to, fc::variant r
    ) {
        return my->call_callbacks(event, from, to, r);
    }

    void private_message_plugin::set_program_options(
        boost::program_options::options_description& cli,
        boost::program_options::options_description& cfg) {
        cfg.add_options() ("pm-account-range",
            boost::program_options::value<std::vector<std::string>>()->composing()->multitoken(),
            "Defines a range of accounts to private messages to/from as a json pair [\"from\",\"to\"] [from,to]"
        ) (
            "pm-account-list",
            boost::program_options::value<std::vector<std::string>>()->composing()->multitoken(),
            "Defines a list of accounts to private messages to/from"
        );
    }

    void private_message_plugin::plugin_initialize(const boost::program_options::variables_map &options) {
        ilog("Intializing private message plugin");
        my = std::make_unique<private_message_plugin::private_message_plugin_impl>(*this);

        my->_db.post_apply_operation.connect([&](const operation_notification& note) {
            my->post_operation(note);
        });

        add_plugin_index<message_index>(my->_db);
        add_plugin_index<settings_index>(my->_db);
        add_plugin_index<contact_index>(my->_db);
        add_plugin_index<contact_size_index>(my->_db);
        add_plugin_index<private_group_index>(my->_db);
        add_plugin_index<private_group_member_index>(my->_db);

        using pairstring = std::pair<std::string, std::string>;
        LOAD_VALUE_SET(options, "pm-account-range", my->tracked_account_ranges_, pairstring);
        if (options.count("pm-account-list")) {
            auto list = options["pm-account-list"].as<std::vector<std::string>>();
            my->tracked_account_list_.insert(list.begin(), list.end());
        }
        JSON_RPC_REGISTER_API(name())
    }

    void private_message_plugin::plugin_startup() {
        ilog("Starting up private message plugin");
    }

    void private_message_plugin::plugin_shutdown() {
        ilog("Shuting down private message plugin");
    }

    bool private_message_plugin::private_message_plugin_impl::is_tracked_account(const account_name_type& name) const {
        if (tracked_account_ranges_.empty() && tracked_account_list_.empty()) {
            return true;
        }

        auto list_itr = tracked_account_list_.find(name);
        if (list_itr != tracked_account_list_.end()) {
            return true;
        }

        auto range_itr = tracked_account_ranges_.lower_bound(name);
        return range_itr != tracked_account_ranges_.end() && name >= range_itr->first && name <= range_itr->second;
    }

    // Api Defines

    DEFINE_API(private_message_plugin, get_inbox) {
        PLUGIN_API_VALIDATE_ARGS(
            (std::string, to)
            (message_box_query, query)
        );

        GOLOS_CHECK_LIMIT_PARAM(query.limit, PRIVATE_DEFAULT_LIMIT);

        GOLOS_CHECK_PARAM(query.filter_accounts, {
            for (auto& acc : query.filter_accounts) {
                GOLOS_CHECK_VALUE(!query.select_accounts.count(acc),
                    "Can't filter and select account '${account}' at the same time",
                    ("account", acc));
            }
        });

        return my->_db.with_weak_read_lock([&]() {
            return my->get_message_box<by_inbox>(
                to, query,
                [&](const message_object& o) -> const account_name_type& {
                    return o.from;
                }
            );
        });
    }

    DEFINE_API(private_message_plugin, get_outbox) {
        PLUGIN_API_VALIDATE_ARGS(
            (std::string, from)
            (message_box_query, query)
        );

        GOLOS_CHECK_LIMIT_PARAM(query.limit, PRIVATE_DEFAULT_LIMIT);

        GOLOS_CHECK_PARAM(query.filter_accounts, {
            for (auto& acc : query.filter_accounts) {
                GOLOS_CHECK_VALUE(!query.select_accounts.count(acc),
                    "Can't filter and select account '${account}' at the same time",
                    ("account", acc));
            }
        });

        return my->_db.with_weak_read_lock([&]() {
            return my->get_message_box<by_outbox>(
                from, query,
                [&](const message_object& o) -> const account_name_type& {
                    return o.to;
                });
        });
    }

    DEFINE_API(private_message_plugin, get_thread) {
        using query_type = message_thread_query;
        PLUGIN_API_VALIDATE_ARGS(
            (fc::variant, from_or_query)
            (std::string, to, "")
            (fc::optional<query_type>, opts, fc::optional<query_type>())
        );

        bool old_version = false;
        query_type query;
        if (!!opts) {
            query = *opts;
        }

        if (from_or_query.is_string()) {
            old_version = true;
            GOLOS_CHECK_PARAM(query, {
                GOLOS_CHECK_VALUE(!query.group.size(), "Old version of API method does not support groups.");
                GOLOS_CHECK_VALUE(!query.from.size(), "You duplicating `from`, remove it from query object.");
                GOLOS_CHECK_VALUE(!query.to.size(), "You duplicating `to`, remove it from query object.");
            });

            query.from = from_or_query.as_string();
            query.to = to;
        } else {
            //GOLOS_CHECK_VALUE(query == query_type(), "You should pass only one argument (query object).");
            GOLOS_CHECK_VALUE(!to.size(), "You should pass only one argument (query object).");

            query = from_or_query.as<query_type>();
        }

        if (query.account != account_name_type()) {
            GOLOS_CHECK_VALUE(query.group.size(), "Authorization (`account`) is only for groups.");
        }

        if (!query.limit) {
            query.limit = PRIVATE_DEFAULT_LIMIT;
        }
        GOLOS_CHECK_LIMIT_PARAM(query.limit, PRIVATE_DEFAULT_LIMIT * 10);

        if (query.newest_date == time_point_sec::min()) {
            query.newest_date = my->_db.head_block_time();
        }

        message_accounts accounts;

        auto vec = my->_db.with_weak_read_lock([&]() {
            auto msgs = my->get_thread(query);

            if (query.accounts) {
                account_name_type current;
                if (!query.group.size()) current = query.from;

                for (const auto& msg : msgs) {
                    my->fill_msg_account(accounts, msg.from,
                        msg.from != current ? current : account_name_type(),
                        query.group);
                    my->fill_msg_account(accounts, msg.to,
                        msg.to != current ? current : account_name_type(),
                        query.group);
                }
                my->fill_msg_account(accounts, query.from, account_name_type(), query.group);
                my->fill_msg_account(accounts, query.to, current, query.group);
            }

            return msgs;
        });

        auto dec_res = my->decrypt_messages(vec, query, query.cache);

        fc::variant var;
        if (!old_version) {
            fc::mutable_variant_object vo;
            vo["status"] = dec_res.status;
            if (dec_res.login_error.size()) vo["login_error"] = dec_res.login_error;
            if (dec_res.error.size()) vo["error"] = dec_res.error;
            vo["messages"] = vec;
            vo["_dec_processed"] = dec_res.decrypt_processed;

            if (query.contacts.valid()) {
                auto cons = my->_db.with_weak_read_lock([&]() {
                    auto contacts = my->get_contacts(*(query.contacts));

                    if (query.accounts) {
                        account_name_type current;
                        if (query.contacts->relations) {
                            current = query.contacts->owner;
                        }

                        my->fill_msg_account(accounts, query.contacts->owner);
                        for (const auto& con : contacts) {
                            if (con.kind != contact_kind::account) continue;
                            my->fill_msg_account(accounts, con.contact, current);
                        }
                    }

                    return contacts;
                });

                std::vector<message_api_object*> lmsgs;
                for (auto& con : cons) {
                    if (con.last_message.from != account_name_type()) {
                        lmsgs.push_back(&con.last_message);
                    }
                }
                auto con_res = my->decrypt_messages(lmsgs, query, query.contacts->cache);

                vo["contacts"] = cons;
                vo["_dec_processed_con"] = con_res.decrypt_processed;
            }

            if (query.accounts) {
                vo["accounts"] = to_variant(accounts);
            }

            var = vo;
        } else {
            fc::to_variant(vec, var);
        }
        return var;
    }

    DEFINE_API(private_message_plugin, get_settings) {
        PLUGIN_API_VALIDATE_ARGS(
            (std::string, owner)
        );

        return my->_db.with_weak_read_lock([&](){
            return my->get_settings(owner);
        });
    }

    DEFINE_API(private_message_plugin, get_contacts_size) {
        PLUGIN_API_VALIDATE_ARGS(
            (std::string, owner)
        );

        return my->_db.with_weak_read_lock([&](){
            return my->get_contacts_size(owner);
        });
    }

    DEFINE_API(private_message_plugin, get_contact_info) {
        PLUGIN_API_VALIDATE_ARGS(
            (fc::variant, owner_or_query)
            (std::string, contact, "")
        );

        private_contact_query query;
        bool old_version = false;
        if (owner_or_query.is_string()) {
            old_version = true;
            // Only account-contacts. Not groups
            query.owner = owner_or_query.as_string();
            query.contact = contact;
        } else {
            query = owner_or_query.as<private_contact_query>();
            if (query.group.size()) {
                GOLOS_CHECK_VALUE(!query.contact.size(), "`contact` is for account contact, `group` is for group, you cannot use them both.");
            }
        }

        auto obj = my->_db.with_weak_read_lock([&](){
            return my->get_contact_info(query);
        });

        fc::variant var;
        if (!old_version) {
            std::vector<message_api_object*> msgs;
            if (obj.last_message.from != account_name_type()) {
                msgs.push_back(&obj.last_message);
            }
            auto dec_res = my->decrypt_messages(msgs, query);

            fc::mutable_variant_object vo;
            vo["status"] = dec_res.status;
            if (dec_res.login_error.size()) vo["login_error"] = dec_res.login_error;
            if (dec_res.error.size()) vo["error"] = dec_res.error;
            vo["contact"] = obj;
            var = vo;
        } else {
            fc::to_variant(obj, var);
        }
        return var;
    }

    DEFINE_API(private_message_plugin, get_contacts) {
        PLUGIN_API_VALIDATE_ARGS(
            (fc::variant, owner_or_query)
            (private_contact_type, type, private_contact_type::unknown)
            (uint16_t, limit, 20)
            (uint32_t, offset, 0)
        );

        contact_query query;
        bool old_version = false;
        if (owner_or_query.is_string()) {
            old_version = true;
            query.owner = owner_or_query.as_string();
            query.kinds = {contact_kind::account}; // Not groups. Compatibility + no decrypting need
            query.type = type;
            query.limit = limit;
            query.offset = offset;
        } else {
            query = owner_or_query.as<contact_query>();
        }

        GOLOS_CHECK_LIMIT_PARAM(query.limit, 100);

        message_accounts accounts;

        auto vec = my->_db.with_weak_read_lock([&](){
            auto contacts = my->get_contacts(query);
            if (query.accounts) {
                account_name_type current;
                if (query.relations) {
                    current = query.owner;
                }

                my->fill_msg_account(accounts, query.owner);
                for (const auto& con : contacts) {
                    if (con.kind != contact_kind::account) continue;
                    my->fill_msg_account(accounts, con.contact, current);
                }
            }
            return contacts;
        });

        fc::variant var;
        if (!old_version) {
            std::vector<message_api_object*> msgs;
            for (auto& con : vec) {
                if (con.last_message.from != account_name_type()) {
                    msgs.push_back(&con.last_message);
                }
            }
            auto dec_res = my->decrypt_messages(msgs, query, query.cache);

            fc::mutable_variant_object vo;
            vo["status"] = dec_res.status;
            if (dec_res.login_error.size()) vo["login_error"] = dec_res.login_error;
            if (dec_res.error.size()) vo["error"] = dec_res.error;
            vo["contacts"] = vec;

            if (query.accounts) {
                vo["accounts"] = to_variant(accounts);
            }

            vo["_dec_processed"] = dec_res.decrypt_processed;
            var = vo;
        } else {
            fc::to_variant(vec, var);
        }
        return var;
    }

    DEFINE_API(private_message_plugin, set_callback) {
        PLUGIN_API_VALIDATE_ARGS(
            (callback_query, query)
        );

        GOLOS_CHECK_PARAM(query.filter_accounts, {
            for (auto& acc : query.filter_accounts) {
                GOLOS_CHECK_VALUE(!query.select_accounts.count(acc),
                    "Can't filter and select account '${account}' at the same time",
                    ("account", acc));
            }
        });

        GOLOS_CHECK_PARAM(query.filter_events, {
            for (auto& event : query.filter_events) {
                GOLOS_CHECK_VALUE(!query.select_events.count(event),
                    "Can't filter and select event '${event}' at the same time",
                    ("event", event));
            }
        });

        json_rpc::msg_pack_transfer transfer(args);
        {
            std::lock_guard<std::mutex> lock(my->callbacks_mutex_);
            my->callbacks_.emplace_back(std::move(query), transfer.msg());
        };
        transfer.complete();
        return {};
    }

    DEFINE_API(private_message_plugin, get_groups) {
        PLUGIN_API_VALIDATE_ARGS(
            (private_group_query, query)
        );

        GOLOS_CHECK_LIMIT_PARAM(query.limit, 100);

        return my->_db.with_weak_read_lock([&](){
            const auto& groups = my->get_groups(query);

            fc::mutable_variant_object vo;
            vo["groups"] = groups;
            return vo;
        });
    }

    DEFINE_API(private_message_plugin, get_group_members) {
        PLUGIN_API_VALIDATE_ARGS(
            (private_group_member_query, query)
        );

        GOLOS_CHECK_LIMIT_PARAM(query.limit, 100);

        const auto& members = my->get_group_members(query);

        fc::mutable_variant_object vo;
        vo["members"] = members;
        if (query.group_data) {
            vo["group"] = my->get_group_info(query.group);
        }
        return vo;
    }

} } } // golos::plugins::private_message
