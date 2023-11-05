#pragma once

#include <golos/protocol/operations.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/plugins/social_network/social_network.hpp>
#include <golos/plugins/tags/tag_visitor.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/ip.hpp>
#include <fc/network/url.hpp>
#include <boost/locale/encoding_utf.hpp>
#include <diff_match_patch.h>

namespace golos { namespace plugins { namespace elastic_search {

#define TAGS_NUMBER 15
#define TAG_MAX_LENGTH 512

using boost::locale::conv::utf_to_utf;
using golos::plugins::social_network::comment_last_update_index;
using golos::plugins::social_network::by_comment;

#define ELASTIC_WRITE_EACH_N_OP_REPLAYING 100

using es_buffer_type = std::map<std::string, fc::mutable_variant_object>;

class elastic_search_state_writer {
public:
    using result_type = void;

    std::string url;
    std::string login;
    std::string password;
    database& _db;
    uint16_t versions_depth;
    fc::time_point_sec skip_comments_before;
    fc::http::connection conn;
    es_buffer_type buffer;
    es_buffer_type buffer_versions;
    int op_num = 0;

    elastic_search_state_writer(const std::string& url, const std::string& login, const std::string& password, database& db,
            uint16_t versions_depth, fc::time_point_sec skip_comments_before)
            : url(url), login(login), password(password), _db(db),
                versions_depth(versions_depth), skip_comments_before(skip_comments_before) {
        auto fc_url = fc::url(url);
        auto host_port = *fc_url.host() + (fc_url.port() ? ":" + std::to_string(*fc_url.port()) : "");
        auto ep = fc::ip::endpoint::from_string(host_port);
        conn.connect_to(ep);
    }

    template<class T>
    result_type operator()(const T& o) {

    }

    std::string make_id(std::string author, const std::string& permlink) const {
        return author + "." + permlink;
    }

    std::wstring utf8_to_wstring(const std::string& str) const {
        return utf_to_utf<wchar_t>(str.c_str(), str.c_str() + str.size());
    }

    std::string wstring_to_utf8(const std::wstring& str) const {
        return utf_to_utf<char>(str.c_str(), str.c_str() + str.size());
    }

    fc::http::headers get_es_headers() {
        fc::http::headers headers;
        std::string authorization;
        if (login.size()) {
            authorization += login + ":";
        }
        if (password.size()) {
            authorization += password;
        }
        if (authorization.size()) {
            headers.emplace_back("Authorization", "Basic " + fc::base64_encode(authorization));
        }
        return headers;
    }

    bool find_in_es(const std::string& id, variant_object& res) {
        auto headers = get_es_headers();
        auto reply0 = conn.request("GET", url + "/blog/post/" + id + "?pretty", "", headers);
        auto reply_body = std::string(reply0.body.data(), reply0.body.size());
        if (reply0.status == fc::http::reply::status_code::OK) {
            auto reply = fc::json::from_string(reply_body);
            if (reply["found"].as_bool()) {
                res = reply["_source"].get_object();
                return true;
            }
        }
        res = variant_object();
        return false;
    }

    bool find_post(const std::string& id, fc::mutable_variant_object& found, bool& from_buffer) {
        auto found_buf = buffer.find(id);
        variant_object found_es;
        from_buffer = false;
        if (found_buf != buffer.end()) {
            from_buffer = true;
            found = found_buf->second;
            return true;
        } else if (find_in_es(id, found_es)) {
            found = fc::mutable_variant_object(found_es);
            return true;
        }
        return false;
    }

    void save_version(const comment_operation& op, const comment_object& cmt, const fc::mutable_variant_object* prevPtr, fc::time_point_sec now) {
        if (_db.has_index<comment_last_update_index>()) {
            const auto& clu_idx = _db.get_index<comment_last_update_index, by_comment>();
            auto clu_itr = clu_idx.find(cmt.id);
            if (clu_itr != clu_idx.end() && clu_itr->num_changes > 0 && clu_itr->num_changes <= versions_depth) {
                auto id = make_id(op.author, op.permlink);
                auto v = clu_itr->num_changes;
                auto vid = id + "," + std::to_string(v);

                fc::mutable_variant_object prev_doc;
                bool from_buffer;
                if (prevPtr == nullptr || !prevPtr->size()) {
                    bool found = find_post(id, prev_doc, from_buffer);
                    if (found) {
                        prevPtr = &prev_doc;
                    } else { // post not exists because of some mistake in elastic node maintenance
                        prevPtr = nullptr;
                    }
                }

                if (prevPtr) {
                    fc::mutable_variant_object doc;
                    const auto& prev = *prevPtr;
                    auto itr = prev.find("body_patch");
                    if (itr != prev.end()) {
                        doc["is_patch"] = true;
                        doc["body"] = itr->value();
                    } else {
                        doc["body"] = prev["body"];
                    }
                    auto lu_itr = prev.find("last_update");
                    if (lu_itr != prev.end()) {
                        doc["time"] = lu_itr->value();
                    } else {
                        doc["time"] = cmt.created; 
                    }
                    doc["v"] = v;
                    doc["post"] = id;

                    buffer_versions[vid] = std::move(doc);
                }
            }
        } else {
            wlog("no comment_last_update_index (no social_network plugin or comment-last-update-depth in config is 0), so we will not save comment/post versions");
        }
    }

    bool just_created(const comment_object& cmt) {
        if (_db.has_index<comment_last_update_index>()) {
            const auto& clu_idx = _db.get_index<comment_last_update_index, by_comment>();
            auto clu_itr = clu_idx.find(cmt.id);
            if (clu_itr != clu_idx.end()) {
                return !clu_itr->num_changes;
            }
        }
        return false;
    }

    result_type operator()(const comment_operation& op) {
        auto id = make_id(op.author, op.permlink);

        if (!op.body.size()) {
            return;
        }

        const auto& cmt = _db.get_comment_by_perm(op.author, op.permlink);

        const auto now = _db.head_block_time();

        fc::mutable_variant_object doc;
        bool from_buffer = false;

        bool has_patch = false;
        std::string body = op.body;
        try {
            diff_match_patch<std::wstring> dmp;
            auto patch = dmp.patch_fromText(utf8_to_wstring(body));
            if (patch.size()) {
                find_post(id, doc, from_buffer);

                std::string base_body;
                if (!just_created(cmt) && doc.size()) {
                    base_body = doc["body"].as_string();
                }
                auto result = dmp.patch_apply(patch, utf8_to_wstring(base_body));
                auto patched_body = wstring_to_utf8(result.first);
                if(!fc::is_utf8(patched_body)) {
                    body = fc::prune_invalid_utf8(patched_body);
                } else {
                    body = patched_body;
                }
                if (now >= skip_comments_before) {
                    has_patch = true;
                }
            }
        } catch ( ... ) {
        }

        if (now >= skip_comments_before) {
            save_version(op, cmt, &doc, now);
        }

        doc["id"] = cmt.id;
        doc["created"] = now;
        doc["author"] = op.author;
        doc["permlink"] = op.permlink;
        doc["parent_author"] = op.parent_author;
        doc["parent_permlink"] = op.parent_permlink;

        if (op.parent_author == STEEMIT_ROOT_POST_PARENT) {
            doc["category"] = op.parent_permlink;
            doc["root_author"] = op.author;
            doc["root_permlink"] = op.permlink;
            doc["root_title"] = op.title;
        } else {
            const auto& root_cmt = _db.get<comment_object, by_id>(cmt.root_comment);
            doc["root_author"] = root_cmt.author;

            const auto* root_extras = _db.find_extras(root_cmt.author, root_cmt.hashlink);
            if (root_extras) {
                doc["category"] = root_extras->parent_permlink;
                doc["root_permlink"] = root_extras->permlink;
            }

            const auto* root_cnt = appbase::app().get_plugin<golos::plugins::social_network::social_network>().find_comment_content(root_cmt.id);
            doc["root_title"] = root_cnt ? to_string(root_cnt->title) : "";
        }
        doc["depth"] = cmt.depth;

        doc["title"] = op.title;
        doc["body"] = body;
        doc["tags"] = golos::plugins::tags::get_metadata(op.json_metadata, TAGS_NUMBER, TAG_MAX_LENGTH).tags;
        doc["json_metadata"] = op.json_metadata;

        doc["author_reputation"] = std::string(_db.get_account_reputation(op.author));

        if (has_patch) {
            doc["body_patch"] = op.body;
        }
        if (now >= skip_comments_before) {
            doc["last_update"] = now;
        }

        buffer[id] = std::move(doc);

        write_buffers();
    }

    result_type operator()(const comment_reward_operation& op) {
// #ifndef STEEMIT_BUILD_TESTNET
//         if (_db.head_block_num() < 35000000) { // Speed up replay
//             return;
//         }
// #endif
        const auto& cmt = _db.get_comment(op.author, op.hashlink);
        const auto* extras = _db.find_extras(op.author, op.hashlink);

        if (!extras) {
            return;
        }

        auto id = make_id(op.author, to_string(extras->permlink));

        fc::mutable_variant_object doc;
        bool from_buffer = false;
        bool exists = find_post(id, doc, from_buffer);

        if (!exists) {
            return;
        }

        doc["net_votes"] = cmt.net_votes;
        doc["net_rshares"] = cmt.net_rshares;
        doc["children"] = cmt.children;

        doc["author_reputation"] = std::string(_db.get_account_reputation(op.author));

        buffer[id] = std::move(doc);

        write_buffers();
    }

    result_type operator()(const donate_operation& op) {
        try {
            auto author_str = op.memo.target["author"].as_string();
            auto permlink = op.memo.target["permlink"].as_string();

            if (!is_valid_account_name(author_str)) return;
            auto author = account_name_type(author_str);

            const auto* comment = _db.find_comment_by_perm(author, permlink);
            if (comment) {
                auto id = make_id(author_str, permlink);

                fc::mutable_variant_object doc;
                bool from_buffer = false;
                find_post(id, doc, from_buffer);

                if (op.amount.symbol == STEEM_SYMBOL) {
                    auto donates = op.amount;
                    auto itr = doc.find("donates");
                    if (itr != doc.end()) {
                        donates += itr->value().as<asset>();
                    }
                    doc["donates"] = donates;
                } else {
                    auto donates_uia = (op.amount.amount / op.amount.precision());
                    auto itr = doc.find("donates_uia");
                    if (itr != doc.end()) {
                        donates_uia += itr->value().as<share_type>();
                    }
                    doc["donates_uia"] = donates_uia;
                }

                doc["author_reputation"] = std::string(_db.get_account_reputation(author_str));

                buffer[id] = std::move(doc);
            }
        } catch (...) {}

        write_buffers();
    }

    void _write_buffer(const std::string& _index, const std::string& _type, es_buffer_type& buf, bool auto_increment = false) {
        std::string bulk;
        for (auto& obj : buf) {
            fc::mutable_variant_object idx;
            idx["_index"] = _index;
            idx["_type"] = _type;
            if (!auto_increment) {
                idx["_id"] = obj.first;
            }
            fc::mutable_variant_object idx2;
            idx2["index"] = idx;
            bulk += fc::json::to_string(idx2) + "\r\n";
            bulk += fc::json::to_string(obj.second) + "\r\n";
        }
        buf.clear();

        if (bulk.empty()) {
            return;
        }

        auto bulk_url = url + "/" + _index + "/_bulk";

        auto headers = get_es_headers();
        //headers.emplace_back("Content-Type", "application/json"); // already set - hardcoded
        auto reply = conn.request("POST", bulk_url, bulk, headers);
        auto reply_body = std::string(reply.body.data(), reply.body.size());
        if (reply.status != fc::http::reply::status_code::OK && reply.status != fc::http::reply::status_code::RecordCreated) {
            wlog("status: " + std::to_string(reply.status) + ", " + reply_body);
        }
    }

    void write_buffers() {
        ++op_num;
        if (_db.is_reindexing() && op_num % ELASTIC_WRITE_EACH_N_OP_REPLAYING != 0) {
            return;
        }

        op_num = 0;

        _write_buffer("blog", "post", buffer);
        _write_buffer("blog_versions", "version", buffer_versions);
    }
};

} } } // golos::plugins::elastic_search
