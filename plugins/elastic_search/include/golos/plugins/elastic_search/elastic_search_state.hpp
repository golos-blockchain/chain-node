#pragma once

#include <golos/protocol/operations.hpp>
#include <golos/chain/database.hpp>
#include <golos/chain/comment_object.hpp>
#include <golos/plugins/social_network/social_network.hpp>
#include <fc/network/http/connection.hpp>
#include <fc/network/ip.hpp>

namespace golos { namespace plugins { namespace elastic_search {

class elastic_search_state_writer {
public:
    using result_type = void;

    database& _db;

    elastic_search_state_writer(database& db)
            : _db(db) {
    }

    template<class T>
    result_type operator()(const T& o) const {

    }

    result_type operator()(const comment_operation& op) const {
        const auto& comment = _db.get_comment(op.author, op.permlink);
        auto id = std::string(op.author) + "." + op.permlink;

        auto* content = appbase::app().get_plugin<golos::plugins::social_network::social_network>().find_comment_content(comment.id);
        if (!content) {
            wlog("Not found comment content for " + id);
            return;
        }

        fc::mutable_variant_object doc;
        doc["published_at"] = comment.created;
        //doc["author"] = comment.author;
        doc["title"] = content->title;
        doc["body"] = content->body;
        std::string str_doc = fc::json::to_string(doc);

        std::string url = "http://127.0.0.1:9200/blog/post/" + id;

        fc::http::connection conn;
        auto ep = fc::ip::endpoint::from_string("127.0.0.1:9200");
        conn.connect_to(ep);
        fc::http::headers headers;
        //headers.emplace_back("Content-Type", "application/json"); // already set - hardcoded
        auto reply = conn.request("PUT", url, str_doc, headers);
        auto body = std::string(reply.body.data(), reply.body.size());
        if (reply.status != fc::http::reply::status_code::OK) {
            wlog(id + ", status: " + std::to_string(reply.status) + ", " + body);
        }
    }
};

} } } // golos::plugins::elastic_search
