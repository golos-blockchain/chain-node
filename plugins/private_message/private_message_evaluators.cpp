#include <golos/plugins/private_message/private_message_operations.hpp>
#include <golos/plugins/private_message/private_message_objects.hpp>
#include <golos/plugins/private_message/private_message_exceptions.hpp>
#include <golos/plugins/private_message/private_message_evaluators.hpp>

uint32_t MAX_GROUPS_LIMIT = 100; // per creator
uint32_t MAX_MODERS_LIMIT = 50; // per group

namespace golos { namespace plugins { namespace private_message {

void check_golos_power(const account_object& acc, const database& _db) {
    auto min_gp = _db.get_min_gp_for_groups();
    GOLOS_CHECK_LOGIC(acc.vesting_shares >= min_gp.first,
        logic_errors::too_low_gp,
        "Too low golos power: ${r} is required, ${p} is present.",
        ("r", min_gp.second)("p", acc.vesting_shares));
}

struct private_message_extension_visitor {
    private_message_extension_visitor(const account_name_type& _requester,
        const database& db, std::string& _group, std::set<account_name_type>& _mentions,
        account_name_type _delete_from = account_name_type())
        : requester(_requester), _db(db), group(_group), mentions(_mentions), delete_from(_delete_from) {
    }

    const account_name_type& requester;
    const database& _db;
    std::string& group;
    std::set<account_name_type>& mentions;
    account_name_type delete_from;

    using result_type = void;

    void operator()(const private_group_options& _pgo) const {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_30__236, "private_group_options");

        const auto& pgo = _db.get<private_group_object, by_name>(_pgo.group);
        group = _pgo.group;
        mentions = _pgo.mentions;
        auto req = _pgo.requester.size() ? _pgo.requester : requester;

        const auto& pgm_idx = _db.get_index<private_group_member_index, by_account_group>();

        auto pgm_itr = pgm_idx.find(std::make_tuple(req, pgo.name));
        bool exists = pgm_itr != pgm_idx.end();

        if (exists) {
            GOLOS_CHECK_LOGIC(pgm_itr->member_type != private_group_member_type::banned,
                logic_errors::unauthorized, "You are banned.");
        }

        bool is_owner = pgo.owner == req;
        bool is_moder = is_owner || (exists && pgm_itr->member_type == private_group_member_type::moder); 

        if (pgo.privacy != private_group_privacy::public_group) {
            GOLOS_CHECK_LOGIC((exists && pgm_itr->member_type == private_group_member_type::member) || is_moder,
                logic_errors::unauthorized, "You should be member of the group.");
        }

        // Update by moderator
        if (_pgo.requester != account_name_type()) {
            GOLOS_CHECK_LOGIC(is_moder,
                logic_errors::unauthorized, "You should be moder to update foreign messages.");
        }

        if (delete_from != account_name_type() && requester != delete_from) {
            GOLOS_CHECK_LOGIC(is_moder,
                logic_errors::unauthorized, "You should be moder to delete foreign messages.");
        }
    }
};

void private_message_evaluator::do_apply(const private_message_operation& op) {
    if (!_plugin->is_tracked_account(op.from) && !_plugin->is_tracked_account(op.to)) {
        return;
    }

    auto& contact_idx = _db.get_index<contact_index, by_contact>();

    if (op.to.size()) {
        _db.get_account(op.to);

        auto contact_itr = contact_idx.find(std::make_tuple(op.from, op.to));

        auto& cfg_idx = _db.get_index<settings_index, by_owner>();
        auto cfg_itr = cfg_idx.find(op.to);

        GOLOS_CHECK_LOGIC(contact_itr == contact_idx.end() || contact_itr->type != ignored,
            logic_errors::sender_in_ignore_list,
            "Sender is in the ignore list of recipient");

        _db.check_no_blocking(op.to, op.from, false);

        GOLOS_CHECK_LOGIC(
            (cfg_itr == cfg_idx.end() || !cfg_itr->ignore_messages_from_unknown_contact) ||
            (contact_itr != contact_idx.end() && contact_itr->type == pinned),
            logic_errors::recipient_ignores_messages_from_unknown_contact,
            "Recipient accepts messages only from his contact list");
    }

    std::string group;
    std::set<account_name_type> mentions;
    private_message_extension_visitor visitor(op.from, _db, group, mentions);
    for (auto& e : op.extensions) {
        e.visit(visitor);
    }
    bool is_group = group.size();
    for (const auto& acc : mentions) {
        _db.get_account(acc);
    }

    auto& id_idx = _db.get_index<message_index, by_nonce>();
    auto id_itr = id_idx.find(std::make_tuple(group, op.from, op.to, op.nonce));

    if (op.update && id_itr == id_idx.end()) {
        GOLOS_THROW_MISSING_OBJECT("private_message",
            fc::mutable_variant_object()("from", op.from)("to", op.to)("nonce", op.nonce));
    } else if (!op.update && id_itr != id_idx.end()){
        GOLOS_THROW_OBJECT_ALREADY_EXIST("private_message",
            fc::mutable_variant_object()("from", op.from)("to", op.to)("nonce", op.nonce));
    }

    auto now = _db.head_block_time();

    for (const auto& men : mentions) {
        _db.check_no_blocking(men, op.from, false);
    }

    auto set_message = [&](message_object& pmo) {
        pmo.from_memo_key = op.from_memo_key;
        pmo.to_memo_key = op.to_memo_key;
        pmo.checksum = op.checksum;
        pmo.receive_date = now;
        pmo.encrypted_message.resize(op.encrypted_message.size());
        std::copy(
            op.encrypted_message.begin(), op.encrypted_message.end(),
            pmo.encrypted_message.begin());
        pmo.mentions.clear();
        for (const auto& men : mentions) {
            pmo.mentions.push_back(men);
        }
    };

    if (id_itr == id_idx.end()) {
        if (is_group) {
            const auto& from = _db.get_account(op.from);
            check_golos_power(from, _db);
        
            const auto& pgo = _db.get<private_group_object, by_name>(group);
            _db.modify(pgo, [&](auto& pgo) {
                ++pgo.total_messages;
            });
        }

        _db.create<message_object>([&](auto& pmo) {
            from_string(pmo.group, group);
            pmo.from = op.from;
            pmo.to = op.to;
            pmo.nonce = op.nonce;
            pmo.inbox_create_date = now;
            pmo.outbox_create_date = now;
            pmo.read_date = (is_group && op.to == account_name_type()) ?
                now : time_point_sec::min();
            pmo.remove_date = time_point_sec::min();
            set_message(pmo);
        });
        id_itr = id_idx.find(std::make_tuple(group, op.from, op.to, op.nonce));
    } else {
        _db.modify(*id_itr, set_message);
    }

    if (_plugin->can_call_callbacks()) {
        _plugin->call_callbacks(
            callback_event_type::message, op.from, op.to,
            fc::variant(callback_message_event({callback_event_type::message, message_api_object(*id_itr)})));
    }

    // Ok, now update contact lists and counters in them
    auto& size_idx = _db.get_index<contact_size_index, by_owner>();

    // Increment counters depends on side of communication
    auto inc_counters = [&](auto& size_object, bool is_send, bool is_mention = false) {
        if (is_send) {
            size_object.total_outbox_messages++;
            if (!is_group || op.to != account_name_type()) {
                size_object.unread_outbox_messages++;
            }
        } else {
            if (is_mention) {
                size_object.unread_mentions++;
            } else {
                size_object.total_inbox_messages++;
                size_object.unread_inbox_messages++;
            }
        }
    };

    // Update global counters by type of contact
    auto modify_size = [&](auto& owner, auto type, bool is_new_contact, bool is_send, bool is_mention) {
        auto modify_counters = [&](auto& pcso) {
            inc_counters(pcso.size, is_send, is_mention);
            if (is_new_contact) {
                pcso.size.total_contacts++;
            }
        };

        auto size_itr = size_idx.find(std::make_tuple(owner, type));
        if (size_itr == size_idx.end()) {
            _db.create<contact_size_object>([&](auto& pcso){
                pcso.owner = owner;
                pcso.type = type;
                modify_counters(pcso);
            });
        } else {
            _db.modify(*size_itr, modify_counters);
        }
    };

    // Add contact list if it doesn't exist or update it if it exits
    auto modify_contact = [&](auto& owner, const std::string& contact, auto kind, auto type, bool is_send, bool is_mention = false) {
        bool is_new_contact;

        auto contact_at = (kind == contact_kind::account) ?
            with_dog(contact) : contact;

        auto contact_itr = contact_idx.find(std::make_tuple(contact_at, owner));
        if (contact_itr != contact_idx.end()) {
            _db.modify(*contact_itr, [&](auto& pco) {
                pco.is_hidden = false;
                inc_counters(pco.size, is_send, is_mention);
            });
            is_new_contact = false;
            type = contact_itr->type;
        } else {
            _db.create<contact_object>([&](auto& pco) {
                pco.owner = owner;
                from_string(pco.contact, contact_at);
                pco.kind = kind;
                pco.type = type;
                inc_counters(pco.size, is_send, is_mention);
            });
            is_new_contact = true;

            if (_plugin->can_call_callbacks()) {
                contact_itr = contact_idx.find(std::make_tuple(contact_at, owner));
                _plugin->call_callbacks(
                    callback_event_type::contact, owner, contact,
                    fc::variant(callback_contact_event(
                        {callback_event_type::contact, contact_api_object(*contact_itr, _db)})));
            }
        }
        modify_size(owner, type, is_new_contact, is_send, is_mention);
    };

    if (!op.update) {
        if (!is_group) {
            modify_contact(op.from, op.to, contact_kind::account, unknown, true);
            modify_contact(op.to, op.from, contact_kind::account, unknown, false);
        } else {
            modify_contact(op.from, group, contact_kind::group, unknown, true);
            if (op.to != account_name_type()) {
                modify_contact(op.to, group, contact_kind::group, unknown, false);
            }
            for (const auto& men : mentions) {
                if (op.to != men) {
                    modify_contact(men, group, contact_kind::group, unknown, false, true);
                }
            }
        }
    }
}

template <typename Direction, typename Operation, typename Action, typename... Args>
bool process_private_messages(database& _db, const Operation& po, Action&& action, Args&&... args) {
    auto start_date = std::max(po.start_date, min_create_date());
    auto stop_date = std::max(po.stop_date, min_create_date());

    auto& idx = _db.get_index<message_index, Direction>();
    auto itr = idx.lower_bound(std::make_tuple(std::forward<Args>(args)..., stop_date));
    auto etr = idx.lower_bound(std::make_tuple(std::forward<Args>(args)..., start_date));

    if (itr == etr) {
        return false;
    }

    while (itr != etr) {
        auto& message = (*itr);
        ++itr;
        if (!action(message)) {
            break;
        }
    }
    return true;
}

template <typename Operation, typename Map, typename ProcessAction, typename NoMsgsAction, typename ContactAction>
void process_group_message_operation(
    database& _db, const Operation& po, const std::string& group, const std::string& requester,
    Map& map, ProcessAction&& process_action, NoMsgsAction&& no_msgs_action, ContactAction&& contact_action,
    bool delete_contact = false
) {
    if (po.nonce != 0) {
        auto& idx = _db.get_index<message_index, by_nonce>();
        auto itr = idx.find(std::make_tuple(group, po.from, po.to, po.nonce));

        if (itr == idx.end()) {
            if (!no_msgs_action()) {
                GOLOS_THROW_MISSING_OBJECT("private_message",
                    fc::mutable_variant_object()("group", group)("from", po.from)("to", po.to)("nonce", po.nonce));
            }
        } else {
            process_action(*itr);
        }
    } else if (group.size()) {
        if (!process_private_messages<by_group>(_db, po, process_action, group)) {
            if (!no_msgs_action()) {
                GOLOS_THROW_MISSING_OBJECT("private_message",
                    fc::mutable_variant_object()("group", group)
                    ("start_date", po.start_date)("stop_date", po.stop_date));
            }
        }
    } else if (po.from.size() && po.to.size() && po.from == requester) {
        bool no_msgs = !process_private_messages<by_outbox_account>(_db, po, process_action, group, po.from, po.to);
        if (delete_contact)
            no_msgs &= !process_private_messages<by_inbox_account>(_db, po, process_action, group, po.from, po.to);
        if (no_msgs) {
            if (!no_msgs_action()) {
                GOLOS_THROW_MISSING_OBJECT("private_message",
                    fc::mutable_variant_object()("from", po.from)("to", po.to)
                    ("start_date", po.start_date)("stop_date", po.stop_date));
            }
        }
    } else if (po.from.size() && po.to.size()) {
        if (!process_private_messages<by_inbox_account>(_db, po, process_action, group, po.to, po.from)) {
            if (!no_msgs_action()) {
                GOLOS_THROW_MISSING_OBJECT("private_message",
                    fc::mutable_variant_object()("from", po.from)("to", po.to)
                    ("start_date", po.start_date)("stop_date", po.stop_date));
            }
        }
    } else if (po.from.size()) {
        if (!process_private_messages<by_outbox>(_db, po, process_action, po.from)) {
            if (!no_msgs_action()) {
                GOLOS_THROW_MISSING_OBJECT("private_message",
                    fc::mutable_variant_object()("from", po.from)
                    ("start_date", po.start_date)("stop_date", po.stop_date));
            }
        }
    } else if (po.to.size()) {
        if (!process_private_messages<by_inbox>(_db, po, process_action, po.to)) {
            if (!no_msgs_action()) {
                GOLOS_THROW_MISSING_OBJECT("private_message",
                    fc::mutable_variant_object()("to", po.to)
                    ("start_date", po.start_date)("stop_date", po.stop_date));
            }
        }
    } else {
        if (!process_private_messages<by_inbox>(_db, po, process_action, requester) &
            !process_private_messages<by_outbox>(_db, po, process_action, requester)
        ) {
            if (!no_msgs_action()) {
                GOLOS_THROW_MISSING_OBJECT("private_message",
                    fc::mutable_variant_object()("requester", requester)
                    ("start_date", po.start_date)("stop_date", po.stop_date));
            }
        }
    }

    auto& contact_idx = _db.get_index<contact_index, by_contact>();
    auto& size_idx = _db.get_index<contact_size_index, by_owner>();

    for (const auto& stat_info: map) {
        const auto& owner = std::get<1>(stat_info.first);

        auto contact_itr = contact_idx.find(stat_info.first);

        FC_ASSERT(contact_itr != contact_idx.end(), "Invalid size");

        auto size_itr = size_idx.find(std::make_tuple(owner, contact_itr->type));

        FC_ASSERT(size_itr != size_idx.end(), "Invalid size");

        const auto& size = stat_info.second;
        if (!contact_action(*contact_itr, *size_itr, size)) {
            _db.modify(*contact_itr, [&](auto& pco) {
                pco.size -= size;
            });

            _db.modify(*size_itr, [&](auto& pcso) {
                pcso.size -= size;
            });
        }
    }
}

struct delete_extension_visitor : private_message_extension_visitor {
    delete_extension_visitor(const account_name_type& _requester,
        const database& db, std::string& _group, std::set<account_name_type>& _mentions,
        account_name_type _delete_from, fc::optional<bool>& _delete_contact)
        : private_message_extension_visitor(_requester, db, _group, _mentions, _delete_from), delete_contact(_delete_contact) {
    }

    fc::optional<bool>& delete_contact;

    using private_message_extension_visitor::operator();

    void operator()(const delete_options& dop) const {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_30__236, "delete_options");
        delete_contact = dop.delete_contact;
    }
};

void private_delete_message_evaluator::do_apply(const private_delete_message_operation& op) {
    if (!_plugin->is_tracked_account(op.from) &&
        !_plugin->is_tracked_account(op.to) &&
        !_plugin->is_tracked_account(op.requester)
    ) {
        return;
    }

    std::string group;
    std::set<account_name_type> mentions;
    fc::optional<bool> delete_contact;
    for (auto& e : op.extensions) {
        e.visit(delete_extension_visitor(op.requester, _db, group, mentions, op.from, delete_contact));
    }
    auto now = _db.head_block_time();
    fc::flat_map<std::tuple<std::string, account_name_type>, contact_size_info> stat_map;

    process_group_message_operation(
        _db, op, group, op.requester, stat_map,
        // process_action
        [&](const message_object& m) -> bool {
            uint32_t unread_messages = 0;

            if (m.read_date == time_point_sec::min()) {
                unread_messages = 1;
            }
            if (!group.size()) {
                // remove from inbox
                auto& inbox_stat = stat_map[std::make_tuple(with_dog(m.from), m.to)];
                inbox_stat.unread_inbox_messages += unread_messages;
                inbox_stat.total_inbox_messages++;
                // remove from outbox
                auto& outbox_stat = stat_map[std::make_tuple(with_dog(m.to), m.from)];
                outbox_stat.unread_outbox_messages += unread_messages;
                outbox_stat.total_outbox_messages++;
            } else {
                if (m.to.size()) {
                    auto& inbox_stat = stat_map[std::make_tuple(group, m.to)];
                    inbox_stat.unread_inbox_messages += unread_messages;
                    inbox_stat.total_inbox_messages++;
                }
                for (const auto& men : m.mentions) {
                    if (men != m.to) {
                        auto& men_stat = stat_map[std::make_tuple(group, men)];
                        men_stat.unread_mentions += 1;
                    }
                }
                auto& outbox_stat = stat_map[std::make_tuple(group, m.from)];
                outbox_stat.total_outbox_messages++;
            }

            if (_plugin->can_call_callbacks()) {
                message_api_object ma(m);
                ma.remove_date = now;

                if (op.requester == op.to) {
                    _plugin->call_callbacks(
                        callback_event_type::remove_inbox, m.from, m.to,
                        fc::variant(callback_message_event(
                            {callback_event_type::remove_inbox, ma})));
                } else {
                    _plugin->call_callbacks(
                        callback_event_type::remove_outbox, m.from, m.to,
                        fc::variant(callback_message_event(
                            {callback_event_type::remove_outbox, ma})));
                }
            }

            _db.remove(m);
            return true;
        },
        // no_msgs_action
        [&]() {
            if (!!delete_contact && *delete_contact) {
                stat_map[std::make_tuple(with_dog(op.from), op.to)] = {};
                stat_map[std::make_tuple(with_dog(op.to), op.from)] = {};
                return true;
            }
            return false;
        },
        // contact_action
        [&](const contact_object& co, const contact_size_object& so, const contact_size_info& size) -> bool {
            if (co.size != size || co.type != unknown) { // not all messages removed or contact is not 'unknown'
                GOLOS_CHECK_VALUE(!delete_contact || !(*delete_contact),
                    "Cannot delete contact without deleting all messages.");
                return false;
            }
            if (!!delete_contact && !(*delete_contact)) {
                return false;
            }
            _db.remove(co);
            if (so.size.total_contacts == 1) {
                _db.remove(so);
            } else {
                _db.modify(so, [&](auto& pcso) {
                    pcso.size.total_contacts--;
                    pcso.size -= size;
                });
            }
            return true;
        },
        !!delete_contact && delete_contact
    );
}

struct private_mark_message_extension_visitor {
    private_mark_message_extension_visitor(account_name_type& _requester,
        const database& db, std::string& _group, std::set<account_name_type>& _mentions)
        : requester(_requester), _db(db), group(_group), mentions(_mentions) {
    }

    account_name_type& requester;
    const database& _db;
    std::string& group;
    std::set<account_name_type>& mentions;

    using result_type = void;

    void operator()(const private_group_options& _pgo) const {
        ASSERT_REQ_HF(STEEMIT_HARDFORK_0_30__236, "private_group_options");

        _db.get<private_group_object, by_name>(_pgo.group);
        group = _pgo.group;
        requester = _pgo.requester;
        mentions = _pgo.mentions;
    }
};

void private_mark_message_evaluator::do_apply(const private_mark_message_operation& op) {
    if (!_plugin->is_tracked_account(op.from) && !_plugin->is_tracked_account(op.to)) {
        return;
    }

    account_name_type requester;
    std::string group;
    std::set<account_name_type> mentions;
    private_mark_message_extension_visitor visitor(requester, _db, group, mentions);
    for (auto& e : op.extensions) {
        e.visit(visitor);
    }
    bool is_group = group.size();

    uint32_t total_marked_messages = 0;
    auto now = _db.head_block_time();
    fc::flat_map<std::tuple<std::string, account_name_type>, contact_size_info> stat_map;

    process_group_message_operation(
        _db, op, group, is_group ? requester : op.to, stat_map,
        // process_action
        [&](const message_object& m) -> bool {
            bool unread = m.read_date == time_point_sec::min();
            bool replied = true;
            auto mentioned = m.mentions.end();
            if (is_group) {
                replied = m.to == requester;
                if (mentions.count(requester)) {
                    mentioned = std::find(m.mentions.begin(), m.mentions.end(),
                        requester);
                }
                unread = (unread && replied) || (mentioned != m.mentions.end());
            }

            if (!unread) {
                return true;
            }

            if (is_group) {
                auto& stat = stat_map[std::make_tuple(group, requester)];
                if (replied) stat.unread_inbox_messages++;
                else if (mentioned != m.mentions.end()) stat.unread_mentions++;
            } else {
                // only recipient can mark messages
                stat_map[std::make_tuple(with_dog(m.from), m.to)].unread_inbox_messages++;
                stat_map[std::make_tuple(with_dog(m.to), m.from)].unread_outbox_messages++;
            }
            total_marked_messages++;

            _db.modify(m, [&](auto& m){
                if (replied) {
                    m.read_date = now;
                }
                if (mentioned != m.mentions.end()) {
                    m.mentions.erase(mentioned);
                }
            });

            if (_plugin->can_call_callbacks()) {
                _plugin->call_callbacks(
                    callback_event_type::mark, m.from, m.to,
                    fc::variant(callback_message_event({callback_event_type::mark, message_api_object(m)})));
            }
            return true;
        },

        // no_msgs_action
        [&]() {
            return false;
        },

        // contact_action
        [&](const contact_object&, const contact_size_object&, const contact_size_info&) -> bool {
            return false;
        }
    );

    GOLOS_CHECK_LOGIC(total_marked_messages > 0,
        logic_errors::no_unread_messages,
        "No unread messages in requested range");
}


struct private_setting_visitor {
    using result_type = void;

    settings_object& _pso;

    private_setting_visitor(settings_object& pso) : _pso(pso) {
    }

    result_type operator()(const ignore_messages_from_unknown_contact& s) {
        _pso.ignore_messages_from_unknown_contact = s.ignore_messages_from_unknown_contact;
    }
};

void private_settings_evaluator::do_apply(const private_settings_operation& op) {
    GOLOS_ASSERT(false, unsupported_operation, "private_settings_operation is not yet supported");

    if (!_plugin->is_tracked_account(op.owner)) {
        return;
    }

    auto& idx = _db.get_index<settings_index, by_owner>();
    auto itr = idx.find(op.owner);

    auto set_settings = [&](auto& pso) {
        pso.owner = op.owner;

        private_setting_visitor visitor(pso);
        for (const auto& s : op.settings) {
            s.visit(visitor);
        }
    };

    if (itr != idx.end()) {
        _db.modify(*itr, set_settings);
    } else {
        _db.create<settings_object>(set_settings);
    }
}

void private_contact_evaluator::do_apply(const private_contact_operation& op) {
    GOLOS_ASSERT(false, unsupported_operation, "private_contact_operation is not yet supported");

    // And this code supports only account-kind, not group

    if (!_plugin->is_tracked_account(op.owner) && !_plugin->is_tracked_account(op.contact)) {
        return;
    }

    auto contact_at = with_dog(op.contact);

    auto& contact_idx = _db.get_index<contact_index, by_contact>();
    auto contact_itr = contact_idx.find(std::make_tuple(contact_at, op.owner));

    _db.get_account(op.contact);

    GOLOS_CHECK_LOGIC(contact_itr != contact_idx.end() || op.type != unknown,
        logic_errors::add_unknown_contact,
        "Can't add unknown contact");

    std::string json_metadata(contact_itr->json_metadata.begin(), contact_itr->json_metadata.end());
    GOLOS_CHECK_LOGIC(contact_itr->type != op.type || op.json_metadata != json_metadata,
        logic_errors::contact_has_not_changed,
        "Contact hasn't changed");

    auto& owner_idx = _db.get_index<contact_size_index, by_owner>();
    auto dst_itr = owner_idx.find(std::make_tuple(op.owner, op.type));

    if (contact_itr != contact_idx.end()) {
        // move counters' score from src to dst contact size type
        auto src_itr = owner_idx.find(std::make_tuple(op.owner, contact_itr->type));
        if (contact_itr->type != op.type) {
            // last contact
            if (src_itr->size.total_contacts == 1) {
                _db.remove(*src_itr);
            } else {
                _db.modify(*src_itr, [&](auto& src) {
                    src.size.total_contacts--;
                    src.size -= contact_itr->size;
               });
            }

            // has messages or type is not unknown
            if (!contact_itr->size.empty() || op.type != unknown) {
                auto modify_counters = [&](auto& dst) {
                    dst.size.total_contacts++;
                    dst.size += contact_itr->size;
                };

                if (dst_itr == owner_idx.end()) {
                    _db.create<contact_size_object>([&](auto& dst) {
                        dst.owner = op.owner;
                        dst.type = op.type;
                        modify_counters(dst);
                    });
                } else {
                    _db.modify(*dst_itr, modify_counters);
                }
            }
        }

        // contact is unknown and no messages
        if (op.type == unknown && contact_itr->size.empty()) {
            _db.remove(*contact_itr);
        } else {
            _db.modify(*contact_itr, [&](auto& plo) {
                plo.type = op.type;
                plo.is_hidden = false;
                from_string(plo.json_metadata, op.json_metadata);
            });
        }
    } else if (op.type != unknown) {
        _db.create<contact_object>([&](auto& plo){
            plo.owner = op.owner;
            from_string(plo.contact, contact_at);
            plo.type = op.type;
            from_string(plo.json_metadata, op.json_metadata);
        });

        contact_itr = contact_idx.find(std::make_tuple(op.owner, contact_at));

        if (dst_itr == owner_idx.end()) {
            _db.create<contact_size_object>([&](auto& pcso) {
                pcso.owner = op.owner;
                pcso.type = op.type;
                pcso.size.total_contacts = 1;
            });
        } else {
            _db.modify(*dst_itr, [&](auto& pcso) {
                pcso.size.total_contacts++;
            });
        }
    }

    if (_plugin->can_call_callbacks()) {
        _plugin->call_callbacks(
            callback_event_type::contact, op.owner, op.contact,
            fc::variant(callback_contact_event(
                {callback_event_type::contact, contact_api_object(*contact_itr, _db)})));
    }
}

void private_group_evaluator::do_apply(const private_group_operation& op) {
    ASSERT_REQ_HF(STEEMIT_HARDFORK_0_30__236, "private_group_operation");

    const auto* pgo = _db.find<private_group_object, by_name>(op.name);

    auto now = _db.head_block_time();

    if (pgo) {
        GOLOS_CHECK_LOGIC(pgo->owner == op.creator,
            logic_errors::already_exists,
            "Group already exists.");
        GOLOS_CHECK_LOGIC(pgo->is_encrypted == op.is_encrypted,
            logic_errors::cannot_change_group_encrypted,
            "Cannot make encrypted group not encrypted or vice-versa.");

        if (op.privacy == private_group_privacy::private_group) {
            GOLOS_CHECK_LOGIC(pgo->is_encrypted,
                logic_errors::cannot_change_group_encrypted,
                "Cannot make group private - messages in this group are not encrypted.");
        }

        const auto& pgm_idx = _db.get_index<private_group_member_index, by_group_type>();

        if (pgo->privacy != private_group_privacy::public_group &&
            op.privacy == private_group_privacy::public_group) {

            auto pend = pgm_idx.lower_bound(std::make_tuple(pgo->name, private_group_member_type::pending));
            for (; pend != pgm_idx.end() && pend->group == pgo->name &&
                pend->member_type == private_group_member_type::pending; ++pend) {

                _db.modify(*pend, [&](auto& pgmo) {
                    pgmo.member_type = private_group_member_type::member;
                    pgmo.updated = now;
                });
            }
        }

        _db.modify(*pgo, [&](auto& pgo) {
            from_string(pgo.json_metadata, op.json_metadata);
            pgo.privacy = op.privacy;
            pgo.pendings = 0;
        });

        return;
    }

    auto fee = _db.get_witness_schedule_object().median_props.private_group_cost;
    if (fee.amount != 0) {
        const auto& creator = _db.get_account(op.creator);
        GOLOS_CHECK_BALANCE(_db, creator, MAIN_BALANCE, fee);
        _db.adjust_balance(creator, -fee);
        _db.adjust_balance(_db.get_account(STEEMIT_WORKER_POOL_ACCOUNT), fee);
    }

    if (op.privacy == private_group_privacy::private_group) {
        GOLOS_CHECK_LOGIC(op.is_encrypted,
            logic_errors::cannot_change_group_encrypted,
            "Cannot create private group not encrypted.");
    }

    const auto& pgo_idx = _db.get_index<private_group_index, by_owner>();
    auto pgo_itr = pgo_idx.lower_bound(op.creator);
    uint32_t count = 0;
    for (; pgo_itr != pgo_idx.end() && pgo_itr->owner == op.creator; ++pgo_itr) {
        ++count;
        if (count == MAX_GROUPS_LIMIT) break;
    }

    GOLOS_CHECK_LOGIC(count < MAX_GROUPS_LIMIT,
        logic_errors::too_many_groups,
        "Too many groups.");

    _db.create<private_group_object>([&](auto& pgo) {
        pgo.owner = op.creator;
        from_string(pgo.name, op.name);
        from_string(pgo.json_metadata, op.json_metadata);
        pgo.is_encrypted = op.is_encrypted;
        pgo.privacy = op.privacy;
        pgo.created = now;
    });
}

void private_group_delete_evaluator::do_apply(const private_group_delete_operation& op) {
    ASSERT_REQ_HF(STEEMIT_HARDFORK_0_30__236, "private_group_delete_operation");

    const auto& pgo = _db.get<private_group_object, by_name>(op.name);

    GOLOS_CHECK_LOGIC(pgo.owner == op.owner,
        logic_errors::unauthorized,
        "Not your group.");

    const auto& pgm_idx = _db.get_index<private_group_member_index, by_group_type>();
    auto pgm_itr = pgm_idx.lower_bound(op.name);
    while (pgm_itr != pgm_idx.end()) {
        if (pgm_itr->group != pgo.name) {
            break;
        }
        const auto& pgm = *pgm_itr;
        ++pgm_itr;
        _db.remove(pgm);
    }

    const auto& con_idx = _db.get_index<contact_index, by_contact>();
    auto con_itr = con_idx.lower_bound(pgo.name);
    while (con_itr != con_idx.end()) {
        if (con_itr->contact != pgo.name) {
            break;
        }
        const auto& con = *con_itr;
        ++con_itr;
        _db.remove(con);
    }

    // In future can be optimized: separated in time (blocks),
    // and forbid group creation if messages not yet removed
    const auto& msg_idx = _db.get_index<message_index, by_group>();
    auto msg_itr = msg_idx.lower_bound(pgo.name);
    while (msg_itr != msg_idx.end()) {
        if (msg_itr->group != pgo.name) {
            break;
        }
        const auto& msg = *msg_itr;
        ++msg_itr;
        _db.remove(msg);
    }

    _db.remove(pgo);
}

void private_group_member_evaluator::do_apply(const private_group_member_operation& op) {
    ASSERT_REQ_HF(STEEMIT_HARDFORK_0_30__236, "private_group_member_operation");

    const auto& member = _db.get_account(op.member);

    const auto& pgo = _db.get<private_group_object, by_name>(op.name);

    auto moders = pgo.moders;
    auto members = pgo.members;
    auto pendings = pgo.pendings;
    auto banneds = pgo.banneds;

    const auto& pgm_idx = _db.get_index<private_group_member_index, by_account_group>();

    auto requester_itr = pgm_idx.find(std::make_tuple(op.requester, op.name));
    bool requester_exists = requester_itr != pgm_idx.end();

    bool is_owner = pgo.owner == op.requester;
    bool is_moder = is_owner || (requester_exists && requester_itr->member_type == private_group_member_type::moder);
    bool is_same = op.requester == op.member;

    auto member_itr = pgm_idx.find(std::make_tuple(op.member, op.name));

    GOLOS_CHECK_LOGIC(op.member != pgo.owner, logic_errors::unauthorized, "Group owner is special status, it can not be member.");

    if (op.member_type == private_group_member_type::member) {
        if (pgo.privacy != private_group_privacy::public_group) {
            GOLOS_CHECK_LOGIC(is_moder, logic_errors::unauthorized, "Only moderators can add members to group or unban them.");
        }
        members++;
    }

    else if (op.member_type == private_group_member_type::pending) {
        GOLOS_CHECK_LOGIC(pgo.privacy != private_group_privacy::public_group,
            logic_errors::group_is_public, "Group is public.");
        GOLOS_CHECK_LOGIC(is_same, logic_errors::unauthorized, "User can request membership only by theirself.");
        check_golos_power(member, _db);
        pendings++;
    }

    else if (op.member_type == private_group_member_type::moder) {
        GOLOS_CHECK_LOGIC(is_owner, logic_errors::unauthorized, "Only owner can make members moderators.");
        moders++;
    }

    else if (op.member_type == private_group_member_type::retired) {
        GOLOS_CHECK_LOGIC(is_same, logic_errors::unauthorized, "User can retire only by theirself.");

        bool is_banned = requester_exists && requester_itr->member_type == private_group_member_type::banned;
        GOLOS_CHECK_LOGIC(!is_banned, logic_errors::unauthorized, "You are banned.");
        GOLOS_CHECK_LOGIC(requester_exists, logic_errors::unauthorized, "You are not in group.");
    }

    else if (op.member_type == private_group_member_type::banned) {
        GOLOS_CHECK_LOGIC(is_moder && !is_same, logic_errors::unauthorized, "Only moderators can ban members.");
        banneds++;
    }

    auto now = _db.head_block_time();

    if (member_itr != pgm_idx.end()) {
        if (member_itr->member_type == private_group_member_type::member) {
            members--;
        } else if (member_itr->member_type == private_group_member_type::moder) {
            moders--;
        } else if (member_itr->member_type == private_group_member_type::pending) {
            pendings--;
        } else if (member_itr->member_type == private_group_member_type::banned) {
            banneds--;
        }

        if (op.member_type == private_group_member_type::retired) {
            _db.remove(*member_itr);
        } else {
            _db.modify(*member_itr, [&](auto& pgmo) {
                pgmo.member_type = op.member_type;
                pgmo.updated = now;
            });
        }

        _db.modify(pgo, [&](auto& pgo) {
            pgo.moders = moders;
            pgo.members = members;
            pgo.pendings = pendings;
            pgo.banneds = banneds;
        });
        return;
    }

    _db.create<private_group_member_object>([&](auto& pgmo) {
        from_string(pgmo.group, op.name);
        pgmo.account = op.member;
        from_string(pgmo.json_metadata, op.json_metadata);
        pgmo.member_type = op.member_type;
        pgmo.invited = op.requester;
        pgmo.joined = now;
        pgmo.updated = now;
    });

    _db.modify(pgo, [&](auto& pgo) {
        pgo.moders = moders;
        pgo.members = members;
        pgo.pendings = pendings;
        pgo.banneds = banneds;
    });
}

} } } // golos::plugins::private_message
