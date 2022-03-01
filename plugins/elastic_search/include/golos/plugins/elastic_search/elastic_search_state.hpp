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

#ifdef STEEMIT_BUILD_TESTNET
    #define ELASTIC_WRITE_EACH_N_OP 3
#else
    #define ELASTIC_WRITE_EACH_N_OP 100
#endif

class elastic_search_state_writer {
public:
    using result_type = void;

    std::string url;
    std::string login;
    std::string password;
    database& _db;
    fc::http::connection conn;
    std::map<std::string, fc::mutable_variant_object> buffer;
    int op_num = 0;

    elastic_search_state_writer(const std::string& url, const std::string& login, const std::string& password, database& db)
            : url(url), login(login), password(password), _db(db) {
        auto fc_url = fc::url(url);
        auto host_port = *fc_url.host() + (fc_url.port() ? ":" + std::to_string(*fc_url.port()) : "");
        auto ep = fc::ip::endpoint::from_string(host_port);
        conn.connect_to(ep);
    }

    template<class T>
    result_type operator()(const T& o) {

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

    result_type operator()(const comment_operation& op) {
        auto id = std::string(op.author) + "." + op.permlink;

        if (!op.body.size()) {
            return;
        }

        fc::mutable_variant_object doc;
        bool from_buffer = false;

        std::string body = op.body;
        try {
            diff_match_patch<std::wstring> dmp;
            auto patch = dmp.patch_fromText(utf8_to_wstring(body));
            if (patch.size()) {
                find_post(id, doc, from_buffer);
                std::string base_body = doc["body"].as_string();
                if (base_body.size()) {
                    auto result = dmp.patch_apply(patch, utf8_to_wstring(base_body));
                    auto patched_body = wstring_to_utf8(result.first);
                    if(!fc::is_utf8(patched_body)) {
                        body = fc::prune_invalid_utf8(patched_body);
                    } else {
                        body = patched_body;
                    }
                }
            }
        } catch ( ... ) {
        }

        const auto& cmt = _db.get_comment(op.author, op.permlink);

        doc["id"] = cmt.id;
        doc["created"] = _db.head_block_time();
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
            doc["category"] = root_cmt.parent_permlink;
            doc["root_author"] = root_cmt.author;
            auto root_permlink = to_string(root_cmt.permlink);
            doc["root_permlink"] = root_permlink;
            const auto* root_cnt = appbase::app().get_plugin<golos::plugins::social_network::social_network>().find_comment_content(root_cmt.id);
            doc["root_title"] = root_cnt ? to_string(root_cnt->title) : "";
        }
        doc["depth"] = cmt.depth;

        doc["title"] = op.title;
        doc["body"] = body;
        doc["tags"] = golos::plugins::tags::get_metadata(op.json_metadata, TAGS_NUMBER, TAG_MAX_LENGTH).tags;
        doc["json_metadata"] = op.json_metadata;

        doc["author_reputation"] = std::string(_db.get_account_reputation(op.author));

        buffer[id] = std::move(doc);

        ++op_num;
        if (op_num % ELASTIC_WRITE_EACH_N_OP != 0) {
            return;
        }

        write_buffer();
    }

    result_type operator()(const comment_reward_operation& op) {
#ifndef STEEMIT_BUILD_TESTNET
        if (_db.head_block_num() < 35000000) { // Speed up replay
            return;
        }
#endif

        auto id = std::string(op.author) + "." + op.permlink;

        fc::mutable_variant_object doc;
        bool from_buffer = false;
        bool exists = find_post(id, doc, from_buffer);

        if (!exists) {
            return;
        }

        const auto& cmt = _db.get_comment(op.author, op.permlink);

        doc["net_votes"] = cmt.net_votes;
        doc["net_rshares"] = cmt.net_rshares;
        doc["children"] = cmt.children;

        doc["author_reputation"] = std::string(_db.get_account_reputation(op.author));

        buffer[id] = std::move(doc);

        ++op_num;
        if (op_num % ELASTIC_WRITE_EACH_N_OP != 0) {
            return;
        }

        write_buffer();
    }

    result_type operator()(const donate_operation& op) {
        try {
            auto author_str = op.memo.target["author"].as_string();
            auto permlink = op.memo.target["permlink"].as_string();

            if (!is_valid_account_name(author_str)) return;
            auto author = account_name_type(author_str);

            const auto* comment = _db.find_comment(author, permlink);
            if (comment) {
                auto id = author_str + "." + permlink;

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

        ++op_num;
        if (op_num % ELASTIC_WRITE_EACH_N_OP != 0) {
            return;
        }

        write_buffer();
    }

    void write_buffer() {
        op_num = 0;

        std::string bulk;
        for (auto& obj : buffer) {
            fc::mutable_variant_object idx;
            idx["_index"] = "blog";
            idx["_type"] = "post";
            idx["_id"] = obj.first;
            fc::mutable_variant_object idx2;
            idx2["index"] = idx;
            bulk += fc::json::to_string(idx2) + "\r\n";
            bulk += fc::json::to_string(obj.second) + "\r\n";
        }
        buffer.clear();

        auto bulk_url = url + "/blog/_bulk";

        auto headers = get_es_headers();
        //headers.emplace_back("Content-Type", "application/json"); // already set - hardcoded
        auto reply = conn.request("POST", bulk_url, bulk, headers);
        auto reply_body = std::string(reply.body.data(), reply.body.size());
        if (reply.status != fc::http::reply::status_code::OK && reply.status != fc::http::reply::status_code::RecordCreated) {
            wlog("status: " + std::to_string(reply.status) + ", " + reply_body);
        }
    }
};

} } } // golos::plugins::elastic_search
