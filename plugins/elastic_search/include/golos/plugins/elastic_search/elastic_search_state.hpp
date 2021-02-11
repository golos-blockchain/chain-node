#pragma once

#include <golos/protocol/operations.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/plugins/social_network/social_network.hpp>
#include <golos/plugins/tags/tag_visitor.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/ip.hpp>
#include <fc/network/url.hpp>

namespace golos { namespace plugins { namespace elastic_search {

#define TAGS_NUMBER 15
#define TAG_MAX_LENGTH 512

class elastic_search_state_writer {
public:
    using result_type = void;

    std::string url;
    database& _db;
    fc::http::connection conn;
    std::map<std::string, std::string> buffer;
    int op_num = 0;

    elastic_search_state_writer(const std::string& url, database& db)
            : url(url), _db(db) {
        auto fc_url = fc::url(url);
        auto host_port = *fc_url.host() + (fc_url.port() ? ":" + std::to_string(*fc_url.port()) : "");
        auto ep = fc::ip::endpoint::from_string(host_port);
        conn.connect_to(ep);
    }

    template<class T>
    result_type operator()(const T& o) {

    }

    result_type operator()(const comment_operation& op) {
        auto id = std::string(op.author) + "." + op.permlink;

        fc::mutable_variant_object doc;
        doc["published_at"] = _db.head_block_time();
        doc["author"] = op.author;
        doc["title"] = op.title;
        doc["body"] = op.body;
        doc["tags"] = golos::plugins::tags::get_metadata(op.json_metadata, TAGS_NUMBER, TAG_MAX_LENGTH).tags;
        std::string str_doc = fc::json::to_string(doc);
        buffer[id] = std::move(str_doc);
        ++op_num;

        if (op_num % 10 != 0) {
            return;
        }

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
            bulk += obj.second + "\r\n";
        }
        buffer.clear();

        auto bulk_url = url + "/blog/_bulk";

        fc::http::headers headers;
        //headers.emplace_back("Content-Type", "application/json"); // already set - hardcoded
        auto reply = conn.request("POST", bulk_url, bulk, headers);
        auto body = std::string(reply.body.data(), reply.body.size());
        if (reply.status != fc::http::reply::status_code::OK && reply.status != fc::http::reply::status_code::RecordCreated) {
            wlog(id + ", status: " + std::to_string(reply.status) + ", " + body);
        }
    }
};

} } } // golos::plugins::elastic_search
