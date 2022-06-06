#include <golos/plugins/private_message/private_message_plugin.hpp>
#include <golos/plugins/private_message/private_message_evaluators.hpp>
#include <golos/plugins/private_message/private_message_objects.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/plugins/chain/plugin.hpp>
#include <appbase/application.hpp>
#include <golos/protocol/donate_targets.hpp>

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
                auto id_itr = id_idx.find(std::make_tuple(md->from, md->to, md->nonce));
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

            _db.set_custom_operation_interpreter(plugin.name(), custom_operation_interpreter_);
        }

        void post_operation(const operation_notification& note) const;

        template <typename Direction, typename Filter>
        std::vector<message_api_object> get_message_box(
            const std::string& account, const message_box_query&, Filter&&) const;

        std::vector<message_api_object> get_thread(
            const std::string& from, const std::string& to, const message_thread_query&) const;

        settings_api_object get_settings(const std::string& owner) const;

        contact_api_object get_contact_item(const contact_object& o) const;

        contact_api_object get_contact_info(const std::string& owner, const std::string& contact) const;

        contacts_size_api_object get_contacts_size(const std::string& owner) const;

        std::vector<contact_api_object> get_contacts(
            const std::string& owner, const private_contact_type, uint16_t limit, uint32_t offset) const;

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

     std::vector<message_api_object> private_message_plugin::private_message_plugin_impl::get_thread(
        const std::string& from, const std::string& to, const message_thread_query& query
    ) const {

        std::vector<message_api_object> result;
        const auto& outbox_idx = _db.get_index<message_index, by_outbox_account>();
        const auto& inbox_idx = _db.get_index<message_index, by_inbox_account>();

        auto outbox_itr = outbox_idx.lower_bound(std::make_tuple(from, to, query.newest_date));
        auto outbox_etr = outbox_idx.upper_bound(std::make_tuple(from, to, min_create_date()));
        auto inbox_itr = inbox_idx.lower_bound(std::make_tuple(from, to, query.newest_date));
        auto inbox_etr = inbox_idx.upper_bound(std::make_tuple(from, to, min_create_date()));
        auto offset = query.offset;

        auto filter = [&](const message_object& o) {
            return (!query.unread_only || o.read_date == time_point_sec::min());
        };

        auto itr_to_message = [&](auto& itr) -> const message_object& {
            const message_object& result = *itr;
            ++itr;
            return result;
        };

        auto select_message = [&]() -> const message_object& {
            if (outbox_itr != outbox_etr) {
                if (inbox_itr == inbox_etr || outbox_itr->id > inbox_itr->id) {
                    return itr_to_message(outbox_itr);
                }
            }
            return itr_to_message(inbox_itr);
        };

        auto is_not_done = [&]() -> bool {
            return outbox_itr != outbox_etr || inbox_itr != inbox_etr;
        };

        while (is_not_done() && offset) {
            auto& message = select_message();
            if (filter(message)){
                --offset;
            }
        }

        result.reserve(query.limit);
        while (is_not_done() && result.size() < query.limit) {
            auto& message = select_message();
            if (filter(message)) {
                result.emplace_back(message);
            }
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
        contact_api_object result(o);

        const auto& idx = _db.get_index<contact_index, by_contact>();
        auto itr = idx.find(std::make_tuple(o.contact, o.owner));

        if (itr != idx.end()) {
            result.remote_type = itr->type;
        }

        message_thread_query query;
        query.limit = 1;
        query.newest_date = _db.head_block_time();
        auto messages = get_thread(o.owner, o.contact, query);
        if (messages.size()) {
            result.last_message = std::move(messages[0]);
        }

        return result;
    }

    contact_api_object private_message_plugin::private_message_plugin_impl::get_contact_info(
        const std::string& owner, const std::string& contact
    ) const {
        const auto& idx = _db.get_index<contact_index, by_contact>();
        auto itr = idx.find(std::make_tuple(owner, contact));

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
        const std::string& owner, const private_contact_type type, uint16_t limit, uint32_t offset
    ) const {
        std::vector<contact_api_object> result;

        result.reserve(limit);

        const auto& idx = _db.get_index<contact_index, by_owner>();
        auto itr = idx.lower_bound(std::make_tuple(owner, type));
        auto etr = idx.upper_bound(std::make_tuple(owner, type));

        for (; itr != etr && offset; ++itr, --offset);

        for (; itr != etr; ++itr) {
            if (!itr->is_hidden)
                result.push_back(get_contact_item(*itr));
        }

        std::sort(result.begin(), result.end(), [&](auto& lhs, auto& rhs) {
            return lhs.last_message.receive_date > rhs.last_message.receive_date;
        });

        return result;
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
        PLUGIN_API_VALIDATE_ARGS(
            (std::string, from)
            (std::string, to)
            (message_thread_query, query)
        );

        GOLOS_CHECK_LIMIT_PARAM(query.limit, PRIVATE_DEFAULT_LIMIT * 10);

        if (!query.limit) {
            query.limit = PRIVATE_DEFAULT_LIMIT;
        }

        if (query.newest_date == time_point_sec::min()) {
            query.newest_date = my->_db.head_block_time();
        }

        return my->_db.with_weak_read_lock([&]() {
            return my->get_thread(from, to, query);
        });
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
            (std::string, owner)
            (std::string, contact)
        );

        return my->_db.with_weak_read_lock([&](){
            return my->get_contact_info(owner, contact);
        });
    }

    DEFINE_API(private_message_plugin, get_contacts) {
        PLUGIN_API_VALIDATE_ARGS(
            (std::string, owner)
            (private_contact_type, type)
            (uint16_t, limit)
            (uint32_t, offset)
        );

        GOLOS_CHECK_LIMIT_PARAM(limit, 100);

        return my->_db.with_weak_read_lock([&](){
            return my->get_contacts(owner, type, limit, offset);
        });
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

} } } // golos::plugins::private_message
