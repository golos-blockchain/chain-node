#pragma once

#include <golos/protocol/asset.hpp>
#include <golos/protocol/authority.hpp>
#include <golos/protocol/base.hpp>

namespace golos { namespace protocol {

    struct paid_subscription_id {
        account_name_type app;
        account_name_type name;
        uint16_t version = 1;

        friend bool operator<(const paid_subscription_id& a, const paid_subscription_id& b) {
            if (a.app == b.app) {
                if (a.name == b.name) {
                    return a.version < b.version;
                }
                return a.name < b.name;
            } 
            return a.app < b.app;
        }

        bool operator==(const paid_subscription_id& o) const {
            return (app == o.app) &&
                   (name == o.name) &&
                   (version == o.version);
        }

        void validate() const;
    };

    struct paid_subscription_create_operation : public base_operation {
        account_name_type author;
        paid_subscription_id oid;
        asset cost;
        bool tip_cost = false;
        bool allow_prepaid = true;
        uint32_t interval = 0; // seconds 
        uint32_t executions = 0; // how many times it should be repeated. Can be 0, some number, or UINT32_MAX (4294967295) for infinity

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(author);
        }
    };

    struct paid_subscription_update_operation : public base_operation {
        account_name_type author;
        paid_subscription_id oid;
        asset cost;
        bool tip_cost = false;
        uint32_t interval = 0; // seconds 
        uint32_t executions = 0; // how many times it should be repeated. Can be 0, some number, or UINT32_MAX (4294967295) for infinity

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(author);
        }
    };

    struct paid_subscription_delete_operation : public base_operation {
        account_name_type author;
        paid_subscription_id oid;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(author);
        }
    };

    struct paid_subscription_transfer_operation : public base_operation {
        account_name_type from;
        account_name_type to;
        paid_subscription_id oid;
        asset amount;
        std::string memo;
        bool from_tip = false;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            if (from_tip)
                a.insert(from);
        }

        void get_required_active_authorities(flat_set<account_name_type>& a) const {
            if (!from_tip)
                a.insert(from);
        }
    };

    struct paid_subscription_cancel_operation : public base_operation {
        account_name_type subscriber;
        account_name_type author;
        paid_subscription_id oid;

        extensions_type extensions;

        void validate() const;

        void get_required_posting_authorities(flat_set<account_name_type>& a) const {
            a.insert(subscriber);
        }
    };

} } // golos::protocol

FC_REFLECT(
    (golos::protocol::paid_subscription_id),
    (app)(name)(version))

FC_REFLECT(
    (golos::protocol::paid_subscription_create_operation),
    (author)(oid)(cost)(tip_cost)(allow_prepaid)(interval)(executions)(extensions))

FC_REFLECT(
    (golos::protocol::paid_subscription_update_operation),
    (author)(oid)(cost)(tip_cost)(interval)(executions)(extensions))

FC_REFLECT(
    (golos::protocol::paid_subscription_delete_operation),
    (author)(oid)(extensions))

FC_REFLECT(
    (golos::protocol::paid_subscription_transfer_operation),
    (from)(to)(oid)(amount)(memo)(from_tip)(extensions))

FC_REFLECT(
    (golos::protocol::paid_subscription_cancel_operation),
    (subscriber)(author)(oid)(extensions))
