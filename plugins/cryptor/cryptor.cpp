#include <fc/crypto/aes.hpp>
#include <fc/crypto/base64.hpp>

#include <golos/plugins/chain/plugin.hpp>
#include <golos/plugins/json_rpc/api_helper.hpp>
#include <golos/plugins/cryptor/cryptor_queries.hpp>
#include <golos/plugins/cryptor/cryptor.hpp>
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
        if (!!donate || op.amount.symbol != STEEM_SYMBOL || op.to != donate->author) {
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

    void post_operation(const operation_notification& note) const {
        try {
            note.op.visit(post_operation_visitor(_db));
        } catch (const fc::assert_exception&) {
            if (_db.is_producing()) {
                throw;
            }
        }
    }

    encrypted_api_object encrypt_body(const encrypt_query& query) {
        encrypted_api_object res;
        res.status = cryptor_status::err;
        res.error = "unknown";

        try {
            if (cryptor_key.size() < 16) {
                res.status = cryptor_status::err;
                res.error = "unavailable_no_key_in_config";
                return res;
            }

            auto encrypt_key = fc::sha512::hash(cryptor_key);

            auto body_data = std::vector<char>(query.body.begin(), query.body.end());
            auto result = fc::aes_encrypt(encrypt_key, body_data);
            res.encrypted = fc::base64_encode((unsigned char const *)&result[0], result.size());

            res.status = cryptor_status::ok;
            res.error = "";
        } FC_CAPTURE_AND_LOG((query));

        return res;
    }

    decrypted_api_object login(const decrypt_query& query) {
        decrypted_api_object res;
        res.status = cryptor_status::ok;

        auto head = _db.with_weak_read_lock([&]() {
            return _db.head_block_num();
        });
        if (head < query.signed_data.head_block_number) {
            res.status = cryptor_status::err;
            res.error = "head_block_num_in_future";
            return res;
        }
        if (head - query.signed_data.head_block_number > 10) {
            res.status = cryptor_status::err;
            res.error = "head_block_num_too_old";
            return res;
        }

        auto exists = _db.with_weak_read_lock([&]() {
            return _db.find_account(query.account) != nullptr;
        });
        if (!exists) {
            res.status = cryptor_status::err;
            res.error = "account_not_exists";
            return res;
        }

        auto sig_hash = fc::sha256::hash(fc::to_string(query.signed_data.head_block_number));

        fc::ecc::public_key key;
        try {
            key = fc::ecc::public_key(query.signature, sig_hash);
        } catch (...) {
            res.status = cryptor_status::err;
            res.error = "illformed_signature";
            return res;
        }
        auto pk = public_key_type(key);

        auto acc_keys = _db.with_weak_read_lock([&]() {
            return _db.get_authority(query.account).posting.get_keys();
        });
        if (!acc_keys.size() || std::find(acc_keys.begin(), acc_keys.end(), pk) == acc_keys.end()) {
            res.status = cryptor_status::err;
            res.error = "wrong_signature";
            return res;
        }

        return res;
    }

    decrypted_api_object decrypt_comments(const decrypt_query& query) {
        decrypted_api_object res;

        struct decrypt_entry {
            std::string author;
            std::string permlink;
            hashlink_type hashlink;
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

        auto decrypt = [&](const decrypt_entry& de) {
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

            fc::variant jvar;
            try {
                jvar = fc::json::from_string(body);
            } catch (...) {
                wlog("wrong_json_or_not_encrypted");
            }
            auto jobj = jvar.get_object();
            auto spec_type = jobj["t"].as_string();
            if (spec_type != "e") {
                wlog("not_encrypted");
            }
            auto version = jobj["v"].as_uint64();
            if (version < 2) {
                wlog("wrong_encrypt_version");
            }
            body = jobj["c"].as_string();
            body = fc::base64_decode(body);

            auto body_data = std::vector<char>(body.begin(), body.end());
            auto result = fc::aes_decrypt(encrypt_key, body_data);
            auto result_str = std::string(result.begin(), result.end());
            return result_str;
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

            dr.body = decrypt(de);
            res.results.push_back(std::move(dr));
        }

        for (size_t i = 0; i < failed.size(); ++i) {
            const auto* de = failed[i];
            auto f = failed_res[i];
            auto& dr = res.results[f];

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

                const auto& cb_idx = _db.get_index<crypto_buyer_index, by_comment_donater>();
                auto cb_itr = cb_idx.find(std::make_tuple(extras->id, query.account));
                if (cb_itr != cb_idx.end() && cb_itr->buyed) {
                    return true;
                }
                return false;
            });

            if (donated) {
                dr.err = "";
                dr.body = decrypt(*de);
            }
        }

        return res;
    }

    std::string cryptor_key;

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
        ("cryptor-key", bpo::value<std::string>(), "Key. Recommended length is 16");
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

} } } // golos::plugins::cryptor
