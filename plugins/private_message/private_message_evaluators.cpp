#include <golos/plugins/private_message/private_message_operations.hpp>
#include <golos/plugins/private_message/private_message_objects.hpp>
#include <golos/plugins/private_message/private_message_exceptions.hpp>
#include <golos/plugins/private_message/private_message_evaluators.hpp>

namespace golos { namespace plugins { namespace private_message {

void private_message_evaluator::do_apply(const private_message_operation& op) {
    if (!_plugin->is_tracked_account(op.from) && !_plugin->is_tracked_account(op.to)) {
        return;
    }

    auto& contact_idx = _db.get_index<contact_index, by_contact>();
    auto contact_itr = contact_idx.find(std::make_tuple(op.to, op.from));

    auto& cfg_idx = _db.get_index<settings_index, by_owner>();
    auto cfg_itr = cfg_idx.find(op.to);

    _db.get_account(op.to);

    GOLOS_CHECK_LOGIC(contact_itr == contact_idx.end() || contact_itr->type != ignored,
        logic_errors::sender_in_ignore_list,
        "Sender is in the ignore list of recipient");

    GOLOS_CHECK_LOGIC(
        (cfg_itr == cfg_idx.end() || !cfg_itr->ignore_messages_from_unknown_contact) ||
        (contact_itr != contact_idx.end() && contact_itr->type == pinned),
        logic_errors::recepient_ignores_messages_from_unknown_contact,
        "Recipient accepts messages only from his contact list");

    auto& id_idx = _db.get_index<message_index, by_nonce>();
    auto id_itr = id_idx.find(std::make_tuple(op.from, op.to, op.nonce));

    if (op.update && id_itr == id_idx.end()) {
        GOLOS_THROW_MISSING_OBJECT("private_message",
            fc::mutable_variant_object()("from", op.from)("to", op.to)("nonce", op.nonce));
    } else if (!op.update && id_itr != id_idx.end()){
        GOLOS_THROW_OBJECT_ALREADY_EXIST("private_message",
            fc::mutable_variant_object()("from", op.from)("to", op.to)("nonce", op.nonce));
    }

    auto now = _db.head_block_time();

    auto set_message = [&](message_object& pmo) {
        pmo.from_memo_key = op.from_memo_key;
        pmo.to_memo_key = op.to_memo_key;
        pmo.checksum = op.checksum;
        pmo.receive_date = now;
        pmo.encrypted_message.resize(op.encrypted_message.size());
        std::copy(
            op.encrypted_message.begin(), op.encrypted_message.end(),
            pmo.encrypted_message.begin());
    };

    if (id_itr == id_idx.end()) {
        _db.create<message_object>([&](auto& pmo) {
            pmo.from = op.from;
            pmo.to = op.to;
            pmo.nonce = op.nonce;
            pmo.inbox_create_date = now;
            pmo.outbox_create_date = now;
            pmo.read_date = time_point_sec::min();
            pmo.remove_date = time_point_sec::min();
            set_message(pmo);
        });
        id_itr = id_idx.find(std::make_tuple(op.from, op.to, op.nonce));
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
    auto inc_counters = [&](auto& size_object, const bool is_send) {
        if (is_send) {
            size_object.total_outbox_messages++;
            size_object.unread_outbox_messages++;
        } else {
            size_object.total_inbox_messages++;
            size_object.unread_inbox_messages++;
        }
    };

    // Update global counters by type of contact
    auto modify_size = [&](auto& owner, auto type, const bool is_new_contact, const bool is_send) {
        auto modify_counters = [&](auto& pcso) {
            inc_counters(pcso.size, is_send);
            if (is_new_contact) {
                pcso.size.total_contacts++;
            }
        };

        auto size_itr = size_idx.find(std::make_tuple(owner, type));
        if (size_idx.end() == size_itr) {
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
    auto modify_contact = [&](auto& owner, auto& contact, auto type, const bool is_send) {
        bool is_new_contact;
        auto contact_itr = contact_idx.find(std::make_tuple(owner, contact));
        if (contact_idx.end() != contact_itr) {
            _db.modify(*contact_itr, [&](auto& pco) {
                inc_counters(pco.size, is_send);
            });
            is_new_contact = false;
            type = contact_itr->type;
        } else {
            _db.create<contact_object>([&](auto& pco) {
                pco.owner = owner;
                pco.contact = contact;
                pco.type = type;
                inc_counters(pco.size, is_send);
            });
            is_new_contact = true;

            if (_plugin->can_call_callbacks()) {
                contact_itr = contact_idx.find(std::make_tuple(owner, contact));
                _plugin->call_callbacks(
                    callback_event_type::contact, owner, contact,
                    fc::variant(callback_contact_event(
                        {callback_event_type::contact, contact_api_object(*contact_itr)})));
            }
        }
        modify_size(owner, type, is_new_contact, is_send);
    };

    if (!op.update) {
        modify_contact(op.from, op.to, unknown, true);
        modify_contact(op.to, op.from, unknown, false);
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

template <typename Operation, typename Map, typename ProcessAction, typename ContactAction>
void process_group_message_operation(
    database& _db, const Operation& po, const std::string& requester,
    Map& map, ProcessAction&& process_action, ContactAction&& contact_action
) {
    if (po.nonce != 0) {
        auto& idx = _db.get_index<message_index, by_nonce>();
        auto itr = idx.find(std::make_tuple(po.from, po.to, po.nonce));

        if (itr == idx.end()) {
            GOLOS_THROW_MISSING_OBJECT("private_message",
                fc::mutable_variant_object()("from", po.from)("to", po.to)("nonce", po.nonce));
        }

        process_action(*itr);
    } else if (po.from.size() && po.to.size() && po.from == requester) {
        if (!process_private_messages<by_outbox_account>(_db, po, process_action, po.from, po.to)) {
            GOLOS_THROW_MISSING_OBJECT("private_message",
                fc::mutable_variant_object()("from", po.from)("to", po.to)
                ("start_date", po.start_date)("stop_date", po.stop_date));
        }
    } else if (po.from.size() && po.to.size()) {
        if (!process_private_messages<by_inbox_account>(_db, po, process_action, po.to, po.from)) {
            GOLOS_THROW_MISSING_OBJECT("private_message",
                fc::mutable_variant_object()("from", po.from)("to", po.to)
                ("start_date", po.start_date)("stop_date", po.stop_date));
        }
    } else if (po.from.size()) {
        if (!process_private_messages<by_outbox>(_db, po, process_action, po.from)) {
            GOLOS_THROW_MISSING_OBJECT("private_message",
                fc::mutable_variant_object()("from", po.from)
                ("start_date", po.start_date)("stop_date", po.stop_date));
        }
    } else if (po.to.size()) {
        if (!process_private_messages<by_inbox>(_db, po, process_action, po.to)) {
            GOLOS_THROW_MISSING_OBJECT("private_message",
                fc::mutable_variant_object()("to", po.to)
                ("start_date", po.start_date)("stop_date", po.stop_date));
        }
    } else {
        if (!process_private_messages<by_inbox>(_db, po, process_action, requester) &
            !process_private_messages<by_outbox>(_db, po, process_action, requester)
        ) {
            GOLOS_THROW_MISSING_OBJECT("private_message",
                fc::mutable_variant_object()("requester", requester)
                ("start_date", po.start_date)("stop_date", po.stop_date));
        }
    }

    auto& contact_idx = _db.get_index<contact_index, by_contact>();
    auto& size_idx = _db.get_index<contact_size_index, by_owner>();

    for (const auto& stat_info: map) {
        const auto& owner = std::get<0>(stat_info.first);
        const auto& size = stat_info.second;
        auto contact_itr = contact_idx.find(stat_info.first);

        FC_ASSERT(contact_idx.end() != contact_itr, "Invalid size");

        auto size_itr = size_idx.find(std::make_tuple(owner, contact_itr->type));

        FC_ASSERT(size_idx.end() != size_itr, "Invalid size");

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

void private_delete_message_evaluator::do_apply(const private_delete_message_operation& op) {
    if (!_plugin->is_tracked_account(op.from) &&
        !_plugin->is_tracked_account(op.to) &&
        !_plugin->is_tracked_account(op.requester)
    ) {
        return;
    }

    auto now = _db.head_block_time();
    fc::flat_map<std::tuple<account_name_type, account_name_type>, contact_size_info> stat_map;

    process_group_message_operation(
        _db, op, op.requester, stat_map,
        /* process_action */
        [&](const message_object& m) -> bool {
            uint32_t unread_messages = 0;

            if (m.read_date == time_point_sec::min()) {
                unread_messages = 1;
            }
            // remove from inbox
            auto& inbox_stat = stat_map[std::make_tuple(m.to, m.from)];
            inbox_stat.unread_inbox_messages += unread_messages;
            inbox_stat.total_inbox_messages++;
            // remove from outbox
            auto& outbox_stat = stat_map[std::make_tuple(m.from, m.to)];
            outbox_stat.unread_outbox_messages += unread_messages;
            outbox_stat.total_outbox_messages++;

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
        /* contact_action */
        [&](const contact_object& co, const contact_size_object& so, const contact_size_info& size) -> bool {
            if (co.size != size || co.type != unknown) { // not all messages removed or contact is not 'unknown'
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
        }
    );
}

void private_mark_message_evaluator::do_apply(const private_mark_message_operation& op) {
    if (!_plugin->is_tracked_account(op.from) && !_plugin->is_tracked_account(op.to)) {
        return;
    }

    uint32_t total_marked_messages = 0;
    auto now = _db.head_block_time();
    fc::flat_map<std::tuple<account_name_type, account_name_type>, contact_size_info> stat_map;

    process_group_message_operation(
        _db, op, op.to, stat_map,
        /* process_action */
        [&](const message_object& m) -> bool {
            if (m.read_date != time_point_sec::min()) {
                return true;
            }
            // only recipient can mark messages
            stat_map[std::make_tuple(m.to, m.from)].unread_inbox_messages++;
            stat_map[std::make_tuple(m.from, m.to)].unread_outbox_messages++;
            total_marked_messages++;

            _db.modify(m, [&](auto& m){
                m.read_date = now;
            });

            if (_plugin->can_call_callbacks()) {
                _plugin->call_callbacks(
                    callback_event_type::mark, m.from, m.to,
                    fc::variant(callback_message_event({callback_event_type::mark, message_api_object(m)})));
            }
            return true;
        },

        /* contact_action */
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

    if (idx.end() != itr) {
        _db.modify(*itr, set_settings);
    } else {
        _db.create<settings_object>(set_settings);
    }
}

void private_contact_evaluator::do_apply(const private_contact_operation& op) {
    if (!_plugin->is_tracked_account(op.owner) && !_plugin->is_tracked_account(op.contact)) {
        return;
    }

    auto& contact_idx = _db.get_index<contact_index, by_contact>();
    auto contact_itr = contact_idx.find(std::make_tuple(op.owner, op.contact));

    _db.get_account(op.contact);

    GOLOS_CHECK_LOGIC(contact_idx.end() != contact_itr || op.type != unknown,
        logic_errors::add_unknown_contact,
        "Can't add unknown contact");

    std::string json_metadata(contact_itr->json_metadata.begin(), contact_itr->json_metadata.end());
    GOLOS_CHECK_LOGIC(contact_itr->type != op.type || op.json_metadata != json_metadata,
        logic_errors::contact_has_not_changed,
        "Contact hasn't changed");

    auto& owner_idx = _db.get_index<contact_size_index, by_owner>();
    auto dst_itr = owner_idx.find(std::make_tuple(op.owner, op.type));

    if (contact_idx.end() != contact_itr) {
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

                if (owner_idx.end() == dst_itr) {
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
                from_string(plo.json_metadata, op.json_metadata);
            });
        }
    } else if (op.type != unknown) {
        _db.create<contact_object>([&](auto& plo){
            plo.owner = op.owner;
            plo.contact = op.contact;
            plo.type = op.type;
            from_string(plo.json_metadata, op.json_metadata);
        });

        contact_itr = contact_idx.find(std::make_tuple(op.owner, op.contact));

        if (owner_idx.end() == dst_itr) {
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
                {callback_event_type::contact, contact_api_object(*contact_itr)})));
    }
}

} } } // golos::plugins::private_message
