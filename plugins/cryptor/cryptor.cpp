#include <fc/crypto/aes.hpp>
#include <fc/crypto/base64.hpp>

#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/plugins/cryptor/cryptor_queries.hpp>
#include <golos/plugins/cryptor/cryptor.hpp>
#include <golos/plugins/private_message/private_message_objects.hpp>
#include <golos/protocol/exceptions.hpp>
#include <golos/protocol/donate_targets.hpp>
#include <golos/chain/operation_notification.hpp>

namespace golos { namespace plugins { namespace cryptor {

struct post_operation_visitor {
    golos::chain::database& _db;

    post_operation_visitor(golos::chain::database& db) : _db(db) {
    }

    using result_type = void;

    template<typename T>
    result_type operator()(const T&) const {
    }

    result_type operator()(const donate_operation& op) const {
        if (!_db.has_index<crypto_buyer_index>()) {
            return;
        }

        auto donate = get_blog_donate(op);
        if (!donate) {
            return;
        }

        if (op.amount.symbol != STEEM_SYMBOL || op.to != donate->author) {
            return;
        }

        auto hashlink = _db.make_hashlink(donate->permlink);
        const auto* comment = _db.find_extras(donate->author, hashlink);
        if (!comment || comment->decrypt_fee.amount == 0) {
            return;
        }

        const auto& idx = _db.get_index<crypto_buyer_index, by_comment_donater>();
        auto itr = idx.find(std::make_tuple(comment->id, op.from));
        if (itr != idx.end()) {
            _db.modify(*itr, [&](auto& cbo) {
                cbo.amount += op.amount;
                if (!itr->buyed && cbo.amount >= comment->decrypt_fee) {
                    cbo.buyed = true;
                }
            });
        } else {
            _db.create<crypto_buyer_object>([&](auto& cbo) {
                cbo.donater = op.from;
                cbo.comment = comment->id;
                cbo.amount = op.amount;
                if (cbo.amount >= comment->decrypt_fee) {
                    cbo.buyed = true;
                }
            });
        }
    }
};

class cryptor::cryptor_impl final {
public:
    cryptor_impl()
            : _db(appbase::app().get_plugin<golos::plugins::chain::plugin>().db()) {
    }

    ~cryptor_impl() {
    }

    std::string mark = "_m_:";
    char mark_sep = ':';
    char mark_author = '@';
    char mark_group = 'g';
    size_t mark_max_length = 128;

    void post_operation(const operation_notification& note) const {
        try {
            note.op.visit(post_operation_visitor(_db));
        } catch (const fc::assert_exception&) {
            if (_db.is_producing()) {
                throw;
            }
        }
    }

    encrypted_api_object encrypt_body(const encrypt_query& query) const {
        encrypted_api_object res;
        res.status = cryptor_status::err;
        res.error = "unknown";

        try {
            bool is_group = query.group.size();

            if ((is_group ? groups_cryptor_key : cryptor_key).size() < 16) {
                res.status = cryptor_status::err;
                res.error = "unavailable_no_key_in_config";
                return res;
            }

            auto encrypt_key = fc::sha512::hash(cryptor_key);

            auto body = mark;
            if (is_group) {
                body = body + mark_group + mark_sep + query.group;

                encrypt_key = fc::sha512::hash(groups_cryptor_key);
            } else {
                body = body + mark_author + mark_sep + query.author;
            }
            body += mark_sep + query.body;

            auto body_data = std::vector<char>(body.begin(), body.end());
            auto result = fc::aes_encrypt(encrypt_key, body_data);
            res.encrypted = fc::base64_encode((unsigned char const *)&result[0], result.size());

            res.status = cryptor_status::ok;
            res.error = "";
        } FC_CAPTURE_AND_LOG((query));

        return res;
    }

    decrypted_api_object login(const login_data& query, bool is_group = false) const {
        decrypted_api_object res;
        res.status = cryptor_status::ok;

        auto head = _db.with_weak_read_lock([&]() {
            return _db.head_block_num();
        });
        if (head < query.signed_data.head_block_number) {
            res.status = cryptor_status::err;
            res.login_error = "head_block_num_in_future";
            return res;
        }

#ifdef STEEMIT_BUILD_TESTNET
        uint32_t lifetime = 5;
#else
        uint32_t lifetime = is_group ?
            STEEMIT_BLOCKS_PER_DAY :
            STEEMIT_BLOCKS_PER_HOUR;
#endif
        if (head - query.signed_data.head_block_number > lifetime) {
            res.status = cryptor_status::err;
            res.login_error = "head_block_num_too_old";
            return res;
        }

        auto exists = _db.with_weak_read_lock([&]() {
            return _db.find_account(query.account) != nullptr;
        });
        if (!exists) {
            res.status = cryptor_status::err;
            res.login_error = "account_not_exists";
            return res;
        }

        auto sig_hash = fc::sha256::hash(fc::to_string(query.signed_data.head_block_number));

        fc::ecc::public_key key;
        try {
            key = fc::ecc::public_key(query.signature, sig_hash);
        } catch (...) {
            res.status = cryptor_status::err;
            res.login_error = "illformed_signature";
            return res;
        }
        auto pk = public_key_type(key);

        auto acc_keys = _db.with_weak_read_lock([&]() {
            return _db.get_authority(query.account).posting.get_keys();
        });
        if (!acc_keys.size() || std::find(acc_keys.begin(), acc_keys.end(), pk) == acc_keys.end()) {
            res.status = cryptor_status::err;
            res.login_error = "wrong_signature";
            return res;
        }

        return res;
    }

    bool get_encrypted_object(decrypted_result& dr, fc::variant_object& jobj,
        const std::string& body, const std::string& check_type) const {

        fc::variant jvar;
        try {
            jvar = fc::json::from_string(body);
            jobj = jvar.get_object();
        } catch (...) {
            dr.err = "wrong_json_or_not_encrypted";
            return false;
        }

        auto spec_itr = jobj.find("t");
        if (spec_itr == jobj.end() || !spec_itr->value().is_string()) {
            dr.err = "no field `t` as valid string";
            return false;
        }
        if (check_type.size()) {
            auto spec_type = spec_itr->value().as_string();
            if (spec_type != check_type) {
                dr.err = "not_encrypted";
                return false;
            }
        }
        return true;
    }

    bool decrypt_body(decrypted_result& dr, std::string body, const fc::sha512& key,
            char mark_type, const std::string& mark_name, bool mark_required = true) const {
        std::string dec;
        try {
            body = fc::base64_decode(body);

            auto body_data = std::vector<char>(body.begin(), body.end());
            auto result = fc::aes_decrypt(key, body_data);
            dec = std::string(result.begin(), result.end());
        } catch (...) {
            dr.err = "cannot_decrypt";
            return false;
        }

        if (!boost::algorithm::starts_with(dec, mark)) {
            if (mark_required) {
                dr.err = "no_mark";
                return false;
            }
            dr.body = dec;
            return true;
        }
        char found_type = '\0';
        bool name_started = false;
        std::string name;
        size_t mark_end = 0;
        try {
            for (size_t i = mark.size(); i < std::min(mark_max_length, dec.size()); ++i) {
                if (found_type == '\0') {
                    found_type = dec[i];
                    continue;
                }
                if (name_started) {
                    if (dec[i] == mark_sep) {
                        mark_end = i + 1;
                        break;
                    }
                    name += dec[i];
                } else {
                    if (dec[i] != mark_sep) {
                        dr.err = "wrong_mark_type_length";
                        return false;
                    } else if (found_type != mark_type) {
                        dr.err = "wrong_mark_type";
                        return false;
                    }
                    name_started = true;
                }
            }
            if (mark_end == 0) {
                dr.err = "wrong_mark_format";
                return false;
            }
            if (name != mark_name) {
                dr.err = "wrong_mark";
                return false;
            }
            dr.body = dec.substr(mark_end);
        } catch (...) {
            dr.err = "cannot_check_mark";
            return false;
        }
        return true;
    }

    decrypted_api_object decrypt_comments(const decrypt_query& query) const {
        decrypted_api_object res;

        struct decrypt_entry {
            std::string author;
            std::string permlink;
            hashlink_type hashlink = hashlink_type();
            std::string body;
        };

        if (cryptor_key.size() < 16) {
            res.status = cryptor_status::err;
            res.error = "unavailable_no_key_in_config";
            return res;
        }

        auto login_res = login(query);
        if (login_res.status != cryptor_status::ok) {
            return login_res;
        }

        auto encrypt_key = fc::sha512::hash(cryptor_key);

        auto decrypt = [&](const decrypt_entry& de, decrypted_result& dr) {
            auto body = _db.with_weak_read_lock([&]() -> std::string {
                if (de.body.size())  {
                    return de.body;
                }

                const comment_object* post = nullptr;
                if (de.hashlink != hashlink_type()) {
                    post = _db.find_comment(de.author, de.hashlink);
                } else {
                    post = _db.find_comment_by_perm(de.author, de.permlink);
                }
                if (post == nullptr) {
                    return "";
                }

                using golos::plugins::social_network::comment_content_index;
                if (!_db.has_index<comment_content_index>()) {
                    return "";
                }
                const auto& idx = _db.get_index<comment_content_index,
                    golos::plugins::social_network::by_comment
                >();
                auto itr = idx.find(post->id);
                if (itr == idx.end()) {
                    return "";
                }

                return to_string(itr->body);
            });

            fc::variant_object jobj;
            if (!get_encrypted_object(dr, jobj, body, "e")) {
                return;
            }

            auto ver_itr = jobj.find("v");
            if (ver_itr == jobj.end() || !ver_itr->value().is_uint64()) {
                dr.err = "no field `v` as valid uint64";
                return;
            }
            auto version = ver_itr->value().as_uint64();
            if (version < 2) {
                dr.err = "not_encrypted";
                return;
            }

            auto c_itr = jobj.find("c");
            if (c_itr == jobj.end() || !c_itr->value().is_string()) {
                dr.err = "no field `c` as valid string";
                return;
            }
            body = c_itr->value().as_string();

            decrypt_body(dr, body, encrypt_key, mark_author, de.author, false);
        };

        std::vector<decrypt_entry> entries;

        std::set<account_name_type> authors;
        std::map<account_name_type, sub_options> inactive_authors;

        for (const auto& entry : query.entries) {
            decrypt_entry de;

            auto author = entry["author"].as_string();
            de.author = author;

            auto permlink_itr = entry.find("permlink");
            if (permlink_itr != entry.end()) {
                de.permlink = permlink_itr->value().as_string();
            }

            auto hashlink_itr = entry.find("hashlink");
            if (hashlink_itr != entry.end()) {
                de.hashlink = hashlink_itr->value().as<hashlink_type>();
            }
            auto body_itr = entry.find("body");
            if (body_itr != entry.end()) {
                de.body = body_itr->value().as_string();
            }

            GOLOS_CHECK_PARAM(query.entries, {
                GOLOS_CHECK_VALUE(de.hashlink != hashlink_type() || de.permlink.size() || de.body.size(),
                    "Each entry should have hashlink or permlink");
            });

            entries.push_back(std::move(de));

            if (author == query.account) {
                continue;
            }

            _db.with_weak_read_lock([&]() {
                const auto& idx = _db.get_index<paid_subscriber_index, by_author_oid_subscriber>();
                auto itr = idx.find(std::make_tuple(author, query.oid, query.account));
                if (itr != idx.end()) {
                    if (itr->active) {
                        authors.insert(author);
                    } else {
                        inactive_authors[author] = sub_options{ itr->cost, itr->tip_cost };
                    }
                }
            });
        }

        std::map<account_name_type, sub_options> pso;

        std::vector<const decrypt_entry*> failed;
        std::vector<size_t> failed_res;

        for (const auto& de : entries) {
            const auto& author = de.author;

            decrypted_result dr;
            dr.author = author;
            dr.permlink = de.permlink;
            dr.hashlink = de.hashlink;

            auto add_failed = [&]() {
                failed.push_back(&de);
                failed_res.push_back(res.results.size() - 1);
            };

            if (author != query.account && !authors.count(author)) {
                auto ina_itr = inactive_authors.find(author);
                if (ina_itr != inactive_authors.end()) {
                    dr.err = "inactive";
                    dr.sub = ina_itr->second;
                    res.results.push_back(std::move(dr));
                    add_failed();
                    continue;
                }

                sub_options opts;
                auto itr = pso.find(author);
                if (itr != pso.end()) {
                    opts = itr->second;

                    dr.err = "no_sponsor";
                    dr.sub = opts;
                    res.results.push_back(std::move(dr));
                    add_failed();
                } else {
                    _db.with_weak_read_lock([&]() {
                        const auto& idx = _db.get_index<paid_subscription_index, by_author_oid>();
                        auto itr = idx.find(std::make_tuple(author, query.oid));
                        if (itr != idx.end()) {
                            opts.cost = itr->cost;
                            opts.tip_cost = itr->tip_cost;
                            pso[author] = opts;

                            dr.err = "no_sponsor";
                            dr.sub = opts;
                            res.results.push_back(std::move(dr));
                            add_failed();
                        } else {
                            dr.err = "no_sub";
                            res.results.push_back(std::move(dr));
                            add_failed();
                        }
                    });
                }
                continue;
            }

            decrypt(de, dr);
            res.results.push_back(std::move(dr));
        }

        for (size_t i = 0; i < failed.size(); ++i) {
            const auto* de = failed[i];
            auto f = failed_res[i];
            auto& dr = res.results[f];

            asset decrypt_fee{0, STEEM_SYMBOL};
            bool donated = _db.with_weak_read_lock([&]() {
                if (!_db.has_index<crypto_buyer_index>()) {
                    return false;
                }

                const comment_extras_object* extras = nullptr;
                if (de->hashlink != hashlink_type()) {
                    extras = _db.find_extras(de->author, de->hashlink);
                } else {
                    extras = _db.find_extras(de->author, _db.make_hashlink(de->permlink));
                }

                if (!extras) {
                    return false;
                }
                decrypt_fee = extras->decrypt_fee;

                const auto& cb_idx = _db.get_index<crypto_buyer_index, by_comment_donater>();
                auto cb_itr = cb_idx.find(std::make_tuple(extras->id, query.account));
                if (cb_itr != cb_idx.end()) {
                    if (cb_itr->buyed) {
                        return true;
                    } else {
                        decrypt_fee -= cb_itr->amount;
                    }
                }
                return false;
            });

            if (donated) {
                dr.err = "";
                decrypt(*de, dr);
            } else if (decrypt_fee.amount.value) {
                dr.decrypt_fee = decrypt_fee;
            }
        }

        return res;
    }

    decrypted_api_object decrypt_messages(const decrypt_messages_query& query) const {
        decrypted_api_object res;

        if (groups_cryptor_key.size() < 16) {
            res.status = cryptor_status::err;
            res.error = "unavailable_no_key_in_config";
            return res;
        }

        auto login_res = login(query, true);
        if (login_res.status != cryptor_status::ok) {
            return login_res;
        }

        using namespace golos::plugins::private_message;

        bool has_plugin = _db.with_weak_read_lock([&]() -> bool {
            return _db.has_index<message_index>();
        });
        if (!has_plugin) {
            res.status = cryptor_status::err;
            res.error = "no_private_message_plugin";
            return res;
        }

        struct group_data {
            bool exists = true;
            private_group_privacy privacy = private_group_privacy::private_group;
            bool can_read = true;
        };
        std::map<std::string, group_data> groups;

        auto load_group = [&](const std::string& group) {
            group_data gd;

            _db.with_weak_read_lock([&]() {
                const auto& idx = _db.get_index<private_group_index, by_name>();
                auto itr = idx.find(group);
                if (itr == idx.end()) {
                    gd.exists = false;
                }
                gd.privacy = itr->privacy;
                if (gd.privacy == private_group_privacy::private_group) {
                    gd.can_read = itr->owner == query.account;
                    if (!gd.can_read) {
                        const auto& pgm_idx = _db.get_index<private_group_member_index, by_group_account>();
                        auto pgm_itr = pgm_idx.find(std::make_tuple(group, query.account));
                        gd.can_read = pgm_itr != pgm_idx.end()
                            && (pgm_itr->member_type == private_group_member_type::member ||
                            pgm_itr->member_type == private_group_member_type::moder);
                    }
                }
            });

            return gd;
        };

        auto encrypt_key = fc::sha512::hash(groups_cryptor_key);

        auto decrypt = [&](const message_to_decrypt& de, decrypted_result& dr) {
            group_data* gd = nullptr;
            const auto& gd_itr = groups.find(de.group);
            if (gd_itr != groups.end()) {
                gd = &gd_itr->second;
            } else {
                groups[de.group] = load_group(de.group);
                gd = &groups[de.group];
            }
            if (!gd->exists) {
                dr.err = "no_group";
                return;
            }
            if (!gd->can_read) {
                dr.err = "not_member";
                return;
            }

            auto body = _db.with_weak_read_lock([&]() -> std::string {
                std::vector<char> encrypted = de.encrypted_message;
                if (!encrypted.size()) {
                    const auto& idx = _db.get_index<message_index, by_nonce>();
                    auto itr = idx.find(std::make_tuple(de.group, de.from, de.to, de.nonce));
                    if (itr == idx.end()) {
                        return "";
                    }
                    encrypted = std::vector<char>(itr->encrypted_message.begin(), itr->encrypted_message.end());
                }
                // Groups are not using VString (string prefixed by varint32 with length)
                // so just convert it
                std::string res(encrypted.begin(), encrypted.end());
                return res;
            });
            if (!body.size()) {
                dr.err = "no_such_message";
                return;
            }

            fc::variant_object jobj;
            if (!get_encrypted_object(dr, jobj, body, "em")) {
                return;
            }

            auto c_itr = jobj.find("c");
            if (c_itr == jobj.end() || !c_itr->value().is_string()) {
                dr.err = "no field `c` as valid string";
                return;
            }
            body = c_itr->value().as_string();

            decrypt_body(dr, body, encrypt_key, mark_group, de.group);
        };

        for (const auto& de : query.entries) {
            decrypted_result dr;

            decrypt(de, dr);
            res.results.push_back(std::move(dr));
        }

        return res;
    }

    std::string cryptor_key;
    std::string groups_cryptor_key;

    database& _db;
};

cryptor::cryptor() = default;

cryptor::~cryptor() = default;

const std::string& cryptor::name() {
    static std::string name = "cryptor";
    return name;
}

void cryptor::set_program_options(bpo::options_description& cli, bpo::options_description& cfg) {
    cfg.add_options()
        ("cryptor-key", bpo::value<std::string>(), "Key. Recommended length is 16")
        ("groups-cryptor-key", bpo::value<std::string>(), "Key for private messages IN GROUPS. Recommended length is 16");
}

void cryptor::plugin_initialize(const bpo::variables_map &options) {
    ilog("Initializing cryptor plugin");

    my = std::make_unique<cryptor::cryptor_impl>();

    add_plugin_index<crypto_buyer_index>(my->_db);

    my->_db.post_apply_operation.connect([&](const operation_notification& note) {
        my->post_operation(note);
    });

    if (options.count("cryptor-key")) {
        my->cryptor_key = options.at("cryptor-key").as<std::string>();
    }
    if (options.count("groups-cryptor-key")) {
        my->groups_cryptor_key = options.at("groups-cryptor-key").as<std::string>();
    }

    JSON_RPC_REGISTER_API(name())
} 

void cryptor::plugin_startup() {
    ilog("Starting up cryptor plugin");
}

void cryptor::plugin_shutdown() {
    ilog("Shutting down cryptor plugin");
}

DEFINE_API(cryptor, encrypt_body) {
    PLUGIN_API_VALIDATE_ARGS(
        (encrypt_query, query)
    )
    return my->encrypt_body(query);
}

DEFINE_API(cryptor, decrypt_comments) {
    PLUGIN_API_VALIDATE_ARGS(
        (decrypt_query, query)
    )
    return my->decrypt_comments(query);
}

DEFINE_API(cryptor, decrypt_messages) {
    PLUGIN_API_VALIDATE_ARGS(
        (decrypt_messages_query, query)
    )
    return my->decrypt_messages(query);
}

decrypted_api_object cryptor::our_decrypt_messages(const decrypt_messages_query& query) const {
    return my->decrypt_messages(query);
}

} } } // golos::plugins::cryptor
